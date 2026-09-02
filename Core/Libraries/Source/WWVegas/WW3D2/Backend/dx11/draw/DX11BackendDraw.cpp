/* DX11 draw subsystem. */
#include "Backend/dx11/draw/DX11DrawBackend.h"
#include "Backend/dx11/core/DX11BackendInternals.h"
#include "Backend/dx11/core/DX11BackendRuntime.h"

namespace dx11_backend
{

template <typename Host>
void DX11DrawBackend<Host>::Set_Vertex_Buffer(RenderBackendVertexBuffer * buffer, unsigned offset_bytes, unsigned stride_bytes, unsigned stream)
{
	if (stream >= MAX_VERTEX_STREAMS)
	{
		return;
	}
	DX11VertexBuffer *vertex_buffer = static_cast<DX11VertexBuffer *>(buffer);
	if (stream == 0 && vertex_buffer != nullptr)
	{
		this->State().Set_Current_Vertex_Layout(vertex_buffer->layout);
	}
	if (stream == 0 && !this->State().applying_render_state_buffers)
	{
		// This overload is the explicit native-buffer binding used by effects
		// such as shadow volumes. Its DrawIndexed base is already expressed by
		// the caller and must not inherit the deferred engine buffer offset.
		this->State().direct_vertex_binding_override = true;
	}
	this->State().vertex_buffers[stream] = vertex_buffer;
	this->State().vertex_offsets[stream] = vertex_buffer == nullptr ? 0 : offset_bytes;
	this->State().vertex_strides[stream] = vertex_buffer == nullptr ? 0 : stride_bytes;
	if (this->State().context != nullptr)
	{
		ID3D11Buffer *native_buffer = vertex_buffer == nullptr ? nullptr : vertex_buffer->resource;
		const UINT native_offset = vertex_buffer == nullptr ? 0 : offset_bytes;
		const UINT native_stride = vertex_buffer == nullptr ? 0 : stride_bytes;
		this->State().context->IASetVertexBuffers(stream, 1, &native_buffer,
			&native_stride, &native_offset);
	}
}

template <typename Host>
void DX11DrawBackend<Host>::Set_Index_Buffer(RenderBackendIndexBuffer * buffer)
{
	DX11IndexBuffer *index_buffer = static_cast<DX11IndexBuffer *>(buffer);
	if (!this->State().applying_render_state_buffers)
	{
		// The explicit low-level binding starts at index zero. The draw call's
		// start index is therefore not combined with the deferred IBA offset.
		this->State().direct_index_binding_override = true;
	}
	this->State().index_buffer = index_buffer;
	if (this->State().context != nullptr)
	{
		this->State().context->IASetIndexBuffer(
			index_buffer == nullptr ? nullptr : index_buffer->resource,
			DXGI_FORMAT_R16_UINT, 0);
	}
}

template <typename Host>
void DX11DrawBackend<Host>::Set_Vertex_Format(RenderBackendVertexFormat format)
{
	this->State().Set_Current_Vertex_Layout(RenderBackend_Vertex_Layout(format));
}

template <typename Host>
bool DX11DrawBackend<Host>::Process_Vertices(VertexBufferClass * destination, unsigned vertex_count)
{
	return this->State().process_vertices.Process(this->State(), destination, vertex_count);
}

template <typename Host>
void DX11DrawBackend<Host>::Draw_Indexed_Primitives(RenderBackendPrimitiveType primitive_type, unsigned base_vertex_index, unsigned min_vertex_index, unsigned vertex_count, unsigned start_index, unsigned primitive_count)
{
	(void)min_vertex_index;
	(void)vertex_count;
	if (!this->State().triangle_draw_enabled || this->State().context == nullptr ||
		this->State().index_buffer == nullptr || primitive_count == 0)
	{
		return;
	}
	if (this->State().programmable_pass_active)
	{
		this->Backend().Apply_Programmable_Render_State_Changes();
	}
	else
	{
		this->Backend().Apply_Render_State_Changes();
	}
	if (!this->State().Ensure_Default_Pipeline())
	{
		return;
	}
	this->State().context->IASetPrimitiveTopology(To_D3D_Topology(primitive_type));
	const unsigned index_count = Primitive_Index_Count(primitive_type, primitive_count);
	const unsigned draw_start_index = start_index +
		(this->State().direct_index_binding_override ? 0 : this->State().index_offset);
	const unsigned draw_base_vertex = base_vertex_index +
		(this->State().direct_vertex_binding_override ? 0 : this->State().base_vertex_offset);
	this->State().context->DrawIndexed(index_count, draw_start_index, draw_base_vertex);
}

template <typename Host>
void DX11DrawBackend<Host>::Draw_Primitive_Up(RenderBackendPrimitiveType primitive_type, unsigned primitive_count, const void * vertices, unsigned stride_bytes, RenderBackendVertexFormat format)
{
	if (vertices == nullptr || stride_bytes == 0 || primitive_count == 0 ||
		this->State().device == nullptr)
	{
		return;
	}
	const unsigned vertex_count = Primitive_Vertex_Count(primitive_type, primitive_count);
	const std::size_t required_size = static_cast<std::size_t>(vertex_count) * stride_bytes;
	if (required_size == 0 || required_size > std::numeric_limits<unsigned>::max())
	{
		return;
	}

	DX11BackendState &impl = this->State();
	if (impl.immediate_vertex_buffer == nullptr ||
		static_cast<unsigned>(required_size) > impl.immediate_vertex_capacity_bytes)
	{
		unsigned capacity = 4096;
		const unsigned required = static_cast<unsigned>(required_size);
		while (capacity < required && capacity <=
			std::numeric_limits<unsigned>::max() / 2u)
		{
			capacity *= 2u;
		}
		capacity = std::max(capacity, required);
		if (impl.vertex_buffers[0] == impl.immediate_vertex_buffer)
		{
		this->Backend().Set_Vertex_Buffer(static_cast<RenderBackendVertexBuffer *>(nullptr), 0, 0, 0);
		}
		delete impl.immediate_vertex_buffer;
		impl.immediate_vertex_buffer = static_cast<DX11VertexBuffer *>(
			this->Backend().Create_Vertex_Buffer(capacity, RenderBackend_Vertex_Layout(format),
				BUFFER_USAGE_DYNAMIC));
		impl.immediate_vertex_capacity_bytes = impl.immediate_vertex_buffer == nullptr ?
			0 : capacity;
	}
	if (impl.immediate_vertex_buffer == nullptr)
	{
		return;
	}
	impl.immediate_vertex_buffer->layout = RenderBackend_Vertex_Layout(format);

	void *destination = nullptr;
	if (this->Backend().Lock_Vertex_Buffer(impl.immediate_vertex_buffer, 0,
		static_cast<unsigned>(required_size), &destination,
		RenderBackendBufferLockMode::Normal))
	{
		std::memcpy(destination, vertices, required_size);
		this->Backend().Unlock_Vertex_Buffer(impl.immediate_vertex_buffer);
		// Apply the deferred W3D state before overriding the IA vertex stream for
		// this immediate draw. Applying it afterwards would restore the regular
		// render-state vertex buffer over the immediate one.
		if (impl.programmable_pass_active)
		{
			this->Backend().Apply_Programmable_Render_State_Changes();
		}
		else
		{
			this->Backend().Apply_Render_State_Changes();
		}
		Set_Vertex_Buffer(impl.immediate_vertex_buffer, 0, stride_bytes, 0);
		Set_Vertex_Format(format);
		if (this->State().Ensure_Default_Pipeline())
		{
			this->State().Apply_D3D_States();
			this->State().context->IASetPrimitiveTopology(To_D3D_Topology(primitive_type));
			this->State().context->Draw(vertex_count, 0);
		}
	}
	Set_Vertex_Buffer(static_cast<RenderBackendVertexBuffer *>(nullptr), 0, 0, 0);
	impl.direct_vertex_binding_override = false;
	impl.direct_index_binding_override = false;
}

template <typename Host>
void DX11DrawBackend<Host>::Draw_Primitive(RenderBackendPrimitiveType primitive_type, unsigned start_vertex, unsigned primitive_count)
{
	if (this->State().context == nullptr || primitive_count == 0)
	{
		return;
	}
	if (this->State().programmable_pass_active)
	{
		this->Backend().Apply_Programmable_Render_State_Changes();
	}
	else
	{
		this->Backend().Apply_Render_State_Changes();
	}
	if (!this->State().Ensure_Default_Pipeline())
	{
		return;
	}
	this->State().context->IASetPrimitiveTopology(To_D3D_Topology(primitive_type));
	this->State().context->Draw(Primitive_Vertex_Count(primitive_type, primitive_count), start_vertex);
	this->State().direct_vertex_binding_override = false;
	this->State().direct_index_binding_override = false;
}

template <typename Host>
void DX11DrawBackend<Host>::Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned stream)
{
	if (stream >= MAX_VERTEX_STREAMS)
	{
		return;
	}
	if (stream == 0)
	{
		this->State().render_state.vba_offset = 0;
		this->State().render_state.vba_count = 0;
		this->State().Set_Current_Vertex_Layout(vb == nullptr ?
			RenderBackend_Vertex_Layout(RenderBackend_Dynamic_Vertex_Format) : vb->Get_Format_Layout());
	}
	REF_PTR_SET(this->State().render_state.vertex_buffers[stream],
		const_cast<VertexBufferClass *>(vb));
	this->State().render_state.vertex_buffer_types[stream] = vb == nullptr ?
		BUFFER_TYPE_INVALID : vb->Type();
	this->State().applying_render_state_buffers = true;
	Set_Vertex_Buffer(vb == nullptr ? nullptr : vb->Get_Backend_Buffer(),
		0, vb == nullptr ? 0 : vb->Get_Vertex_Size(), stream);
	this->State().applying_render_state_buffers = false;
	if (stream == 0)
	{
		this->State().direct_vertex_binding_override = false;
	}
}

