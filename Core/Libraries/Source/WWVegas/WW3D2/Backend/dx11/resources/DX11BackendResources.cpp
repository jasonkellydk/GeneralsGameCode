/* DX11 resource subsystem. */
#include "Backend/dx11/resources/DX11ResourceBackend.h"
#include "Backend/dx11/core/DX11BackendInternals.h"
#include "Backend/dx11/core/DX11BackendRuntime.h"

namespace dx11_backend
{
template <typename Host>
TextureClass * DX11ResourceBackend<Host>::Create_Render_Target(int width, int height, WW3DFormat format)
{
	if (format == WW3D_FORMAT_UNKNOWN)
	{
		format = WW3D_FORMAT_A8R8G8B8;
	}
	if (width <= 0 || height <= 0 || !this->Backend().Supports_Render_To_Texture_Format(format))
	{
		return nullptr;
	}
	return new TextureClass(static_cast<unsigned>(width), static_cast<unsigned>(height),
		format, MIP_LEVELS_1, TextureBaseClass::POOL_DEFAULT, true, false);
}

template <typename Host>
TextureClass * DX11ResourceBackend<Host>::Create_Scene_Depth_Texture()
{
	DX11Texture *scene_depth = this->State().scene_depth_texture;
	if (scene_depth == nullptr || scene_depth->resource == nullptr ||
		scene_depth->shader_resource_view == nullptr)
	{
		return nullptr;
	}

	return new TextureClass(reinterpret_cast<RenderBackendTextureHandle>(
		scene_depth));
}

template <typename Host>
bool DX11ResourceBackend<Host>::Capture_Scene_Depth()
{
	DX11BackendState &impl = this->State();
	DX11Texture *scene_depth = impl.scene_depth_texture;
	if (impl.context == nullptr || impl.depth_buffer == nullptr ||
		scene_depth == nullptr || scene_depth->resource == nullptr)
	{
		return false;
	}

	// Copying from the active DSV is valid on DX11, but detaching the DSV for
	// the copy keeps this pass valid with the debug layer and restores the
	// exact active attachments immediately afterward.
	ID3D11RenderTargetView *active_color = impl.active_render_target_view;
	ID3D11DepthStencilView *active_depth = impl.active_depth_stencil_view;
	impl.context->OMSetRenderTargets(1, &active_color, nullptr);
	impl.context->CopyResource(scene_depth->resource, impl.depth_buffer);
	impl.context->OMSetRenderTargets(1, &active_color, active_depth);
	impl.native_state_valid = false;
	impl.Mark_All_State_Dirty();
	return true;
}

template <typename Host>
void DX11ResourceBackend<Host>::Create_Render_Target(int width, int height, WW3DFormat format, WW3DZFormat depth_format, TextureClass ** target, ZTextureClass ** depth_target)
{
	if (target != nullptr)
	{
		*target = nullptr;
	}
	if (depth_target != nullptr)
	{
		*depth_target = nullptr;
	}
	if (target == nullptr || depth_target == nullptr)
	{
		return;
	}

	TextureClass *render_target = Create_Render_Target(width, height, format);
	if (render_target == nullptr)
	{
		return;
	}
	ZTextureClass *depth = nullptr;
	if (depth_format != WW3D_ZFORMAT_UNKNOWN)
	{
		depth = new ZTextureClass(static_cast<unsigned>(width), static_cast<unsigned>(height),
			depth_format, MIP_LEVELS_1, TextureBaseClass::POOL_DEFAULT);
		if (!depth->Is_Initialized())
		{
			REF_PTR_RELEASE(depth);
		}
	}
	*target = render_target;
	*depth_target = depth;
}

template <typename Host>
void DX11ResourceBackend<Host>::Set_Render_Target(TextureBaseClass * render_target, ZTextureClass * depth_target)
{
	DX11BackendState &impl = this->State();
	if (impl.context == nullptr)
	{
		return;
	}

	DX11Texture *color_texture = nullptr;
	DX11Texture *depth_texture = nullptr;
	if (render_target != nullptr)
	{
		if (!render_target->Ensure_Render_Backend_Texture())
		{
			return;
		}
		color_texture = As_DX11_Texture(render_target->Peek_Render_Backend_Texture());
		if (color_texture == nullptr || color_texture->render_target_view == nullptr)
		{
			return;
		}
	}
	if (depth_target != nullptr)
	{
		if (!depth_target->Ensure_Render_Backend_Texture())
		{
			return;
		}
		depth_texture = As_DX11_Texture(depth_target->Peek_Render_Backend_Texture());
		if (depth_texture == nullptr || depth_texture->depth_stencil_view == nullptr)
		{
			return;
		}
	}

	// D3D11 forbids a resource from being simultaneously bound as a pixel
	// shader input and an output-merger target. Reflection and post-process
	// passes commonly reuse the same TextureClass, so explicitly detach all
	// shader-resource views before changing the target.
	ID3D11ShaderResourceView *null_views[MAX_TEXTURE_STAGES] = {};
	impl.context->PSSetShaderResources(0, MAX_TEXTURE_STAGES, null_views);
	impl.native_state_valid = false;
	impl.Mark_All_State_Dirty();
	impl.render_to_texture = color_texture != nullptr;
	impl.active_render_target_view = color_texture != nullptr ?
		color_texture->render_target_view : impl.back_buffer_view;
	impl.active_depth_stencil_view = depth_texture != nullptr ?
		depth_texture->depth_stencil_view :
		color_texture == nullptr ? impl.depth_buffer_view :
		color_texture->render_target_depth_stencil_view;
	impl.active_render_target = render_target;
	impl.active_depth_target = depth_target;
	impl.context->OMSetRenderTargets(1, &impl.active_render_target_view,
		impl.active_depth_stencil_view);
	if (color_texture != nullptr)
	{
		impl.viewport = {0, 0, color_texture->width, color_texture->height, 0.0f, 1.0f};
	}
	else
	{
		impl.viewport = {0, 0, impl.width, impl.height, 0.0f, 1.0f};
	}
	this->Backend().Set_Viewport(impl.viewport);
}

template <typename Host>
RenderBackendSurface * DX11ResourceBackend<Host>::Create_System_Memory_Surface(unsigned width, unsigned height, WW3DFormat format)
{
	if (width == 0 || height == 0 || format == WW3D_FORMAT_UNKNOWN)
	{
		return nullptr;
	}
	return new DX11Surface(width, height, format);
}

template <typename Host>
SurfaceClass * DX11ResourceBackend<Host>::Create_Surface(unsigned width, unsigned height, WW3DFormat format)
{
	RenderBackendSurface *surface = Create_System_Memory_Surface(width, height, format);
	return surface == nullptr ? nullptr : new SurfaceClass(surface);
}

template <typename Host>
RenderBackendSurface * DX11ResourceBackend<Host>::Create_Surface_From_File(const char * filename)
{
	if (filename == nullptr || filename[0] == '\0')
	{
		return Create_Missing_Surface();
	}
	StringClass name(filename, true);
	return TextureLoader::Load_Surface_Immediate(name, WW3D_FORMAT_UNKNOWN, true);
}

template <typename Host>
bool DX11ResourceBackend<Host>::Get_Surface_Description(RenderBackendSurface * surface, RenderBackendSurfaceDescription & description) const
{
	const DX11Surface *dx11_surface = As_DX11_Surface(surface);
	if (dx11_surface == nullptr)
	{
		return false;
	}
	description.format = dx11_surface->format;
	description.width = dx11_surface->width;
	description.height = dx11_surface->height;
	return true;
}

template <typename Host>
bool DX11ResourceBackend<Host>::Lock_Surface(RenderBackendSurface * surface, RenderBackendLockedSurface & locked_surface, const RenderBackendRect * rect, RenderBackendSurfaceLockMode mode)
{
	DX11Surface *dx11_surface = As_DX11_Surface(surface);
	if (dx11_surface == nullptr || dx11_surface->locked || Is_Compressed_Format(dx11_surface->format))
	{
		return false;
	}
	if (rect != nullptr && (rect->left < 0 || rect->top < 0 || rect->right <= rect->left ||
		rect->bottom <= rect->top || static_cast<unsigned>(rect->right) > dx11_surface->width ||
		static_cast<unsigned>(rect->bottom) > dx11_surface->height))
	{
		return false;
	}
	(void)mode;
	dx11_surface->locked = true;
	const unsigned bpp = Format_Bytes_Per_Pixel(dx11_surface->format);
	unsigned char *bits = dx11_surface->pixels.data();
	if (rect != nullptr)
	{
		bits += static_cast<unsigned>(rect->top) * dx11_surface->pitch;
		bits += static_cast<unsigned>(rect->left) * bpp;
	}
	locked_surface.bits = bits;
	locked_surface.pitch = dx11_surface->pitch;
	return true;
}

template <typename Host>
void DX11ResourceBackend<Host>::Unlock_Surface(RenderBackendSurface * surface)
{
	DX11Surface *dx11_surface = As_DX11_Surface(surface);
	if (dx11_surface != nullptr)
	{
		dx11_surface->locked = false;
		const bool writeback = dx11_surface->owner_writeback;
		const bool committed = Commit_Surface(dx11_surface);
		(void)writeback;
		(void)committed;
	}
}

template <typename Host>
void DX11ResourceBackend<Host>::Release_Surface(RenderBackendSurface * surface)
{
	DX11Surface *dx11_surface = As_DX11_Surface(surface);
	if (dx11_surface != nullptr)
	{
		const bool writeback = dx11_surface->owner_writeback;
		const bool committed = Commit_Surface(dx11_surface);
		(void)writeback;
		(void)committed;
	}
	delete As_DX11_Surface(surface);
}

template <typename Host>
void DX11ResourceBackend<Host>::Copy_Surface_Rect(RenderBackendSurface * source, const RenderBackendRect & source_rect, SurfaceClass * destination, const RenderBackendPoint & destination_point)
{
	if (source == nullptr || destination == nullptr)
	{
		return;
	}
	DX11Surface *destination_surface = As_DX11_Surface(destination->Get_Render_Backend_Surface());
	if (Copy_Surface_Contents(As_DX11_Surface(source), source_rect,
		destination_surface, destination_point))
	{
		const bool writeback = destination_surface != nullptr && destination_surface->owner_writeback;
		const bool committed = Commit_Surface(destination_surface);
		(void)writeback;
		(void)committed;
	}
}

template <typename Host>
bool DX11ResourceBackend<Host>::Copy_Surface_Rect(SurfaceClass * source, const RenderBackendRect & source_rect, RenderBackendSurface * destination, const RenderBackendPoint & destination_point)
{
	if (source == nullptr)
	{
		return false;
	}
	const bool copied = Copy_Surface_Contents(
		As_DX11_Surface(source->Get_Render_Backend_Surface()), source_rect,
		As_DX11_Surface(destination), destination_point);
	if (copied)
	{
		DX11Surface *destination_surface = As_DX11_Surface(destination);
		const bool writeback = destination_surface != nullptr && destination_surface->owner_writeback;
		const bool committed = Commit_Surface(destination_surface);
		(void)writeback;
		(void)committed;
	}
	return copied;
}

template <typename Host>
bool DX11ResourceBackend<Host>::Copy_Surface(SurfaceClass * source, SurfaceClass * destination)
{
	if (source == nullptr || destination == nullptr)
	{
		return false;
	}
	SurfaceClass::SurfaceDescription source_description;
	source->Get_Description(source_description);
	const RenderBackendRect rect = {0, 0, static_cast<int>(source_description.Width),
		static_cast<int>(source_description.Height)};
	const RenderBackendPoint point = {0, 0};
	return Copy_Surface_Rect(source, rect, destination->Get_Render_Backend_Surface(), point);
}

template <typename Host>
bool DX11ResourceBackend<Host>::Copy_Surface_Rect(SurfaceClass * source, const RenderBackendRect & source_rect, SurfaceClass * destination, const RenderBackendPoint & destination_point)
{
	if (source == nullptr || destination == nullptr)
	{
		return false;
	}
	const bool copied = Copy_Surface_Contents(
		As_DX11_Surface(source->Get_Render_Backend_Surface()), source_rect,
		As_DX11_Surface(destination->Get_Render_Backend_Surface()), destination_point);
	if (copied)
	{
		DX11Surface *destination_surface = As_DX11_Surface(destination->Get_Render_Backend_Surface());
		const bool writeback = destination_surface != nullptr && destination_surface->owner_writeback;
		const bool committed = Commit_Surface(destination_surface);
		(void)writeback;
		(void)committed;
	}
	return copied;
}

template <typename Host>
bool DX11ResourceBackend<Host>::Copy_Surface_Stretch(SurfaceClass * source, const RenderBackendRect & source_rect, SurfaceClass * destination, const RenderBackendRect & destination_rect)
{
	if (source == nullptr || destination == nullptr || source_rect.right <= source_rect.left ||
		source_rect.bottom <= source_rect.top || destination_rect.right <= destination_rect.left ||
		destination_rect.bottom <= destination_rect.top)
	{
		return false;
	}
	DX11Surface *source_surface = As_DX11_Surface(source->Get_Render_Backend_Surface());
	DX11Surface *destination_surface = As_DX11_Surface(destination->Get_Render_Backend_Surface());
	if (source_surface == nullptr || destination_surface == nullptr ||
		Is_Compressed_Format(source_surface->format) || Is_Compressed_Format(destination_surface->format))
	{
		return false;
	}
	const unsigned source_bpp = Format_Bytes_Per_Pixel(source_surface->format);
	const unsigned destination_bpp = Format_Bytes_Per_Pixel(destination_surface->format);
	for (int y = destination_rect.top; y < destination_rect.bottom; ++y)
	{
		const float v = static_cast<float>(y - destination_rect.top) /
			std::max(1, destination_rect.bottom - destination_rect.top - 1);
		const int source_y = source_rect.top + static_cast<int>(v * (source_rect.bottom - source_rect.top - 1));
		for (int x = destination_rect.left; x < destination_rect.right; ++x)
		{
			const float u = static_cast<float>(x - destination_rect.left) /
				std::max(1, destination_rect.right - destination_rect.left - 1);
			const int source_x = source_rect.left + static_cast<int>(u * (source_rect.right - source_rect.left - 1));
			const unsigned char *source_pixel = source_surface->pixels.data() +
				static_cast<unsigned>(source_y) * source_surface->pitch +
				static_cast<unsigned>(source_x) * source_bpp;
			unsigned char *destination_pixel = destination_surface->pixels.data() +
				static_cast<unsigned>(y) * destination_surface->pitch +
				static_cast<unsigned>(x) * destination_bpp;
			BitmapHandlerClass::Copy_Pixel(destination_pixel, destination_surface->format,
				source_pixel, source_surface->format, nullptr, 0);
		}
	}
	const bool writeback = destination_surface->owner_writeback;
	const bool committed = Commit_Surface(destination_surface);
	(void)writeback;
	(void)committed;
	return true;
}

template <typename Host>
int DX11ResourceBackend<Host>::Read_Back_Buffer_Rect(void * buffer, int buffer_size, int x, int y, int width, int height)
{
	if (buffer == nullptr || buffer_size <= 0 || width <= 0 || height <= 0)
	{
		return 0;
	}
	SurfaceClass *back_buffer = this->Backend().Get_Back_Buffer_Surface();
	if (back_buffer == nullptr)
	{
		return 0;
	}
	const unsigned row_bytes = static_cast<unsigned>(width) * 4u;
	const int required = static_cast<int>(row_bytes * static_cast<unsigned>(height));
	if (buffer_size < required)
	{
		REF_PTR_RELEASE(back_buffer);
		return required;
	}
	const RenderBackendRect source_rect = {x, y, x + width, y + height};
	const RenderBackendPoint destination_point = {0, 0};
	DX11Surface *surface = As_DX11_Surface(back_buffer->Get_Render_Backend_Surface());
	if (surface == nullptr || source_rect.left < 0 || source_rect.top < 0 ||
		source_rect.right > static_cast<int>(surface->width) ||
		source_rect.bottom > static_cast<int>(surface->height))
	{
		REF_PTR_RELEASE(back_buffer);
		return 0;
	}
	for (int row = 0; row < height; ++row)
	{
		std::memcpy(static_cast<unsigned char *>(buffer) + row * row_bytes,
			surface->pixels.data() + (y + row) * surface->pitch + x * 4, row_bytes);
	}
	REF_PTR_RELEASE(back_buffer);
	return required;
}

template <typename Host>
RenderBackendTextureHandle DX11ResourceBackend<Host>::Create_Transient_Render_Texture(unsigned width, unsigned height, WW3DFormat format)
{
	return Create_Texture_Handle(width, height, format, MIP_LEVELS_1, false, true);
}

template <typename Host>
bool DX11ResourceBackend<Host>::Copy_Back_Buffer_To_Texture(RenderBackendTextureHandle texture)
{
	DX11BackendState &impl = this->State();
	DX11Texture *destination = As_DX11_Texture(texture);
	if (destination == nullptr || destination->resource == nullptr || impl.back_buffer == nullptr)
	{
		return false;
	}
	impl.context->CopyResource(destination->resource, impl.back_buffer);
	return true;
}

template <typename Host>
bool DX11ResourceBackend<Host>::Copy_Texture_To_Surface(RenderBackendTextureHandle texture, SurfaceClass * destination)
{
	DX11Texture *source = As_DX11_Texture(texture);
	if (source == nullptr || destination == nullptr || source->resource == nullptr ||
		this->State().device == nullptr || this->State().context == nullptr ||
		source->kind != RenderBackendTextureKind::Texture2D)
	{
		return false;
	}
	SurfaceClass::SurfaceDescription destination_description;
	destination->Get_Description(destination_description);
	DX11Surface *destination_surface = As_DX11_Surface(destination->Get_Render_Backend_Surface());
	if (destination_surface == nullptr || Is_Compressed_Format(destination_surface->format) ||
		Is_Compressed_Format(source->format))
	{
		return false;
	}
	D3D11_TEXTURE2D_DESC description = {};
	source->resource->GetDesc(&description);
	D3D11_TEXTURE2D_DESC staging_description = description;
	staging_description.Usage = D3D11_USAGE_STAGING;
	staging_description.BindFlags = 0;
	staging_description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	staging_description.MiscFlags = 0;
	ID3D11Texture2D *staging = nullptr;
	if (FAILED(this->State().device->CreateTexture2D(&staging_description, nullptr, &staging)))
	{
		return false;
	}
	this->State().context->CopyResource(staging, source->resource);
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	const bool mapped_ok = SUCCEEDED(this->State().context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped));
	if (mapped_ok)
	{
		const unsigned copy_width = std::min(source->width, destination_surface->width);
		const unsigned copy_height = std::min(source->height, destination_surface->height);
		const unsigned source_bpp = Format_Bytes_Per_Pixel(source->format);
		const unsigned destination_bpp = Format_Bytes_Per_Pixel(destination_surface->format);
		if (source->format == destination_surface->format)
		{
			for (unsigned row = 0; row < copy_height; ++row)
			{
				std::memcpy(destination_surface->pixels.data() + row * destination_surface->pitch,
					static_cast<const unsigned char *>(mapped.pData) + row * mapped.RowPitch,
					copy_width * source_bpp);
			}
		}
		else
		{
			for (unsigned y = 0; y < copy_height; ++y)
			{
				const unsigned char *source_row =
					static_cast<const unsigned char *>(mapped.pData) + y * mapped.RowPitch;
				unsigned char *destination_row =
					destination_surface->pixels.data() + y * destination_surface->pitch;
				for (unsigned x = 0; x < copy_width; ++x)
				{
					BitmapHandlerClass::Copy_Pixel(
						destination_row + x * destination_bpp, destination_surface->format,
						source_row + x * source_bpp, source->format, nullptr, 0);
				}
			}
		}
		this->State().context->Unmap(staging, 0);
	}
	Release_Com(staging);
	return mapped_ok;
}

