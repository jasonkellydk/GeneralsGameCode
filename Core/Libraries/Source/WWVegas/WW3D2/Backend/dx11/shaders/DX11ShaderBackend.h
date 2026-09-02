#pragma once

#include "../core/DX11BackendComponent.h"

namespace dx11_backend
{
	template <typename Host>
	class DX11ShaderBackend;

	template <typename Host>
	class DX11ShaderBackend : public DX11BackendComponent<Host, DX11ShaderBackend<Host>>
	{
	public:
		void Set_Shader(const ShaderClass &shader);
		void Get_Shader(ShaderClass &shader);
		void Set_Vertex_Shader(uintptr_t shader, const RenderBackendVertexShaderInputLayout *input_layout = nullptr);
		void Set_Pixel_Shader(uintptr_t shader);
		bool Create_Pixel_Shader(const void *bytecode, unsigned bytecode_size, uintptr_t *shader);
		bool Create_Vertex_Shader(const void *bytecode, unsigned bytecode_size, uintptr_t *shader, const RenderBackendVertexShaderInputLayout *input_layout = nullptr);
		void Release_Vertex_Shader(uintptr_t shader);
		void Release_Pixel_Shader(uintptr_t shader);
		void Set_Vertex_Shader_Constant(unsigned reg, const void *data, unsigned count);
		void Set_Pixel_Shader_Constant(unsigned reg, const void *data, unsigned count);
		void Set_Texture(unsigned stage, TextureBaseClass *texture);
		void Set_Texture_Resource(unsigned stage, const TextureBaseClass *texture);
		void Set_Programmable_Texture_Resource(unsigned stage,
			const TextureBaseClass *texture);
		void Set_Texture_Handle(unsigned stage, uintptr_t texture);
	};
}