template <typename Host>
void DX11DrawBackend<Host>::Set_Vertex_Buffer(const DynamicVBAccessClass & vba)
{
	for (unsigned stream = 1; stream < MAX_VERTEX_STREAMS; ++stream)
	{
		Set_Vertex_Buffer(static_cast<RenderBackendVertexBuffer *>(nullptr), 0, 0, stream);
		REF_PTR_RELEASE(this->State().render_state.vertex_buffers[stream]);
		this->State().render_state.vertex_buffer_types[stream] = BUFFER_TYPE_INVALID;
	}
	VertexBufferClass *vertex_buffer = vba.Get_Vertex_Buffer();
	REF_PTR_SET(this->State().render_state.vertex_buffers[0], vertex_buffer);
	this->State().render_state.vertex_buffer_types[0] = vba.Get_Type();
	this->State().render_state.vba_offset = vba.Get_Vertex_Buffer_Offset();
	this->State().render_state.vba_count = vba.Get_Vertex_Count();
	this->State().Set_Current_Vertex_Layout(vba.Get_Format_Info().Get_Layout());
	const unsigned stride = vba.Get_Format_Info().Get_Vertex_Size();
	// Match the DX9 dynamic-buffer contract.  The access object's vertex
	// offset is supplied to DrawIndexed as the base vertex; the stream itself
	// is bound at byte offset zero.  Binding the stream at vba_offset as well
	// would apply that offset twice on D3D11.
	this->State().applying_render_state_buffers = true;
	Set_Vertex_Buffer(vertex_buffer == nullptr ? nullptr : vertex_buffer->Get_Backend_Buffer(),
		0, stride, 0);
	this->State().applying_render_state_buffers = false;
	this->State().direct_vertex_binding_override = false;
}