template <typename Host>
bool DX11ResourceBackend<Host>::Copy_Render_Target_To_Surface(TextureClass * source, SurfaceClass * destination)
{
	if (source == nullptr)
	{
		return false;
	}
	const_cast<TextureClass *>(source)->Ensure_Render_Backend_Texture();
	return Copy_Texture_To_Surface(source->Peek_Render_Backend_Texture(), destination);
}

template <typename Host>
void DX11ResourceBackend<Host>::Release_Transient_Render_Texture(RenderBackendTextureHandle texture)
{
	Release_Texture_Handle(texture);
}

template <typename Host>
RenderBackendTextureHandle DX11ResourceBackend<Host>::Create_Texture_Handle(unsigned width, unsigned height, WW3DFormat format, unsigned mip_levels, bool dynamic, bool render_target)
{
	(void)dynamic;
	return Create_Texture_Handle_Pooled(width, height, format, mip_levels,
		RenderBackendTexturePool::Default, render_target);
}

template <typename Host>
RenderBackendTextureHandle DX11ResourceBackend<Host>::Create_ZTexture_Handle(unsigned width, unsigned height, WW3DZFormat format, unsigned mip_levels)
{
	return Create_ZTexture_Handle_Pooled(width, height, format, mip_levels,
		RenderBackendTexturePool::Default);
}

