/*
** Command & Conquer Generals Zero Hour(tm)
**
** Reusable backend-neutral material pass for explicit programmable shaders.
*/

#pragma once

#include <array>

#include "Backend/RenderBackend.h"
#include "ProgrammableShader.h"
#include "WWMath/vector4.h"

class ProgrammableMaterialPass
{
public:
	ProgrammableMaterialPass();
	~ProgrammableMaterialPass();

	ProgrammableMaterialPass(const ProgrammableMaterialPass &) = delete;
	ProgrammableMaterialPass &operator=(const ProgrammableMaterialPass &) = delete;

	void Set_Program(ShaderProgramClass *program) { m_program = program; }
	void Set_Texture(unsigned stage, TextureBaseClass *texture);
	void Set_Vertex_Constants(unsigned reg, const Vector4 *values, unsigned count);
	void Set_Pixel_Constants(unsigned reg, const Vector4 *values, unsigned count);

	bool Apply();
	void Reset();

private:
	ShaderProgramClass *m_program;
	std::array<TextureBaseClass *, MAX_PROGRAMMABLE_TEXTURE_STAGES> m_textures;
	std::array<Vector4, MAX_VERTEX_SHADER_CONSTANTS> m_vertex_constants;
	std::array<Vector4, MAX_PIXEL_SHADER_CONSTANTS> m_pixel_constants;
};