template <typename Host>
void DX11DrawBackend<Host>::Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset)
{
	REF_PTR_SET(this->State().render_state.index_buffer, const_cast<IndexBufferClass *>(ib));
	this->State().render_state.index_buffer_type = ib == nullptr ?
		BUFFER_TYPE_INVALID : ib->Type();
	this->State().render_state.iba_offset = 0;
	this->State().render_state.index_base_offset = index_base_offset;
	this->State().index_offset = 0;
	this->State().base_vertex_offset = index_base_offset +
		this->State().render_state.vba_offset;
	this->State().applying_render_state_buffers = true;
	Set_Index_Buffer(ib == nullptr ? nullptr : ib->Get_Backend_Buffer());
	this->State().applying_render_state_buffers = false;
	this->State().direct_index_binding_override = false;
}

template <typename Host>
void DX11DrawBackend<Host>::Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset)
{
	IndexBufferClass *index_buffer = iba.Get_Index_Buffer();
	REF_PTR_SET(this->State().render_state.index_buffer, index_buffer);
	this->State().render_state.index_buffer_type = iba.Get_Type();
	this->State().render_state.iba_offset = iba.Get_Index_Buffer_Offset();
	this->State().render_state.index_base_offset = index_base_offset;
	this->State().index_offset = iba.Get_Index_Buffer_Offset();
	this->State().base_vertex_offset = index_base_offset +
		this->State().render_state.vba_offset;
	this->State().applying_render_state_buffers = true;
	Set_Index_Buffer(index_buffer == nullptr ? nullptr : index_buffer->Get_Backend_Buffer());
	this->State().applying_render_state_buffers = false;
	this->State().direct_index_binding_override = false;
}