template <typename Host>
RenderBackendTextureHandle DX11ResourceBackend<Host>::Create_Surface_Handle(unsigned width, unsigned height, WW3DFormat format)
{
	return Create_Texture_Handle(width, height, format, MIP_LEVELS_1, false, false);
}

template <typename Host>
RenderBackendTextureHandle DX11ResourceBackend<Host>::Create_Surface_Handle(const char *filename)
{
	return Create_Texture_From_File_Handle(filename, MIP_LEVELS_1);
}

template <typename Host>
RenderBackendTextureHandle DX11ResourceBackend<Host>::Create_Texture_Handle_Pooled(unsigned width, unsigned height, WW3DFormat format, unsigned mip_levels, RenderBackendTexturePool pool, bool render_target)
{
	(void)pool;
	if (this->State().device == nullptr || width == 0 || height == 0 ||
		(render_target && Is_Compressed_Format(format)))
	{
		return 0;
	}
	if (format == WW3D_FORMAT_UNKNOWN || !Is_Natively_Supported_Format(format))
	{
		format = WW3D_FORMAT_A8R8G8B8;
	}
	format = Storage_Color_Format(format);
	const unsigned levels = Mip_Level_Count(width, height, mip_levels);
	DX11Texture *texture = new DX11Texture();
	texture->width = width;
	texture->height = height;
	texture->levels = levels;
	texture->format = format;
	texture->kind = RenderBackendTextureKind::Texture2D;
	texture->native_format = Native_Color_Format(format);
	texture->render_target = render_target;
	D3D11_TEXTURE2D_DESC description = {};
	description.Width = width;
	description.Height = height;
	description.MipLevels = levels;
	description.ArraySize = 1;
	description.Format = texture->native_format;
	description.SampleDesc.Count = 1;
	description.Usage = D3D11_USAGE_DEFAULT;
	description.BindFlags = D3D11_BIND_SHADER_RESOURCE |
		((render_target || (levels > 1 && !Is_Compressed_Format(format))) ?
			D3D11_BIND_RENDER_TARGET : 0);
	description.MiscFlags = levels > 1 && !Is_Compressed_Format(format) ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
	HRESULT result = this->State().device->CreateTexture2D(&description, nullptr, &texture->resource);
	if (FAILED(result))
	{
		delete texture;
		return 0;
	}
	D3D11_SHADER_RESOURCE_VIEW_DESC view_description = {};
	view_description.Format = texture->native_format;
	view_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	view_description.Texture2D.MostDetailedMip = 0;
	view_description.Texture2D.MipLevels = levels;
	result = this->State().device->CreateShaderResourceView(texture->resource,
		&view_description, &texture->shader_resource_view);
	if (FAILED(result))
	{
		delete texture;
		return 0;
	}
	if (render_target)
	{
		D3D11_RENDER_TARGET_VIEW_DESC render_view_description = {};
		 render_view_description.Format = texture->native_format;
		 render_view_description.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		 render_view_description.Texture2D.MipSlice = 0;
		result = this->State().device->CreateRenderTargetView(texture->resource,
			&render_view_description, &texture->render_target_view);
		if (FAILED(result))
		{
			delete texture;
			return 0;
		}
		if (!Create_Render_Target_Depth_Stencil(this->State().device, width, height,
			texture))
		{
			delete texture;
			return 0;
		}
	}
	return reinterpret_cast<RenderBackendTextureHandle>(texture);
}

