/* DX11 shader subsystem. */
#include "Backend/dx11/shaders/DX11ShaderBackend.h"
#include "Backend/dx11/core/DX11BackendInternals.h"
#include "Backend/dx11/core/DX11BackendRuntime.h"

namespace dx11_backend
{
bool DX11BackendState::Recreate_Compiled_Shaders()
{
	if (device == nullptr)
	{
		return false;
	}

	for (DX11VertexShader *shader : vertex_shaders)
	{
		if (shader == nullptr || shader->shader != nullptr)
		{
			continue;
		}
		if (shader->bytecode.empty() || FAILED(device->CreateVertexShader(
			shader->bytecode.data(), shader->bytecode.size(), nullptr, &shader->shader)))
		{
			return false;
		}
	}
	for (DX11PixelShader *shader : pixel_shaders)
	{
		if (shader == nullptr || shader->shader != nullptr)
		{
			continue;
		}
		if (shader->bytecode.empty() || FAILED(device->CreatePixelShader(
			shader->bytecode.data(), shader->bytecode.size(), nullptr, &shader->shader)))
		{
			return false;
		}
	}
	default_vertex_shaders.fill(nullptr);
	default_pixel_shaders.fill(nullptr);
	for (DX11VertexShader *shader : vertex_shaders)
	{
		if (shader != nullptr && shader->precompiled_default &&
			shader->default_format != RenderBackendVertexFormat::Unknown)
		{
			const unsigned format = static_cast<unsigned>(shader->default_format);
			if (format < default_vertex_shaders.size())
			{
				default_vertex_shaders[format] = shader;
			}
		}
	}
	for (DX11PixelShader *shader : pixel_shaders)
	{
		if (shader != nullptr && shader->precompiled_default)
		{
			const unsigned variant = Default_Pixel_Shader_Variant(
				shader->default_texture_kind, shader->default_texture_stage);
			if (variant < default_pixel_shaders.size())
			{
				default_pixel_shaders[variant] = shader;
			}
		}
	}
	return true;
}

bool DX11BackendState::Create_Input_Layout(DX11VertexShader * shader,
	const std::vector<DX11VertexInput> & inputs, ID3D11InputLayout **layout_result)
{
	if (device == nullptr || shader == nullptr || inputs.empty() ||
		shader->bytecode.empty() || layout_result == nullptr)
	{
		return false;
	}

	std::vector<D3D11_INPUT_ELEMENT_DESC> descriptions;
	descriptions.reserve(inputs.size());
	for (const DX11VertexInput & input : inputs)
	{
		D3D11_INPUT_ELEMENT_DESC description = {};
		description.SemanticName = Semantic_Name(input.element.semantic);
		description.SemanticIndex = input.element.semantic_index;
		description.Format = Vertex_Native_Format(input.element.type);
		description.InputSlot = input.element.stream;
		description.AlignedByteOffset = input.element.offset;
		description.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		description.InstanceDataStepRate = 0;
		if (description.SemanticName != nullptr)
		{
			descriptions.push_back(description);
		}
	}

	ID3D11InputLayout *new_layout = nullptr;
	const HRESULT result = device->CreateInputLayout(descriptions.data(),
		static_cast<UINT>(descriptions.size()), shader->bytecode.data(),
		shader->bytecode.size(), &new_layout);
	if (FAILED(result))
	{
		return false;
	}
	*layout_result = new_layout;
	return true;
}

DX11VertexShader *DX11BackendState::Find_Default_Vertex_Shader()
{
	const unsigned format = static_cast<unsigned>(current_layout.format);
	if (format == 0 || format >= default_vertex_shaders.size())
	{
		return nullptr;
	}
	if (default_vertex_shaders[format] != nullptr)
	{
		return default_vertex_shaders[format];
	}

	const std::vector<DX11VertexInput> inputs = Make_Vertex_Input(current_layout);
	for (DX11VertexShader *shader : vertex_shaders)
	{
		if (shader->explicit_layout || shader->precompiled_default ||
			shader->inputs.size() != inputs.size())
		{
			continue;
		}
		bool same = true;
		for (unsigned index = 0; index < inputs.size(); ++index)
		{
			const auto &left = shader->inputs[index].element;
			const auto &right = inputs[index].element;
			if (left.offset != right.offset || left.type != right.type ||
				left.semantic != right.semantic || left.semantic_index != right.semantic_index)
			{
				same = false;
				break;
			}
		}
		if (same)
		{
			return shader;
		}
	}

	const std::string filename = "shaders/standard_vertex_" + std::to_string(format) + ".vso";
	std::vector<unsigned char> bytecode;
	if (!Load_Precompiled_DXBC(filename.c_str(), bytecode))
	{
		return nullptr;
	}
	DX11VertexShader *shader = new DX11VertexShader();
	shader->bytecode = std::move(bytecode);
	shader->inputs = inputs;
	shader->precompiled_default = true;
	shader->default_format = current_layout.format;
	const HRESULT result = device->CreateVertexShader(shader->bytecode.data(),
		shader->bytecode.size(), nullptr, &shader->shader);
	if (FAILED(result))
	{
		delete shader;
		return nullptr;
	}
	vertex_shaders.push_back(shader);
	default_vertex_shaders[format] = shader;
	return shader;
}

DX11PixelShader *DX11BackendState::Find_Default_Pixel_Shader()
{
	RenderBackendTextureKind special_texture_kind = RenderBackendTextureKind::Texture2D;
	unsigned special_texture_stage = 0;
	bool has_special_texture = false;
	for (unsigned stage = 0; stage < MAX_TEXTURE_STAGES; ++stage)
	{
		const RenderBackendTextureKind kind = texture_kinds[stage];
		if (kind != RenderBackendTextureKind::Cube &&
			kind != RenderBackendTextureKind::Volume)
		{
			continue;
		}
		if (has_special_texture)
		{
			return nullptr;
		}
		has_special_texture = true;
		special_texture_kind = kind;
		special_texture_stage = stage;
	}

	const unsigned variant = Default_Pixel_Shader_Variant(
		special_texture_kind, special_texture_stage);
	if (default_pixel_shaders[variant] != nullptr)
	{
		return default_pixel_shaders[variant];
	}

	std::vector<unsigned char> bytecode;
	std::string filename = "shaders/standard.pso";
	if (special_texture_kind == RenderBackendTextureKind::Cube)
	{
		filename = "shaders/standard_cube_" + std::to_string(special_texture_stage) + ".pso";
	}
	else if (special_texture_kind == RenderBackendTextureKind::Volume)
	{
		filename = "shaders/standard_volume_" + std::to_string(special_texture_stage) + ".pso";
	}
	if (!Load_Precompiled_DXBC(filename.c_str(), bytecode))
	{
		return nullptr;
	}
	DX11PixelShader *shader = new DX11PixelShader();
	shader->bytecode = std::move(bytecode);
	shader->precompiled_default = true;
	shader->default_texture_kind = special_texture_kind;
	shader->default_texture_stage = special_texture_stage;
	const HRESULT result = device->CreatePixelShader(shader->bytecode.data(),
		shader->bytecode.size(), nullptr, &shader->shader);
	if (FAILED(result))
	{
		delete shader;
		return nullptr;
	}
	pixel_shaders.push_back(shader);
	default_pixel_shaders[variant] = shader;
	return shader;
}

bool DX11BackendState::Ensure_Default_Pipeline()
{
	if (device == nullptr || context == nullptr)
	{
		return false;
	}
	if (active_vertex_shader == nullptr)
	{
		active_vertex_shader = Find_Default_Vertex_Shader();
		has_explicit_layout = false;
		active_explicit_layout = nullptr;
	}
	if (active_pixel_shader == nullptr)
	{
		active_pixel_shader = Find_Default_Pixel_Shader();
	}
	if (active_vertex_shader == nullptr || active_pixel_shader == nullptr)
	{
		return false;
	}

	std::vector<DX11VertexInput> explicit_inputs;
	const std::vector<DX11VertexInput> *inputs = &active_vertex_shader->inputs;
	if (has_explicit_layout && active_explicit_layout != nullptr)
	{
		explicit_inputs = Make_Vertex_Input(*active_explicit_layout);
		inputs = &explicit_inputs;
	}
	if (input_layout == nullptr)
	{
		const unsigned format = static_cast<unsigned>(current_layout.format);
		const bool cache_default_layout = !has_explicit_layout &&
			!active_vertex_shader->explicit_layout &&
			active_vertex_shader->precompiled_default &&
			format < default_input_layouts.size();
		if (cache_default_layout)
		{
			if (default_input_layouts[format] == nullptr &&
				!Create_Input_Layout(active_vertex_shader, *inputs,
					&default_input_layouts[format]))
			{
				return false;
			}
			input_layout = default_input_layouts[format];
			input_layout_owned = false;
		}
		else
		{
			if (!Create_Input_Layout(active_vertex_shader, *inputs, &input_layout))
			{
				return false;
			}
			input_layout_owned = true;
		}
	}
	if (!shader_bindings_valid || bound_input_layout != input_layout ||
		bound_vertex_shader != active_vertex_shader ||
		bound_pixel_shader != active_pixel_shader)
	{
		context->IASetInputLayout(input_layout);
		context->VSSetShader(active_vertex_shader->shader, nullptr, 0);
		context->PSSetShader(active_pixel_shader->shader, nullptr, 0);
		bound_input_layout = input_layout;
		bound_vertex_shader = active_vertex_shader;
		bound_pixel_shader = active_pixel_shader;
		shader_bindings_valid = true;
	}
	return true;
}
template <typename Host>
void DX11ShaderBackend<Host>::Set_Shader(const ShaderClass & shader)
{
	this->State().current_shader = shader;
	this->State().render_state.shader_bits = shader.Get_Bits();
}

template <typename Host>
void DX11ShaderBackend<Host>::Get_Shader(ShaderClass & shader)
{
	shader = this->State().current_shader;
}

template <typename Host>
void DX11ShaderBackend<Host>::Set_Vertex_Shader(uintptr_t shader, const RenderBackendVertexShaderInputLayout * input_layout)
{
	this->State().shader_bindings_valid = false;
	this->State().active_vertex_shader = reinterpret_cast<DX11VertexShader *>(shader);
	this->State().active_explicit_layout = nullptr;
	this->State().has_explicit_layout = input_layout != nullptr;
	if (input_layout != nullptr)
	{
		this->State().explicit_layout_storage = *input_layout;
		this->State().active_explicit_layout = &this->State().explicit_layout_storage;
	}
	this->State().Release_Active_Input_Layout();
}

template <typename Host>
void DX11ShaderBackend<Host>::Set_Pixel_Shader(uintptr_t shader)
{
	this->State().shader_bindings_valid = false;
	this->State().active_pixel_shader = reinterpret_cast<DX11PixelShader *>(shader);
}

template <typename Host>
bool DX11ShaderBackend<Host>::Create_Pixel_Shader(const void * bytecode, unsigned bytecode_size, uintptr_t * shader)
{
	if (bytecode == nullptr || bytecode_size == 0 || shader == nullptr || this->State().device == nullptr)
	{
		return false;
	}

	uint32_t version = 0;
	if (bytecode_size >= sizeof(version))
	{
		std::memcpy(&version, bytecode, sizeof(version));
	}
	if ((version >> 16) == 0xffffu && ((version >> 8) & 0xffu) == 1u)
	{
		// DX11 consumes only build-time Slang DXBC. Legacy token streams belong
		// to the temporary DX9 backend and are deliberately rejected here.
		return false;
	}

	if (bytecode_size < 4 || version != 0x43425844u) // 'DXBC'
	{
		return false;
	}

	DX11PixelShader *pixel_shader = new DX11PixelShader();
	pixel_shader->bytecode.assign(static_cast<const unsigned char *>(bytecode),
		static_cast<const unsigned char *>(bytecode) + bytecode_size);
	if (FAILED(this->State().device->CreatePixelShader(pixel_shader->bytecode.data(),
		pixel_shader->bytecode.size(), nullptr, &pixel_shader->shader)))
	{
		delete pixel_shader;
		return false;
	}
	this->State().pixel_shaders.push_back(pixel_shader);
	*shader = reinterpret_cast<uintptr_t>(pixel_shader);
	return true;
}

template <typename Host>
bool DX11ShaderBackend<Host>::Create_Vertex_Shader(const void * bytecode, unsigned bytecode_size, uintptr_t * shader, const RenderBackendVertexShaderInputLayout * input_layout)
{
	if (bytecode == nullptr || bytecode_size == 0 || shader == nullptr || this->State().device == nullptr)
	{
		return false;
	}
	const std::vector<DX11VertexInput> inputs = input_layout != nullptr ?
		Make_Vertex_Input(*input_layout) : Make_Vertex_Input(this->State().current_layout);
	if (inputs.empty())
	{
		return false;
	}
	uint32_t version = 0;
	if (bytecode_size >= sizeof(version))
	{
		std::memcpy(&version, bytecode, sizeof(version));
	}
	if ((version >> 16) == 0xfffeu && ((version >> 8) & 0xffu) == 1u)
	{
		// DX11 consumes only build-time Slang DXBC. Legacy token streams belong
		// to the temporary DX9 backend and are deliberately rejected here.
		return false;
	}

	if (bytecode_size < 4 || version != 0x43425844u) // 'DXBC'
	{
		return false;
	}

	DX11VertexShader *vertex_shader = new DX11VertexShader();
	vertex_shader->bytecode.assign(static_cast<const unsigned char *>(bytecode),
		static_cast<const unsigned char *>(bytecode) + bytecode_size);
	vertex_shader->inputs = inputs;
	vertex_shader->explicit_layout = input_layout != nullptr;
	if (FAILED(this->State().device->CreateVertexShader(vertex_shader->bytecode.data(),
		vertex_shader->bytecode.size(), nullptr, &vertex_shader->shader)))
	{
		delete vertex_shader;
		return false;
	}
	this->State().vertex_shaders.push_back(vertex_shader);
	*shader = reinterpret_cast<uintptr_t>(vertex_shader);
	return true;
}

template <typename Host>
void DX11ShaderBackend<Host>::Release_Vertex_Shader(uintptr_t shader)
{
	DX11VertexShader *vertex_shader = reinterpret_cast<DX11VertexShader *>(shader);
	if (vertex_shader == nullptr)
	{
		return;
	}
	if (this->State().active_vertex_shader == vertex_shader)
	{
		this->State().active_vertex_shader = nullptr;
		this->State().shader_bindings_valid = false;
	}
	if (this->State().bound_vertex_shader == vertex_shader)
	{
		this->State().shader_bindings_valid = false;
	}
	auto iterator = std::find(this->State().vertex_shaders.begin(),
		this->State().vertex_shaders.end(), vertex_shader);
	if (iterator != this->State().vertex_shaders.end())
	{
		this->State().vertex_shaders.erase(iterator);
		delete vertex_shader;
	}
}

template <typename Host>
void DX11ShaderBackend<Host>::Release_Pixel_Shader(uintptr_t shader)
{
	DX11PixelShader *pixel_shader = reinterpret_cast<DX11PixelShader *>(shader);
	if (pixel_shader == nullptr)
	{
		return;
	}
	if (this->State().active_pixel_shader == pixel_shader)
	{
		this->State().active_pixel_shader = nullptr;
		this->State().shader_bindings_valid = false;
	}
	if (this->State().bound_pixel_shader == pixel_shader)
	{
		this->State().shader_bindings_valid = false;
	}
	auto iterator = std::find(this->State().pixel_shaders.begin(),
		this->State().pixel_shaders.end(), pixel_shader);
	if (iterator != this->State().pixel_shaders.end())
	{
		this->State().pixel_shaders.erase(iterator);
		delete pixel_shader;
	}
}

template <typename Host>
void DX11ShaderBackend<Host>::Set_Vertex_Shader_Constant(unsigned reg, const void * data, unsigned count)
{
	if (data == nullptr || reg >= MAX_VERTEX_SHADER_CONSTANTS)
	{
		return;
	}
	const unsigned copy_count = std::min(count, MAX_VERTEX_SHADER_CONSTANTS - reg);
	std::memcpy(this->State().vertex_constants.data() + reg, data,
		copy_count * sizeof(Vector4));
	this->State().Mark_Constant_State_Dirty();
}

template <typename Host>
void DX11ShaderBackend<Host>::Set_Pixel_Shader_Constant(unsigned reg, const void * data, unsigned count)
{
	if (data == nullptr || reg >= MAX_PIXEL_SHADER_CONSTANTS)
	{
		return;
	}
	const unsigned copy_count = std::min(count, MAX_PIXEL_SHADER_CONSTANTS - reg);
	std::memcpy(this->State().pixel_constants.data() + reg, data,
		copy_count * sizeof(Vector4));
	this->State().Mark_Constant_State_Dirty();
}

template <typename Host>
void DX11ShaderBackend<Host>::Set_Texture(unsigned stage, TextureBaseClass * texture)
{
	if (stage < MAX_TEXTURE_STAGES)
	{
		REF_PTR_SET(this->State().render_state.Textures[stage], texture);
		// A normal WW3D texture assignment owns this stage again. The resource
		// itself will be resolved by Apply_Render_State_Changes().
		this->State().direct_texture_overrides[stage] = nullptr;
		this->State().direct_texture_override_valid[stage] = false;
	}
}

template <typename Host>
void DX11ShaderBackend<Host>::Set_Texture_Resource(unsigned stage, const TextureBaseClass * texture)
{
	if (stage >= MAX_TEXTURE_STAGES)
	{
		return;
	}
	DX11Texture *resource = nullptr;
	RenderBackendTextureKind kind = RenderBackendTextureKind::Texture2D;
	if (texture != nullptr)
	{
		TextureBaseClass *mutable_texture = const_cast<TextureBaseClass *>(texture);
		if (mutable_texture->As_CubeTextureClass() != nullptr)
		{
			kind = RenderBackendTextureKind::Cube;
		}
		else if (mutable_texture->As_VolumeTextureClass() != nullptr)
		{
			kind = RenderBackendTextureKind::Volume;
		}
		const bool ensured = mutable_texture->Ensure_Render_Backend_Texture();
		if (ensured)
		{
			resource = As_DX11_Texture(mutable_texture->Peek_Render_Backend_Texture());
			if (resource != nullptr)
			{
				kind = resource->kind;
			}
		}
	}
	const bool resource_changed = this->State().textures[stage] != resource ||
		this->State().texture_kinds[stage] != kind;
	if (this->State().texture_kinds[stage] != kind)
	{
		this->State().texture_kinds[stage] = kind;
		this->State().Invalidate_Default_Pixel_Shader_Selection();
	}
	this->State().textures[stage] = resource;
	if (!this->State().applying_render_state)
	{
		const bool override_changed = this->State().direct_texture_override_valid[stage] !=
			(resource != nullptr) ||
			(this->State().direct_texture_override_valid[stage] &&
				this->State().direct_texture_overrides[stage] != resource);
		this->State().direct_texture_overrides[stage] = resource;
		this->State().direct_texture_override_valid[stage] = resource != nullptr;
		if (resource_changed || override_changed)
		{
			this->State().Mark_All_State_Dirty();
		}
	}
	else if (resource_changed)
	{
		this->State().Mark_All_State_Dirty();
	}
}

template <typename Host>
void DX11ShaderBackend<Host>::Set_Programmable_Texture_Resource(
	unsigned stage, const TextureBaseClass *texture)
{
	if (stage != MAX_TEXTURE_STAGES)
	{
		return;
	}

	DX11Texture *resource = nullptr;
	if (texture != nullptr)
	{
		TextureBaseClass *mutable_texture = const_cast<TextureBaseClass *>(texture);
		if (mutable_texture->Ensure_Render_Backend_Texture())
		{
			resource = As_DX11_Texture(
				mutable_texture->Peek_Render_Backend_Texture());
		}
	}
	this->State().programmable_texture = resource;
	this->State().Mark_All_State_Dirty();
	if (resource == nullptr && this->State().context != nullptr)
	{
		ID3D11ShaderResourceView *null_view = nullptr;
		this->State().context->PSSetShaderResources(MAX_TEXTURE_STAGES, 1,
			&null_view);
	}
}

template <typename Host>
void DX11ShaderBackend<Host>::Set_Texture_Handle(unsigned stage, uintptr_t texture)
{
	if (stage < MAX_TEXTURE_STAGES)
	{
		DX11Texture *resource = reinterpret_cast<DX11Texture *>(texture);
		const RenderBackendTextureKind kind = resource == nullptr ?
			RenderBackendTextureKind::Texture2D : resource->kind;
		const bool resource_changed = this->State().textures[stage] != resource ||
			this->State().texture_kinds[stage] != kind;
		if (this->State().texture_kinds[stage] != kind)
		{
			this->State().texture_kinds[stage] = kind;
			this->State().Invalidate_Default_Pixel_Shader_Selection();
		}
		this->State().textures[stage] = resource;
		if (!this->State().applying_render_state)
		{
			const bool override_changed = this->State().direct_texture_override_valid[stage] !=
				(resource != nullptr) ||
				(this->State().direct_texture_override_valid[stage] &&
					this->State().direct_texture_overrides[stage] != resource);
			this->State().direct_texture_overrides[stage] = resource;
			this->State().direct_texture_override_valid[stage] = resource != nullptr;
			if (resource_changed || override_changed)
			{
				this->State().Mark_All_State_Dirty();
			}
		}
		else if (resource_changed)
		{
			this->State().Mark_All_State_Dirty();
		}
	}
}

template class DX11ShaderBackend<DX11BackendRuntime>;
}
