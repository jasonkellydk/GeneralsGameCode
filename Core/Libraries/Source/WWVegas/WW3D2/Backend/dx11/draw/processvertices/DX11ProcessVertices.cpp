/* Backend-private implementation of Process_Vertices. */
#include "Backend/dx11/draw/processvertices/DX11ProcessVertices.h"

#include "Backend/dx11/core/DX11BackendInternals.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace dx11_backend
{
namespace
{
	constexpr unsigned kInvalidOffset = std::numeric_limits<unsigned>::max();
	constexpr unsigned kThreadGroupSize = 64;

	struct DX11ProcessVerticesConstants
	{
		float world[16] = {};
		float view[16] = {};
		float projection[16] = {};
		float viewport[4] = {};
		unsigned source_stride = 0;
		unsigned destination_stride = 0;
		unsigned source_start = 0;
		unsigned vertex_count = 0;
		unsigned source_location_offset = 0;
		unsigned destination_location_offset = 0;
		unsigned source_diffuse_offset = kInvalidOffset;
		unsigned destination_diffuse_offset = kInvalidOffset;
		unsigned source_texture_count = 0;
		unsigned destination_texture_count = 0;
	unsigned padding[2] = {};
	unsigned source_texture_offsets[8][4] = {};
	unsigned destination_texture_offsets[8][4] = {};
	unsigned source_texture_dimensions[8][4] = {};
	unsigned destination_texture_dimensions[8][4] = {};
	};

	static_assert(sizeof(DX11ProcessVerticesConstants) % 16u == 0u);

	void Release_Com(ID3D11ComputeShader *&object)
	{
		if (object != nullptr)
		{
			object->Release();
			object = nullptr;
		}
	}

	void Release_Com(ID3D11Buffer *&object)
	{
		if (object != nullptr)
		{
			object->Release();
			object = nullptr;
		}
	}

	unsigned Safe_Texture_Dimension(const RenderBackendVertexLayout &layout,
		unsigned index)
	{
		return index < layout.texture_count ?
			std::min(4u, std::max(1u, layout.texture_dimensions[index])) : 0u;
	}

	void Copy_Matrix(float (&destination)[16], const Matrix4x4 &source)
	{
		std::memcpy(destination, &source, sizeof(destination));
	}
}

DX11ProcessVertices::~DX11ProcessVertices()
{
	Release();
}

bool DX11ProcessVertices::Initialize(ID3D11Device *device)
{
	Release();
	if (device == nullptr)
	{
		return false;
	}

	std::vector<unsigned char> bytecode;
	if (!Load_Precompiled_DXBC("shaders/process_vertices.cso", bytecode) ||
		FAILED(device->CreateComputeShader(bytecode.data(), bytecode.size(), nullptr,
			&shader)) ||
		!Create_Constant_Buffer(device, sizeof(DX11ProcessVerticesConstants),
			&constant_buffer))
	{
		Release();
		return false;
	}
	return true;
}

void DX11ProcessVertices::Release()
{
	Release_Com(constant_buffer);
	Release_Com(shader);
}

bool DX11ProcessVertices::Process(DX11BackendState &state,
	VertexBufferClass *destination, unsigned vertex_count)
{
	if (state.context == nullptr || shader == nullptr || constant_buffer == nullptr ||
		destination == nullptr || vertex_count == 0 ||
		vertex_count > destination->Get_Vertex_Count() ||
		!destination->Get_Format_Layout().transformed ||
		state.vertex_buffers[0] == nullptr || state.vertex_strides[0] == 0)
	{
		return false;
	}

	DX11VertexBuffer *source = state.vertex_buffers[0];
	DX11VertexBuffer *output = static_cast<DX11VertexBuffer *>(
		destination->Get_Backend_Buffer());
	if (source == nullptr || output == nullptr || source == output ||
		source->resource == nullptr || output->resource == nullptr ||
		source->process_shader_resource_view == nullptr ||
		output->process_unordered_access_view == nullptr)
	{
		return false;
	}

	const RenderBackendVertexLayout source_layout = state.current_layout;
	const RenderBackendVertexLayout destination_layout = destination->Get_Format_Layout();
	const VertexFormatInfoClass source_info(source_layout);
	const VertexFormatInfoClass destination_info(destination_layout);
	const unsigned source_stride = state.vertex_strides[0];
	const unsigned destination_stride = destination_info.Get_Vertex_Size();
	const unsigned source_start = state.vertex_offsets[0];
	if (destination_stride == 0 || source_stride < source_info.Get_Vertex_Size() ||
		source_start > source->size ||
		vertex_count > (source->size - source_start) / source_stride ||
		vertex_count > output->size / destination_stride)
	{
		return false;
	}

	DX11ProcessVerticesConstants constants;
	Copy_Matrix(constants.world,
		state.transforms[static_cast<unsigned>(RenderBackendTransform::World)]);
	Copy_Matrix(constants.view,
		state.transforms[static_cast<unsigned>(RenderBackendTransform::View)]);
	Copy_Matrix(constants.projection,
		state.transforms[static_cast<unsigned>(RenderBackendTransform::Projection)]);
	constants.viewport[0] = static_cast<float>(state.viewport.x);
	constants.viewport[1] = static_cast<float>(state.viewport.y);
	constants.viewport[2] = state.viewport.width > 0 ?
		static_cast<float>(state.viewport.width) : 1.0f;
	constants.viewport[3] = state.viewport.height > 0 ?
		static_cast<float>(state.viewport.height) : 1.0f;
	constants.source_stride = source_stride;
	constants.destination_stride = destination_stride;
	constants.source_start = source_start;
	constants.vertex_count = vertex_count;
	constants.source_location_offset = source_info.Get_Location_Offset();
	constants.destination_location_offset = destination_info.Get_Location_Offset();
	constants.source_diffuse_offset = source_layout.has_diffuse ?
		source_info.Get_Diffuse_Offset() : kInvalidOffset;
	constants.destination_diffuse_offset = destination_layout.has_diffuse ?
		destination_info.Get_Diffuse_Offset() : kInvalidOffset;
	constants.source_texture_count = std::min(source_layout.texture_count, 8u);
	constants.destination_texture_count = std::min(destination_layout.texture_count, 8u);
	for (unsigned index = 0; index < 8; ++index)
	{
		constants.source_texture_offsets[index][0] = index < constants.source_texture_count ?
			source_info.Get_Tex_Offset(index) : 0;
		constants.destination_texture_offsets[index][0] = index < constants.destination_texture_count ?
			destination_info.Get_Tex_Offset(index) : 0;
		constants.source_texture_dimensions[index][0] = Safe_Texture_Dimension(source_layout, index);
		constants.destination_texture_dimensions[index][0] = Safe_Texture_Dimension(destination_layout, index);
	}

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (FAILED(state.context->Map(constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0,
		&mapped)))
	{
		return false;
	}
	std::memcpy(mapped.pData, &constants, sizeof(constants));
	state.context->Unmap(constant_buffer, 0);

	ID3D11ShaderResourceView *source_view = source->process_shader_resource_view;
	ID3D11UnorderedAccessView *output_view = output->process_unordered_access_view;
	state.context->CSSetShader(shader, nullptr, 0);
	state.context->CSSetConstantBuffers(0, 1, &constant_buffer);
	state.context->CSSetShaderResources(0, 1, &source_view);
	state.context->CSSetUnorderedAccessViews(0, 1, &output_view, nullptr);
	state.context->Dispatch((vertex_count + kThreadGroupSize - 1u) /
		kThreadGroupSize, 1, 1);

	ID3D11ShaderResourceView *null_shader_resource = nullptr;
	ID3D11UnorderedAccessView *null_unordered_access = nullptr;
	ID3D11Buffer *null_constant_buffer = nullptr;
	state.context->CSSetShaderResources(0, 1, &null_shader_resource);
	state.context->CSSetUnorderedAccessViews(0, 1, &null_unordered_access, nullptr);
	state.context->CSSetConstantBuffers(0, 1, &null_constant_buffer);
	state.context->CSSetShader(nullptr, nullptr, 0);
	return true;
}
}