template <typename Host>
RenderBackendTextureHandle DX11ResourceBackend<Host>::Create_ZTexture_Handle_Pooled(unsigned width, unsigned height, WW3DZFormat format, unsigned mip_levels, RenderBackendTexturePool pool)
{
	(void)pool;
	if (this->State().device == nullptr || width == 0 || height == 0 ||
		!this->Backend().Supports_Depth_Stencil_Format(format))
	{
		return 0;
	}
	DX11Texture *texture = new DX11Texture();
	texture->width = width;
	texture->height = height;
	texture->levels = 1;
	texture->depth_format = format;
	texture->kind = RenderBackendTextureKind::DepthStencil;
	texture->native_format = Native_Depth_Format(format);
	D3D11_TEXTURE2D_DESC description = {};
	description.Width = width;
	description.Height = height;
	description.MipLevels = 1;
	description.ArraySize = 1;
	description.Format = texture->native_format;
	description.SampleDesc.Count = 1;
	description.Usage = D3D11_USAGE_DEFAULT;
	description.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	HRESULT result = this->State().device->CreateTexture2D(&description, nullptr, &texture->resource);
	if (SUCCEEDED(result))
	{
		result = this->State().device->CreateDepthStencilView(texture->resource, nullptr,
			&texture->depth_stencil_view);
	}
	if (FAILED(result))
	{
		delete texture;
		return 0;
	}
	return reinterpret_cast<RenderBackendTextureHandle>(texture);
}

