/*
** Command & Conquer Generals Zero Hour(tm)
**
** Backend-neutral compiled shader program used by programmable materials.
** The concrete backend owns the objects represented by the opaque handles.
*/

#pragma once

#include "Backend/RenderBackend.h"

class ShaderProgramClass
{
public:
	ShaderProgramClass();
	~ShaderProgramClass();

	ShaderProgramClass(const ShaderProgramClass &) = delete;
	ShaderProgramClass &operator=(const ShaderProgramClass &) = delete;

	bool Initialize(const char *vertex_shader_path, const char *pixel_shader_path,
		const RenderBackendVertexShaderInputLayout &vertex_input_layout);
	void Shutdown();

	bool Apply() const;
	void Reset() const;
	bool Is_Initialized() const
	{
		return m_vertex_shader != 0 && m_pixel_shader != 0;
	}

private:
	IRenderBackend *m_backend;
	uintptr_t m_vertex_shader;
	uintptr_t m_pixel_shader;
	RenderBackendVertexShaderInputLayout m_vertex_input_layout;
};
