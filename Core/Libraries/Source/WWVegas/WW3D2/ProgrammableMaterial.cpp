/* Backend-neutral programmable material pass. */

#include "ProgrammableMaterial.h"

#include <algorithm>
#include <cstring>

#include "WW3D2/Texture.h"
#include "WW3D2/WW3D.h"

ProgrammableMaterialPass::ProgrammableMaterialPass() : m_program(nullptr)
{
	m_textures.fill(nullptr);
	m_vertex_constants.fill(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
	m_pixel_constants.fill(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
}

ProgrammableMaterialPass::~ProgrammableMaterialPass()
{
	Reset();
}

void ProgrammableMaterialPass::Set_Texture(unsigned stage, TextureBaseClass *texture)
{
	if (stage >= MAX_PROGRAMMABLE_TEXTURE_STAGES || m_textures[stage] == texture)
	{
		return;
	}
	REF_PTR_SET(m_textures[stage], texture);
}

void ProgrammableMaterialPass::Set_Vertex_Constants(unsigned reg,
	const Vector4 *values, unsigned count)
{
	if (values == nullptr || reg >= MAX_VERTEX_SHADER_CONSTANTS)
	{
		return;
	}
	const unsigned copy_count = std::min(count, MAX_VERTEX_SHADER_CONSTANTS - reg);
	std::memcpy(m_vertex_constants.data() + reg, values,
		copy_count * sizeof(Vector4));
}

void ProgrammableMaterialPass::Set_Pixel_Constants(unsigned reg,
	const Vector4 *values, unsigned count)
{
	if (values == nullptr || reg >= MAX_PIXEL_SHADER_CONSTANTS)
	{
		return;
	}
	const unsigned copy_count = std::min(count, MAX_PIXEL_SHADER_CONSTANTS - reg);
	std::memcpy(m_pixel_constants.data() + reg, values,
		copy_count * sizeof(Vector4));
}

bool ProgrammableMaterialPass::Apply()
{
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend == nullptr || m_program == nullptr || !m_program->Is_Initialized())
	{
		return false;
	}

	backend->Begin_Programmable_Pass();
	if (!m_program->Apply())
	{
		backend->End_Programmable_Pass();
		return false;
	}

	for (unsigned stage = 0; stage < MAX_TEXTURE_STAGES; ++stage)
	{
		backend->Set_Texture_Resource(stage, m_textures[stage]);
	}
	for (unsigned stage = MAX_TEXTURE_STAGES;
		stage < MAX_PROGRAMMABLE_TEXTURE_STAGES; ++stage)
	{
		backend->Set_Programmable_Texture_Resource(stage, m_textures[stage]);
	}
	backend->Set_Vertex_Shader_Constant(0, m_vertex_constants.data(),
		MAX_VERTEX_SHADER_CONSTANTS);
	backend->Set_Pixel_Shader_Constant(0, m_pixel_constants.data(),
		MAX_PIXEL_SHADER_CONSTANTS);
	backend->Apply_Programmable_Render_State_Changes();
	return true;
}

void ProgrammableMaterialPass::Reset()
{
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend != nullptr)
	{
		for (unsigned stage = 0; stage < MAX_TEXTURE_STAGES; ++stage)
		{
			backend->Set_Texture_Resource(stage, nullptr);
		}
		for (unsigned stage = MAX_TEXTURE_STAGES;
			stage < MAX_PROGRAMMABLE_TEXTURE_STAGES; ++stage)
		{
			backend->Set_Programmable_Texture_Resource(stage, nullptr);
		}
		if (m_program != nullptr)
		{
			m_program->Reset();
		}
		backend->End_Programmable_Pass();
	}
	for (TextureBaseClass *&texture : m_textures)
	{
		REF_PTR_RELEASE(texture);
	}
}