template <typename Host>
RenderBackendTextureHandle DX11ResourceBackend<Host>::Create_Cube_Texture_Handle(unsigned width, unsigned height, WW3DFormat format, unsigned mip_levels, RenderBackendTexturePool pool, bool render_target)
{
	(void)pool;
	if (this->State().device == nullptr || width == 0 || height == 0 || width != height ||
		(render_target && Is_Compressed_Format(format)))
	{
		return 0;
	}
	if (format == WW3D_FORMAT_UNKNOWN || !Is_Natively_Supported_Format(format))
	{
		format = WW3D_FORMAT_A8R8G8B8;
	}
	format = Storage_Color_Format(format);
	const unsigned levels = Mip_Level_Count(width, height, mip_levels);
	DX11Texture *texture = new DX11Texture();
	texture->width = width;
	texture->height = height;
	texture->levels = levels;
	texture->array_size = 6;
	texture->format = format;
	texture->kind = RenderBackendTextureKind::Cube;
	texture->native_format = Native_Color_Format(format);
	texture->render_target = render_target;
	texture->is_cube = true;

	D3D11_TEXTURE2D_DESC description = {};
	description.Width = width;
	description.Height = height;
	description.MipLevels = levels;
	description.ArraySize = 6;
	description.Format = texture->native_format;
	description.SampleDesc.Count = 1;
	description.Usage = D3D11_USAGE_DEFAULT;
	const bool generate_mips = levels > 1 && !Is_Compressed_Format(format);
	description.BindFlags = D3D11_BIND_SHADER_RESOURCE |
		((render_target || generate_mips) ? D3D11_BIND_RENDER_TARGET : 0);
	description.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE |
		(generate_mips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0);
	HRESULT result = this->State().device->CreateTexture2D(&description, nullptr,
		&texture->resource);
	if (FAILED(result))
	{
		delete texture;
		return 0;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC view_description = {};
	view_description.Format = texture->native_format;
	view_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	view_description.TextureCube.MostDetailedMip = 0;
	view_description.TextureCube.MipLevels = levels;
	result = this->State().device->CreateShaderResourceView(texture->resource,
		&view_description, &texture->shader_resource_view);
	if (FAILED(result))
	{
		delete texture;
		return 0;
	}
	if (render_target)
	{
		// The neutral render-target API has no cube-face parameter. Expose the
		// first face as the writable view; callers that need the other faces can
		// still populate them through Lock_Cube_Texture.
		D3D11_RENDER_TARGET_VIEW_DESC render_view_description = {};
		render_view_description.Format = texture->native_format;
		render_view_description.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		render_view_description.Texture2DArray.MipSlice = 0;
		render_view_description.Texture2DArray.FirstArraySlice = 0;
		render_view_description.Texture2DArray.ArraySize = 1;
		result = this->State().device->CreateRenderTargetView(texture->resource,
			&render_view_description, &texture->render_target_view);
		if (FAILED(result))
		{
			delete texture;
			return 0;
		}
		if (!Create_Render_Target_Depth_Stencil(this->State().device, width, height,
			texture))
		{
			delete texture;
			return 0;
		}
	}
	return reinterpret_cast<RenderBackendTextureHandle>(texture);
}

template <typename Host>
RenderBackendTextureHandle DX11ResourceBackend<Host>::Create_Volume_Texture_Handle(unsigned width, unsigned height, unsigned depth, WW3DFormat format, unsigned mip_levels, RenderBackendTexturePool pool)
{
	(void)pool;
	if (this->State().device == nullptr || width == 0 || height == 0 || depth == 0)
	{
		return 0;
	}
	// Block-compressed 3D resources are not supported by D3D11. Preserve the
	// WW3D request as an uncompressed color volume rather than returning a
	// handle whose resource can never be created.
	if (format == WW3D_FORMAT_UNKNOWN || !Is_Natively_Supported_Format(format) ||
		Is_Compressed_Format(format))
	{
		format = WW3D_FORMAT_A8R8G8B8;
	}
	format = Storage_Color_Format(format);
	const unsigned levels = Mip_Level_Count(width, height, depth, mip_levels);
	DX11Texture *texture = new DX11Texture();
	texture->width = width;
	texture->height = height;
	texture->depth = depth;
	texture->levels = levels;
	texture->format = format;
	texture->kind = RenderBackendTextureKind::Volume;
	texture->native_format = Native_Color_Format(format);

	D3D11_TEXTURE3D_DESC description = {};
	description.Width = width;
	description.Height = height;
	description.Depth = depth;
	description.MipLevels = levels;
	description.Format = texture->native_format;
	description.Usage = D3D11_USAGE_DEFAULT;
	const bool generate_mips = levels > 1;
	description.BindFlags = D3D11_BIND_SHADER_RESOURCE |
		(generate_mips ? D3D11_BIND_RENDER_TARGET : 0);
	description.MiscFlags = generate_mips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
	HRESULT result = this->State().device->CreateTexture3D(&description, nullptr,
		&texture->volume_resource);
	if (FAILED(result))
	{
		delete texture;
		return 0;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC view_description = {};
	view_description.Format = texture->native_format;
	view_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	view_description.Texture3D.MostDetailedMip = 0;
	view_description.Texture3D.MipLevels = levels;
	result = this->State().device->CreateShaderResourceView(texture->volume_resource,
		&view_description, &texture->shader_resource_view);
	if (FAILED(result))
	{
		delete texture;
		return 0;
	}
	return reinterpret_cast<RenderBackendTextureHandle>(texture);
}

template <typename Host>
RenderBackendTextureHandle DX11ResourceBackend<Host>::Create_Texture_From_Surface(RenderBackendSurface * surface, unsigned mip_levels)
{
	const DX11Surface *source = As_DX11_Surface(surface);
	if (this->State().device == nullptr || source == nullptr || source->width == 0 ||
		source->height == 0 || Is_Compressed_Format(source->format) &&
		(source->width % 4 != 0 || source->height % 4 != 0))
	{
		return 0;
	}

	const WW3DFormat destination_format = Is_Natively_Supported_Format(source->format) ?
		source->format : WW3D_FORMAT_A8R8G8B8;
	const RenderBackendTextureHandle handle = Create_Texture_Handle_Pooled(
		source->width, source->height, destination_format, mip_levels,
		RenderBackendTexturePool::Default, false);
	DX11Texture *destination = As_DX11_Texture(handle);
	if (destination == nullptr)
	{
		return 0;
	}

	std::vector<unsigned char> converted;
	const unsigned char *source_bits = source->pixels.data();
	unsigned source_pitch = source->pitch;
	if (source->format != destination->format)
	{
		converted.resize(static_cast<size_t>(source->width) * source->height * 4u);
		const unsigned source_bpp = Format_Bytes_Per_Pixel(source->format);
		for (unsigned y = 0; y < source->height; ++y)
		{
			for (unsigned x = 0; x < source->width; ++x)
			{
				BitmapHandlerClass::Copy_Pixel(
					converted.data() + (y * source->width + x) * 4u,
					WW3D_FORMAT_A8R8G8B8,
					source_bits + y * source_pitch + x * source_bpp,
					source->format, nullptr, 0);
			}
		}
		source_bits = converted.data();
		source_pitch = source->width * 4u;
	}

	this->State().context->UpdateSubresource(destination->resource, 0, nullptr,
		source_bits, source_pitch, Surface_Bytes(destination->format,
		destination->width, destination->height));
	if (destination->levels > 1 &&
		destination->shader_resource_view != nullptr)
	{
		this->State().context->GenerateMips(destination->shader_resource_view);
	}
	return handle;
}

template <typename Host>
SurfaceClass * DX11ResourceBackend<Host>::Get_Texture_Surface_Level(RenderBackendTextureHandle texture, unsigned level)
{
	DX11Texture *source = As_DX11_Texture(texture);
	if (source == nullptr || source->resource == nullptr || this->State().device == nullptr ||
		level >= source->levels || source->kind != RenderBackendTextureKind::Texture2D)
	{
		return nullptr;
	}

	D3D11_TEXTURE2D_DESC source_description = {};
	source->resource->GetDesc(&source_description);
	D3D11_TEXTURE2D_DESC staging_description = source_description;
	staging_description.Usage = D3D11_USAGE_STAGING;
	staging_description.BindFlags = 0;
	staging_description.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	staging_description.MiscFlags = 0;
	ID3D11Texture2D *staging = nullptr;
	if (FAILED(this->State().device->CreateTexture2D(&staging_description, nullptr, &staging)))
	{
		return nullptr;
	}
	this->State().context->CopySubresourceRegion(staging, level, 0, 0, 0,
		source->resource, level, nullptr);
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (FAILED(this->State().context->Map(staging, level, D3D11_MAP_READ, 0, &mapped)))
	{
		Release_Com(staging);
		return nullptr;
	}

	const unsigned level_width = std::max(1u, source->width >> level);
	const unsigned level_height = std::max(1u, source->height >> level);
	DX11Surface *surface = new DX11Surface(level_width, level_height, source->format);
	const unsigned rows = Is_Compressed_Format(source->format) ?
		std::max(1u, (level_height + 3u) / 4u) : level_height;
	const unsigned row_bytes = Row_Bytes(source->format, level_width);
	for (unsigned row = 0; row < rows; ++row)
	{
		std::memcpy(surface->pixels.data() + row * surface->pitch,
			static_cast<const unsigned char *>(mapped.pData) + row * mapped.RowPitch,
			std::min(row_bytes, surface->pitch));
	}
	this->State().context->Unmap(staging, level);
	surface->owner_resource = source->resource;
	surface->owner_resource->AddRef();
	surface->owner_staging = staging;
	surface->owner_context = this->State().context;
	surface->owner_context->AddRef();
	surface->owner_subresource = level;
	surface->owner_writeback = true;
	return new SurfaceClass(surface);
}

template <typename Host>
RenderBackendTextureHandle DX11ResourceBackend<Host>::Create_Texture_From_File_Handle(const char *filename, unsigned mip_levels)
{
	if (filename == nullptr || filename[0] == '\0')
	{
		return Create_Missing_Texture();
	}
	StringClass name(filename, true);
	RenderBackendSurface *surface = TextureLoader::Load_Surface_Immediate(
		name, WW3D_FORMAT_UNKNOWN, true);
	if (surface == nullptr)
	{
		return Create_Missing_Texture();
	}
	const RenderBackendTextureHandle texture = Create_Texture_From_Surface(surface, mip_levels);
	Release_Surface(surface);
	if (texture == 0)
	{
		return Create_Missing_Texture();
	}
	return texture;
}

template <typename Host>
RenderBackendTextureHandle DX11ResourceBackend<Host>::Add_Texture_Reference(RenderBackendTextureHandle texture)
{
	DX11Texture *dx11_texture = As_DX11_Texture(texture);
	if (dx11_texture != nullptr)
	{
		++dx11_texture->references;
	}
	return texture;
}

template <typename Host>
void DX11ResourceBackend<Host>::Release_Texture_Handle(RenderBackendTextureHandle texture)
{
	DX11Texture *dx11_texture = As_DX11_Texture(texture);
	if (dx11_texture == nullptr || dx11_texture->references == 0)
	{
		return;
	}
	--dx11_texture->references;
	if (dx11_texture->references != 0)
	{
		return;
	}
	for (DX11Texture *&bound_texture : this->State().textures)
	{
		if (bound_texture == dx11_texture)
		{
			const unsigned stage = static_cast<unsigned>(&bound_texture - this->State().textures.data());
			bound_texture = nullptr;
			this->State().texture_kinds[stage] = RenderBackendTextureKind::Texture2D;
		}
	}
	for (unsigned stage = 0; stage < MAX_TEXTURE_STAGES; ++stage)
	{
		if (this->State().direct_texture_overrides[stage] == dx11_texture)
		{
			this->State().direct_texture_overrides[stage] = nullptr;
			this->State().direct_texture_override_valid[stage] = false;
		}
	}
	if (this->State().active_render_target_view == dx11_texture->render_target_view)
	{
		this->State().active_render_target_view = this->State().back_buffer_view;
		this->State().render_to_texture = false;
	}
	if (this->State().active_depth_stencil_view == dx11_texture->depth_stencil_view ||
		this->State().active_depth_stencil_view == dx11_texture->render_target_depth_stencil_view)
	{
		this->State().active_depth_stencil_view = this->State().depth_buffer_view;
	}
	// The native state cache stores raw D3D11 view pointers.  A final texture
	// release can invalidate one of those pointers without changing any of the
	// neutral render-state fields, so the next draw must rebuild the bindings.
	// This is especially important when an asynchronously loaded terrain input
	// is replaced after the first frame.
	this->State().Mark_All_State_Dirty();
	this->State().native_state_valid = false;
	delete dx11_texture;
}

template <typename Host>
unsigned DX11ResourceBackend<Host>::Get_Texture_Level_Count(RenderBackendTextureHandle texture) const
{
	const DX11Texture *dx11_texture = As_DX11_Texture_Const(texture);
	return dx11_texture == nullptr ? 0 : dx11_texture->levels;
}

template <typename Host>
bool DX11ResourceBackend<Host>::Get_Texture_Description(RenderBackendTextureHandle texture, unsigned level, RenderBackendTextureDescription & description) const
{
	const DX11Texture *dx11_texture = As_DX11_Texture_Const(texture);
	if (dx11_texture == nullptr || level >= dx11_texture->levels)
	{
		return false;
	}
	description = RenderBackendTextureDescription();
	description.kind = dx11_texture->kind;
	description.format = dx11_texture->format;
	description.depth_format = dx11_texture->depth_format;
	description.width = std::max(1u, dx11_texture->width >> level);
	description.height = std::max(1u, dx11_texture->height >> level);
	description.depth = dx11_texture->depth;
	description.mip_levels = dx11_texture->levels;
	return true;
}

template <typename Host>
bool DX11ResourceBackend<Host>::Lock_Texture(RenderBackendTextureHandle texture, unsigned level, RenderBackendTextureLock & locked_texture, bool read_only)
{
	locked_texture = RenderBackendTextureLock();
	DX11Texture *dx11_texture = As_DX11_Texture(texture);
	if (dx11_texture == nullptr || dx11_texture->resource == nullptr ||
		this->State().device == nullptr || this->State().context == nullptr ||
		dx11_texture->kind != RenderBackendTextureKind::Texture2D ||
		level >= dx11_texture->levels)
	{
		return false;
	}
	const unsigned subresource = Texture_Subresource(dx11_texture, level);
	const auto existing_lock = std::find_if(dx11_texture->active_locks.begin(),
		dx11_texture->active_locks.end(),
		[subresource](const DX11TextureLock &lock) {
			return lock.subresource == subresource;
		});
	if (existing_lock != dx11_texture->active_locks.end())
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC source_description = {};
	dx11_texture->resource->GetDesc(&source_description);
	D3D11_TEXTURE2D_DESC staging_description = source_description;
	staging_description.Usage = D3D11_USAGE_STAGING;
	staging_description.BindFlags = 0;
	staging_description.CPUAccessFlags = D3D11_CPU_ACCESS_READ |
		(read_only ? 0 : D3D11_CPU_ACCESS_WRITE);
	staging_description.MiscFlags = 0;
	ID3D11Texture2D *staging = nullptr;
	if (FAILED(this->State().device->CreateTexture2D(&staging_description, nullptr, &staging)))
	{
		return false;
	}
	dx11_texture->active_locks.emplace_back();
	DX11TextureLock &active_lock = dx11_texture->active_locks.back();
	active_lock.level = level;
	active_lock.subresource = subresource;
	active_lock.staging = staging;
	active_lock.read_only = read_only;
	this->State().context->CopySubresourceRegion(active_lock.staging, subresource,
		0, 0, 0, dx11_texture->resource, subresource, nullptr);
	const D3D11_MAP map_mode = read_only ? D3D11_MAP_READ : D3D11_MAP_READ_WRITE;
	if (FAILED(this->State().context->Map(active_lock.staging, subresource, map_mode, 0,
		&active_lock.mapped)))
	{
		Release_Com(active_lock.staging);
		dx11_texture->active_locks.pop_back();
		return false;
	}
	locked_texture.bits = active_lock.mapped.pData;
	locked_texture.row_pitch = active_lock.mapped.RowPitch;
	locked_texture.slice_pitch = active_lock.mapped.DepthPitch;
	return true;
}

template <typename Host>
void DX11ResourceBackend<Host>::Unlock_Texture(RenderBackendTextureHandle texture, unsigned level)
{
	DX11Texture *dx11_texture = As_DX11_Texture(texture);
	if (dx11_texture == nullptr || dx11_texture->resource == nullptr ||
		this->State().context == nullptr || level >= dx11_texture->levels)
	{
		return;
	}
	const unsigned subresource = Texture_Subresource(dx11_texture, level);
	const auto lock = std::find_if(dx11_texture->active_locks.begin(),
		dx11_texture->active_locks.end(),
		[subresource](const DX11TextureLock &active_lock) {
			return active_lock.subresource == subresource;
		});
	if (lock == dx11_texture->active_locks.end())
	{
		return;
	}
	this->State().context->Unmap(lock->staging, subresource);
	if (!lock->read_only)
	{
		this->State().context->CopySubresourceRegion(dx11_texture->resource, subresource,
			0, 0, 0, lock->staging, subresource, nullptr);
	}
	Release_Com(lock->staging);
	dx11_texture->active_locks.erase(lock);
}

template <typename Host>
bool DX11ResourceBackend<Host>::Lock_Cube_Texture(RenderBackendTextureHandle texture, RenderBackendCubeFace face, unsigned level, RenderBackendTextureLock & locked_texture, bool read_only)
{
	locked_texture = RenderBackendTextureLock();
	DX11Texture *dx11_texture = As_DX11_Texture(texture);
	if (dx11_texture == nullptr || dx11_texture->resource == nullptr ||
		this->State().device == nullptr || this->State().context == nullptr ||
		dx11_texture->kind != RenderBackendTextureKind::Cube ||
		level >= dx11_texture->levels || Cube_Face_Index(face) >= dx11_texture->array_size)
	{
		return false;
	}
	const unsigned array_slice = Cube_Face_Index(face);
	const unsigned subresource = Texture_Subresource(dx11_texture, level, array_slice);
	const auto existing_lock = std::find_if(dx11_texture->active_locks.begin(),
		dx11_texture->active_locks.end(),
		[subresource](const DX11TextureLock &lock) {
			return lock.subresource == subresource;
		});
	if (existing_lock != dx11_texture->active_locks.end())
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC source_description = {};
	dx11_texture->resource->GetDesc(&source_description);
	D3D11_TEXTURE2D_DESC staging_description = source_description;
	staging_description.Usage = D3D11_USAGE_STAGING;
	staging_description.BindFlags = 0;
	staging_description.CPUAccessFlags = D3D11_CPU_ACCESS_READ |
		(read_only ? 0 : D3D11_CPU_ACCESS_WRITE);
	staging_description.MiscFlags = 0;
	ID3D11Texture2D *staging = nullptr;
	if (FAILED(this->State().device->CreateTexture2D(&staging_description, nullptr, &staging)))
	{
		return false;
	}
	dx11_texture->active_locks.emplace_back();
	DX11TextureLock &active_lock = dx11_texture->active_locks.back();
	active_lock.level = level;
	active_lock.subresource = subresource;
	active_lock.staging = staging;
	active_lock.read_only = read_only;
	this->State().context->CopySubresourceRegion(active_lock.staging, subresource,
		0, 0, 0, dx11_texture->resource, subresource, nullptr);
	const D3D11_MAP map_mode = read_only ? D3D11_MAP_READ : D3D11_MAP_READ_WRITE;
	if (FAILED(this->State().context->Map(active_lock.staging, subresource, map_mode, 0,
		&active_lock.mapped)))
	{
		Release_Com(active_lock.staging);
		dx11_texture->active_locks.pop_back();
		return false;
	}
	locked_texture.bits = active_lock.mapped.pData;
	locked_texture.row_pitch = active_lock.mapped.RowPitch;
	locked_texture.slice_pitch = active_lock.mapped.DepthPitch;
	return true;
}

template <typename Host>
void DX11ResourceBackend<Host>::Unlock_Cube_Texture(RenderBackendTextureHandle texture, RenderBackendCubeFace face, unsigned level)
{
	DX11Texture *dx11_texture = As_DX11_Texture(texture);
	if (dx11_texture == nullptr || dx11_texture->resource == nullptr ||
		this->State().context == nullptr || dx11_texture->kind != RenderBackendTextureKind::Cube ||
		level >= dx11_texture->levels || Cube_Face_Index(face) >= dx11_texture->array_size)
	{
		return;
	}
	const unsigned subresource = Texture_Subresource(dx11_texture, level, Cube_Face_Index(face));
	const auto lock = std::find_if(dx11_texture->active_locks.begin(),
		dx11_texture->active_locks.end(),
		[subresource](const DX11TextureLock &active_lock) {
			return active_lock.subresource == subresource;
		});
	if (lock == dx11_texture->active_locks.end())
	{
		return;
	}
	this->State().context->Unmap(lock->staging, subresource);
	if (!lock->read_only)
	{
		this->State().context->CopySubresourceRegion(dx11_texture->resource, subresource,
			0, 0, 0, lock->staging, subresource, nullptr);
	}
	Release_Com(lock->staging);
	dx11_texture->active_locks.erase(lock);
}

template <typename Host>
bool DX11ResourceBackend<Host>::Lock_Volume_Texture(RenderBackendTextureHandle texture, unsigned level, RenderBackendTextureLock & locked_texture, bool read_only)
{
	locked_texture = RenderBackendTextureLock();
	DX11Texture *dx11_texture = As_DX11_Texture(texture);
	if (dx11_texture == nullptr || dx11_texture->volume_resource == nullptr ||
		this->State().device == nullptr || this->State().context == nullptr ||
		dx11_texture->kind != RenderBackendTextureKind::Volume ||
		level >= dx11_texture->levels)
	{
		return false;
	}
	const unsigned subresource = Texture_Subresource(dx11_texture, level);
	const auto existing_lock = std::find_if(dx11_texture->active_locks.begin(),
		dx11_texture->active_locks.end(),
		[subresource](const DX11TextureLock &lock) {
			return lock.subresource == subresource;
		});
	if (existing_lock != dx11_texture->active_locks.end())
	{
		return false;
	}

	D3D11_TEXTURE3D_DESC source_description = {};
	dx11_texture->volume_resource->GetDesc(&source_description);
	D3D11_TEXTURE3D_DESC staging_description = source_description;
	staging_description.Usage = D3D11_USAGE_STAGING;
	staging_description.BindFlags = 0;
	staging_description.CPUAccessFlags = D3D11_CPU_ACCESS_READ |
		(read_only ? 0 : D3D11_CPU_ACCESS_WRITE);
	staging_description.MiscFlags = 0;
	ID3D11Texture3D *staging = nullptr;
	if (FAILED(this->State().device->CreateTexture3D(&staging_description, nullptr, &staging)))
	{
		return false;
	}
	dx11_texture->active_locks.emplace_back();
	DX11TextureLock &active_lock = dx11_texture->active_locks.back();
	active_lock.level = level;
	active_lock.subresource = subresource;
	active_lock.staging = staging;
	active_lock.read_only = read_only;
	this->State().context->CopySubresourceRegion(active_lock.staging, subresource,
		0, 0, 0, dx11_texture->volume_resource, subresource, nullptr);
	const D3D11_MAP map_mode = read_only ? D3D11_MAP_READ : D3D11_MAP_READ_WRITE;
	if (FAILED(this->State().context->Map(active_lock.staging, subresource, map_mode, 0,
		&active_lock.mapped)))
	{
		Release_Com(active_lock.staging);
		dx11_texture->active_locks.pop_back();
		return false;
	}
	locked_texture.bits = active_lock.mapped.pData;
	locked_texture.row_pitch = active_lock.mapped.RowPitch;
	locked_texture.slice_pitch = active_lock.mapped.DepthPitch;
	return true;
}

template <typename Host>
void DX11ResourceBackend<Host>::Unlock_Volume_Texture(RenderBackendTextureHandle texture, unsigned level)
{
	DX11Texture *dx11_texture = As_DX11_Texture(texture);
	if (dx11_texture == nullptr || dx11_texture->volume_resource == nullptr ||
		this->State().context == nullptr || dx11_texture->kind != RenderBackendTextureKind::Volume ||
		level >= dx11_texture->levels)
	{
		return;
	}
	const unsigned subresource = Texture_Subresource(dx11_texture, level);
	const auto lock = std::find_if(dx11_texture->active_locks.begin(),
		dx11_texture->active_locks.end(),
		[subresource](const DX11TextureLock &active_lock) {
			return active_lock.subresource == subresource;
		});
	if (lock == dx11_texture->active_locks.end())
	{
		return;
	}
	this->State().context->Unmap(lock->staging, subresource);
	if (!lock->read_only)
	{
		this->State().context->CopySubresourceRegion(dx11_texture->volume_resource, subresource,
			0, 0, 0, lock->staging, subresource, nullptr);
	}
	Release_Com(lock->staging);
	dx11_texture->active_locks.erase(lock);
}

template <typename Host>
bool DX11ResourceBackend<Host>::Update_Texture(RenderBackendTextureHandle source, RenderBackendTextureHandle destination)
{
	DX11Texture *source_texture = As_DX11_Texture(source);
	DX11Texture *destination_texture = As_DX11_Texture(destination);
	ID3D11Resource *source_resource = Texture_Resource(source_texture);
	ID3D11Resource *destination_resource = Texture_Resource(destination_texture);
	if (source_texture == nullptr || destination_texture == nullptr ||
		source_resource == nullptr || destination_resource == nullptr ||
		source_texture->kind != destination_texture->kind ||
		source_texture->width != destination_texture->width ||
		source_texture->height != destination_texture->height ||
		source_texture->depth != destination_texture->depth ||
		source_texture->levels != destination_texture->levels ||
		source_texture->native_format != destination_texture->native_format)
	{
		return false;
	}
	this->State().context->CopyResource(destination_resource, source_resource);
	return true;
}

template <typename Host>
bool DX11ResourceBackend<Host>::Generate_Texture_Mipmaps(RenderBackendTextureHandle texture)
{
	DX11Texture *dx11_texture = As_DX11_Texture(texture);
	if (dx11_texture == nullptr || dx11_texture->shader_resource_view == nullptr ||
		dx11_texture->levels <= 1 || Is_Compressed_Format(dx11_texture->format))
	{
		return false;
	}
	this->State().context->GenerateMips(dx11_texture->shader_resource_view);
	return true;
}

template <typename Host>
void DX11ResourceBackend<Host>::Set_Texture_LOD(RenderBackendTextureHandle texture, unsigned lod)
{
	DX11Texture *dx11_texture = As_DX11_Texture(texture);
	if (dx11_texture != nullptr)
	{
		dx11_texture->lod = std::min(lod, dx11_texture->levels == 0 ? 0u : dx11_texture->levels - 1);
	}
}

template <typename Host>
unsigned DX11ResourceBackend<Host>::Get_Texture_Priority(RenderBackendTextureHandle texture) const
{
	const DX11Texture *dx11_texture = As_DX11_Texture_Const(texture);
	return dx11_texture == nullptr ? 0 : dx11_texture->priority;
}

template <typename Host>
unsigned DX11ResourceBackend<Host>::Set_Texture_Priority(RenderBackendTextureHandle texture, unsigned priority)
{
	DX11Texture *dx11_texture = As_DX11_Texture(texture);
	if (dx11_texture == nullptr)
	{
		return 0;
	}
	const unsigned previous = dx11_texture->priority;
	dx11_texture->priority = priority;
	return previous;
}

template <typename Host>
bool DX11ResourceBackend<Host>::Is_Missing_Texture_Handle(RenderBackendTextureHandle texture) const
{
	const DX11Texture *dx11_texture = As_DX11_Texture_Const(texture);
	return dx11_texture != nullptr && dx11_texture->missing;
}

template <typename Host>
RenderBackendTextureHandle DX11ResourceBackend<Host>::Create_Missing_Texture()
{
	const RenderBackendTextureHandle handle = Create_Texture_Handle(
		2, 2, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1, false, false);
	DX11Texture *texture = As_DX11_Texture(handle);
	if (texture == nullptr)
	{
		return 0;
	}
	texture->missing = true;
	const unsigned pixels[4] = {0xffff00ffu, 0xff000000u, 0xff000000u, 0xffff00ffu};
	this->State().context->UpdateSubresource(texture->resource, 0, nullptr, pixels,
		2u * sizeof(unsigned), sizeof(pixels));
	return handle;
}

template <typename Host>
RenderBackendSurface * DX11ResourceBackend<Host>::Create_Missing_Surface()
{
	DX11Surface *surface = new DX11Surface(2, 2, WW3D_FORMAT_A8R8G8B8);
	const unsigned pixels[4] = {0xffff00ffu, 0xff000000u, 0xff000000u, 0xffff00ffu};
	std::memcpy(surface->pixels.data(), pixels, sizeof(pixels));
	return surface;
}

template <typename Host>
void DX11ResourceBackend<Host>::Register_Texture(TextureBaseClass * texture, RenderBackendTextureKind kind, unsigned width, unsigned height, unsigned depth, WW3DFormat format, WW3DZFormat depth_format, unsigned mip_levels, bool render_target)
{
	if (texture == nullptr)
	{
		return;
	}
	const auto existing = std::find_if(this->State().registered_textures.begin(),
		this->State().registered_textures.end(),
		[texture](const DX11TextureRegistration & registration) {
			return registration.texture == texture;
		});
	if (existing != this->State().registered_textures.end())
	{
		return;
	}
	DX11TextureRegistration registration;
	registration.texture = texture;
	registration.kind = kind;
	registration.width = width;
	registration.height = height;
	registration.depth = depth;
	registration.format = format;
	registration.depth_format = depth_format;
	registration.mip_levels = mip_levels;
	registration.render_target = render_target;
	this->State().registered_textures.push_back(registration);
}

template <typename Host>
void DX11ResourceBackend<Host>::Unregister_Texture(TextureBaseClass * texture)
{
	if (texture == nullptr)
	{
		return;
	}
	if (this->State().active_render_target == texture)
	{
		this->State().active_render_target = nullptr;
		this->State().active_render_target_view = this->State().back_buffer_view;
		this->State().render_to_texture = false;
	}
	if (this->State().active_depth_target == texture)
	{
		this->State().active_depth_target = nullptr;
		this->State().active_depth_stencil_view = this->State().depth_buffer_view;
	}
	this->State().Mark_All_State_Dirty();
	this->State().native_state_valid = false;
	this->State().registered_textures.erase(
		std::remove_if(this->State().registered_textures.begin(),
			this->State().registered_textures.end(),
			[texture](const DX11TextureRegistration & registration) {
				return registration.texture == texture;
			}),
		this->State().registered_textures.end());
}

template <typename Host>
RenderBackendVertexBuffer * DX11ResourceBackend<Host>::Create_Vertex_Buffer(unsigned size_bytes, const RenderBackendVertexLayout &layout, unsigned usage)
{
	if (this->State().device == nullptr || size_bytes == 0)
	{
		return nullptr;
	}
	DX11VertexBuffer *buffer = new DX11VertexBuffer();
	buffer->size = size_bytes;
	buffer->layout = layout;
	buffer->dynamic = (usage & BUFFER_USAGE_DYNAMIC) != 0;
	buffer->shadow.resize(size_bytes);
	D3D11_BUFFER_DESC description = {};
	description.ByteWidth = size_bytes;
	description.Usage = buffer->dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
	const bool process_capable = !buffer->dynamic && (size_bytes & 3u) == 0u;
	description.BindFlags = D3D11_BIND_VERTEX_BUFFER |
		(process_capable ? (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS) : 0u);
	description.CPUAccessFlags = buffer->dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
	description.MiscFlags = process_capable ? D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS : 0u;
	if (FAILED(this->State().device->CreateBuffer(&description, nullptr, &buffer->resource)))
	{
		delete buffer;
		return nullptr;
	}
	if (process_capable)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_description = {};
		shader_resource_description.Format = DXGI_FORMAT_R32_TYPELESS;
		shader_resource_description.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
		shader_resource_description.BufferEx.FirstElement = 0;
		shader_resource_description.BufferEx.NumElements = size_bytes / 4u;
		shader_resource_description.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
		if (FAILED(this->State().device->CreateShaderResourceView(buffer->resource,
			&shader_resource_description, &buffer->process_shader_resource_view)))
		{
			delete buffer;
			return nullptr;
		}

		D3D11_UNORDERED_ACCESS_VIEW_DESC unordered_access_description = {};
		unordered_access_description.Format = DXGI_FORMAT_R32_TYPELESS;
		unordered_access_description.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		unordered_access_description.Buffer.FirstElement = 0;
		unordered_access_description.Buffer.NumElements = size_bytes / 4u;
		unordered_access_description.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
		if (FAILED(this->State().device->CreateUnorderedAccessView(buffer->resource,
			&unordered_access_description, &buffer->process_unordered_access_view)))
		{
			delete buffer;
			return nullptr;
		}
	}
	return buffer;
}

template <typename Host>
RenderBackendIndexBuffer * DX11ResourceBackend<Host>::Create_Index_Buffer(unsigned size_bytes, unsigned usage)
{
	if (this->State().device == nullptr || size_bytes == 0)
	{
		return nullptr;
	}
	DX11IndexBuffer *buffer = new DX11IndexBuffer();
	buffer->size = size_bytes;
	buffer->dynamic = (usage & BUFFER_USAGE_DYNAMIC) != 0;
	buffer->shadow.resize(size_bytes);
	D3D11_BUFFER_DESC description = {};
	description.ByteWidth = size_bytes;
	description.Usage = buffer->dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
	description.BindFlags = D3D11_BIND_INDEX_BUFFER;
	description.CPUAccessFlags = buffer->dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
	if (FAILED(this->State().device->CreateBuffer(&description, nullptr, &buffer->resource)))
	{
		delete buffer;
		return nullptr;
	}
	return buffer;
}

template <typename Host>
bool DX11ResourceBackend<Host>::Lock_Vertex_Buffer(RenderBackendVertexBuffer * buffer, unsigned offset_bytes, unsigned size_bytes, void ** data, RenderBackendBufferLockMode mode)
{
	DX11VertexBuffer *vertex_buffer = static_cast<DX11VertexBuffer *>(buffer);
	if (vertex_buffer == nullptr || data == nullptr || vertex_buffer->locked ||
		offset_bytes > vertex_buffer->size)
	{
		return false;
	}
	size_bytes = size_bytes == 0 ? vertex_buffer->size - offset_bytes : size_bytes;
	if (offset_bytes + size_bytes > vertex_buffer->size)
	{
		return false;
	}
	if (!vertex_buffer->dynamic)
	{
		vertex_buffer->locked = true;
		*data = vertex_buffer->shadow.data() + offset_bytes;
		return true;
	}
	const D3D11_MAP map_mode = mode == RenderBackendBufferLockMode::NoOverwrite ?
		D3D11_MAP_WRITE_NO_OVERWRITE : D3D11_MAP_WRITE_DISCARD;
	if (FAILED(this->State().context->Map(vertex_buffer->resource, 0, map_mode, 0,
		&vertex_buffer->mapped)))
	{
		return false;
	}
	vertex_buffer->locked = true;
	vertex_buffer->gpu_mapped = true;
	vertex_buffer->mapped_offset = offset_bytes;
	vertex_buffer->mapped_size = size_bytes;
	*data = static_cast<unsigned char *>(vertex_buffer->mapped.pData) + offset_bytes;
	return true;
}

template <typename Host>
bool DX11ResourceBackend<Host>::Lock_Index_Buffer(RenderBackendIndexBuffer * buffer, unsigned offset_bytes, unsigned size_bytes, void ** data, RenderBackendBufferLockMode mode)
{
	DX11IndexBuffer *index_buffer = static_cast<DX11IndexBuffer *>(buffer);
	if (index_buffer == nullptr || data == nullptr || index_buffer->locked ||
		offset_bytes > index_buffer->size)
	{
		return false;
	}
	size_bytes = size_bytes == 0 ? index_buffer->size - offset_bytes : size_bytes;
	if (offset_bytes + size_bytes > index_buffer->size)
	{
		return false;
	}
	if (!index_buffer->dynamic)
	{
		index_buffer->locked = true;
		*data = index_buffer->shadow.data() + offset_bytes;
		return true;
	}
	const D3D11_MAP map_mode = mode == RenderBackendBufferLockMode::NoOverwrite ?
		D3D11_MAP_WRITE_NO_OVERWRITE : D3D11_MAP_WRITE_DISCARD;
	if (FAILED(this->State().context->Map(index_buffer->resource, 0, map_mode, 0,
		&index_buffer->mapped)))
	{
		return false;
	}
	index_buffer->locked = true;
	index_buffer->gpu_mapped = true;
	index_buffer->mapped_offset = offset_bytes;
	index_buffer->mapped_size = size_bytes;
	*data = static_cast<unsigned char *>(index_buffer->mapped.pData) + offset_bytes;
	return true;
}

template <typename Host>
void DX11ResourceBackend<Host>::Unlock_Vertex_Buffer(RenderBackendVertexBuffer * buffer)
{
	DX11VertexBuffer *vertex_buffer = static_cast<DX11VertexBuffer *>(buffer);
	if (vertex_buffer == nullptr || !vertex_buffer->locked)
	{
		return;
	}
	if (vertex_buffer->gpu_mapped)
	{
		std::memcpy(vertex_buffer->shadow.data() + vertex_buffer->mapped_offset,
			static_cast<const unsigned char *>(vertex_buffer->mapped.pData) +
				vertex_buffer->mapped_offset, vertex_buffer->mapped_size);
		this->State().context->Unmap(vertex_buffer->resource, 0);
		vertex_buffer->gpu_mapped = false;
	}
	else if (vertex_buffer->resource != nullptr)
	{
		this->State().context->UpdateSubresource(vertex_buffer->resource, 0, nullptr,
			vertex_buffer->shadow.data(), 0, 0);
	}
	vertex_buffer->locked = false;
}

template <typename Host>
void DX11ResourceBackend<Host>::Unlock_Index_Buffer(RenderBackendIndexBuffer * buffer)
{
	DX11IndexBuffer *index_buffer = static_cast<DX11IndexBuffer *>(buffer);
	if (index_buffer == nullptr || !index_buffer->locked)
	{
		return;
	}
	if (index_buffer->gpu_mapped)
	{
		std::memcpy(index_buffer->shadow.data() + index_buffer->mapped_offset,
			static_cast<const unsigned char *>(index_buffer->mapped.pData) +
				index_buffer->mapped_offset, index_buffer->mapped_size);
		this->State().context->Unmap(index_buffer->resource, 0);
		index_buffer->gpu_mapped = false;
	}
	else if (index_buffer->resource != nullptr)
	{
		this->State().context->UpdateSubresource(index_buffer->resource, 0, nullptr,
			index_buffer->shadow.data(), 0, 0);
	}
	index_buffer->locked = false;
}

template <typename Host>
void DX11ResourceBackend<Host>::Release_Vertex_Buffer(RenderBackendVertexBuffer * buffer)
{
	delete static_cast<DX11VertexBuffer *>(buffer);
}

template <typename Host>
void DX11ResourceBackend<Host>::Release_Index_Buffer(RenderBackendIndexBuffer * buffer)
{
	delete static_cast<DX11IndexBuffer *>(buffer);
}

template class DX11ResourceBackend<DX11BackendRuntime>;
}
