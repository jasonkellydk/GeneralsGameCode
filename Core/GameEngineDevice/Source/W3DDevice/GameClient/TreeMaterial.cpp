/* Explicit programmable tree material. */

#include "W3DDevice/GameClient/TreeMaterial.h"

#include <cstddef>

#include "WW3D2/Texture.h"
#include "WW3D2/VertexFormat.h"
#include "WW3D2/WW3D.h"

namespace
{
	RenderBackendVertexShaderInputLayout Make_Tree_Vertex_Layout()
	{
		RenderBackendVertexShaderInputLayout layout;
		layout.Add(0, offsetof(VertexFormatXYZNDUV1, x),
			RenderBackendVertexInputType::Float3,
			RenderBackendVertexInputSemantic::Position, 0, 0);
		layout.Add(0, offsetof(VertexFormatXYZNDUV1, nx),
			RenderBackendVertexInputType::Float3,
			RenderBackendVertexInputSemantic::Normal, 0, 1);
		layout.Add(0, offsetof(VertexFormatXYZNDUV1, diffuse),
			RenderBackendVertexInputType::Color,
			RenderBackendVertexInputSemantic::Color, 0, 2);
		layout.Add(0, offsetof(VertexFormatXYZNDUV1, u1),
			RenderBackendVertexInputType::Float2,
			RenderBackendVertexInputSemantic::TextureCoordinate, 0, 7);
		return layout;
	}
}

TreeMaterialClass::TreeMaterialClass()
{
	m_pass.Set_Program(&m_program);
}

TreeMaterialClass::~TreeMaterialClass()
{
	Shutdown();
}

bool TreeMaterialClass::Initialize()
{
	if (m_program.Is_Initialized())
	{
		return true;
	}

	m_program.Shutdown();
	return m_program.Initialize("shaders/Trees.vso", "shaders/Trees.pso",
		Make_Tree_Vertex_Layout());
}

bool TreeMaterialClass::Apply(TextureBaseClass *tree_texture,
	TextureBaseClass *shroud_texture,
	const TreeMaterialParameters &parameters)
{
	if (tree_texture == nullptr || !Initialize())
	{
		return false;
	}

	m_pass.Set_Program(&m_program);
	m_pass.Set_Texture(0, tree_texture);
	m_pass.Set_Texture(1, shroud_texture);

	Vector4 vertex_constants[TREE_MATERIAL_MAX_SWAY_TYPES + 1];
	for (unsigned index = 0; index < TREE_MATERIAL_MAX_SWAY_TYPES; ++index)
	{
		vertex_constants[index] = parameters.sway[index];
	}
	vertex_constants[TREE_MATERIAL_MAX_SWAY_TYPES] =
		parameters.shroud_projection;
	m_pass.Set_Vertex_Constants(0, vertex_constants,
		TREE_MATERIAL_MAX_SWAY_TYPES + 1);

	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend == nullptr)
	{
		return false;
	}

	const bool shroud_ready = parameters.options.X > 0.5f &&
		shroud_texture != nullptr &&
		shroud_texture->Ensure_Render_Backend_Texture() &&
		!shroud_texture->Is_Missing_Texture();
	Vector4 pixel_options = parameters.options;
	pixel_options.X = shroud_ready ? 1.0f : 0.0f;
	m_pass.Set_Pixel_Constants(0, &pixel_options, 1);

	// The atlas and the shroud both contain projected/packed data. Explicit
	// sampler state keeps filtering from crossing an atlas or shroud edge.
	for (unsigned stage = 0; stage < 2; ++stage)
	{
		backend->Set_Texture_Address_Mode(stage, true,
			RenderBackendTextureAddressMode::Clamp);
		backend->Set_Texture_Address_Mode(stage, false,
			RenderBackendTextureAddressMode::Clamp);
		backend->Set_Texture_Filter(stage,
			RenderBackendTextureFilterType::Minification,
			RenderBackendTextureFilter::Linear);
		backend->Set_Texture_Filter(stage,
			RenderBackendTextureFilterType::Magnification,
			RenderBackendTextureFilter::Linear);
		backend->Set_Texture_Filter(stage,
			RenderBackendTextureFilterType::MipMap,
			RenderBackendTextureFilter::Linear);
	}

	// Trees are alpha-cutout geometry. The shader performs the cutout itself,
	// so this pass does not depend on the legacy fixed-function alpha test.
	backend->Set_Depth_Test_Enabled(true);
	backend->Set_Depth_Write_Enabled(true);
	backend->Set_Depth_Function(RenderBackendCompareFunction::LessEqual);
	backend->Set_Cull_Mode(RenderBackendCullMode::None);
	backend->Set_Lighting_Enabled(false);
	backend->Set_Color_Write_Mask(RenderBackendColorWriteMask::All);
	backend->Set_Alpha_Blend_Enabled(false);
	backend->Set_Source_Blend_Factor(RenderBackendBlendFactor::One);
	backend->Set_Destination_Blend_Factor(RenderBackendBlendFactor::Zero);
	backend->Set_Alpha_Test_Enabled(false);

	return m_pass.Apply();
}

void TreeMaterialClass::Shutdown()
{
	m_pass.Reset();
	m_program.Shutdown();
}

void TreeMaterialClass::Reset()
{
	m_pass.Reset();
}