template <typename Host>
void DX11DrawBackend<Host>::Set_Index_Buffer_Index_Offset(unsigned offset)
{
	this->State().render_state.index_base_offset = static_cast<unsigned short>(offset);
	this->State().base_vertex_offset = offset + this->State().render_state.vba_offset;
}

template <typename Host>
void DX11DrawBackend<Host>::Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix, float znear, float zfar)
{
	(void)znear;
	(void)zfar;
	Set_Transform(RenderBackendTransform::Projection, matrix);
}

template <typename Host>
void DX11DrawBackend<Host>::Set_Transform(RenderBackendTransform transform, const Matrix4x4 & matrix)
{
	const unsigned index = static_cast<unsigned>(transform);
	if (index >= std::size(this->State().transforms))
	{
		return;
	}
	this->State().transforms[index] = matrix;
	this->State().Mark_Constant_State_Dirty();
	if (transform == RenderBackendTransform::World)
	{
		this->State().render_state.world = matrix;
	}
	else if (transform == RenderBackendTransform::View)
	{
		this->State().render_state.view = matrix;
	}
}

template <typename Host>
void DX11DrawBackend<Host>::Set_Transform(RenderBackendTransform transform, const Matrix3D & matrix)
{
	Set_Transform(transform, Matrix4x4(matrix));
}

template <typename Host>
void DX11DrawBackend<Host>::Get_Transform(RenderBackendTransform transform, Matrix4x4 & matrix)
{
	const unsigned index = static_cast<unsigned>(transform);
	if (index < std::size(this->State().transforms))
	{
		matrix = this->State().transforms[index];
	}
}

template <typename Host>
void DX11DrawBackend<Host>::Set_Transform(RenderBackendTransform transform, const float * matrix_elements)
{
	if (matrix_elements == nullptr)
	{
		return;
	}
	Matrix4x4 matrix;
	std::memcpy(&matrix, matrix_elements, sizeof(matrix));
	Set_Transform(transform, matrix);
}

template <typename Host>
void DX11DrawBackend<Host>::Get_Transform(RenderBackendTransform transform, float * matrix_elements)
{
	if (matrix_elements == nullptr)
	{
		return;
	}
	Matrix4x4 matrix;
	Get_Transform(transform, matrix);
	std::memcpy(matrix_elements, &matrix, sizeof(matrix));
}

template <typename Host>
void DX11DrawBackend<Host>::Set_World_Identity()
{
	Matrix4x4 identity(true);
	Set_Transform(RenderBackendTransform::World, identity);
}

template <typename Host>
void DX11DrawBackend<Host>::Set_View_Identity()
{
	Matrix4x4 identity(true);
	Set_Transform(RenderBackendTransform::View, identity);
}

template <typename Host>
bool DX11DrawBackend<Host>::Is_World_Identity()
{
	return this->State().transforms[
		static_cast<unsigned>(RenderBackendTransform::World)] == Matrix4x4(true);
}

template <typename Host>
bool DX11DrawBackend<Host>::Is_View_Identity()
{
	return this->State().transforms[
		static_cast<unsigned>(RenderBackendTransform::View)] == Matrix4x4(true);
}

template class DX11DrawBackend<DX11BackendRuntime>;
}
