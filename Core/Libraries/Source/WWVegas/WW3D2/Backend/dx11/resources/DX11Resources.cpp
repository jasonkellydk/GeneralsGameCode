/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2025 Electronic Arts Inc.
**
** D3D11 resource storage and surface transfer implementation.
*/

#include "Backend/dx11/resources/DX11Resources.h"

#include <algorithm>
#include <cstring>

#include "BitmapHandler.h"

namespace dx11_backend
{
namespace
{
	template <typename T>
	void Release_Com(T *&object)
	{
		if (object != nullptr)
		{
			object->Release();
			object = nullptr;
		}
	}
}

unsigned Format_Bytes_Per_Pixel(WW3DFormat format)
{
	switch (format)
	{
	case WW3D_FORMAT_R8G8B8:
		return 3;
	case WW3D_FORMAT_A8R8G8B8:
	case WW3D_FORMAT_X8R8G8B8:
	case WW3D_FORMAT_X8L8V8U8:
		return 4;
	case WW3D_FORMAT_R5G6B5:
	case WW3D_FORMAT_X1R5G5B5:
	case WW3D_FORMAT_A1R5G5B5:
	case WW3D_FORMAT_A4R4G4B4:
	case WW3D_FORMAT_A8R3G3B2:
	case WW3D_FORMAT_X4R4G4B4:
	case WW3D_FORMAT_A8P8:
	case WW3D_FORMAT_A8L8:
	case WW3D_FORMAT_U8V8:
	case WW3D_FORMAT_L6V5U5:
		return 2;
	case WW3D_FORMAT_R3G3B2:
	case WW3D_FORMAT_A8:
	case WW3D_FORMAT_P8:
	case WW3D_FORMAT_L8:
	case WW3D_FORMAT_A4L4:
		return 1;
	default:
		return 4;
	}
}

bool Is_Compressed_Format(WW3DFormat format)
{
	return format == WW3D_FORMAT_DXT1 || format == WW3D_FORMAT_DXT2 ||
		format == WW3D_FORMAT_DXT3 || format == WW3D_FORMAT_DXT4 ||
		format == WW3D_FORMAT_DXT5;
}

unsigned Compressed_Block_Bytes(WW3DFormat format)
{
	return format == WW3D_FORMAT_DXT1 ? 8u : 16u;
}

unsigned Row_Bytes(WW3DFormat format, unsigned width)
{
	if (Is_Compressed_Format(format))
	{
		return std::max(1u, (width + 3u) / 4u) * Compressed_Block_Bytes(format);
	}
	return width * Format_Bytes_Per_Pixel(format);
}

unsigned Surface_Bytes(WW3DFormat format, unsigned width, unsigned height)
{
	if (Is_Compressed_Format(format))
	{
		return Row_Bytes(format, width) * std::max(1u, (height + 3u) / 4u);
	}
	return Row_Bytes(format, width) * height;
}

WW3DFormat Storage_Color_Format(WW3DFormat format)
{
	// The game stores a number of DX8-era color formats in 16-bit layouts.
	// D3D11 does not guarantee the remaining legacy formats on every
	// feature-level 11 device, and the terrain atlas is written through the
	// lock API. Store those remaining formats as BGRA8 at this backend boundary.
	switch (format)
	{
	case WW3D_FORMAT_A1R5G5B5:
	case WW3D_FORMAT_A4R4G4B4:
	case WW3D_FORMAT_L8:
		return WW3D_FORMAT_A8R8G8B8;
	default:
		return format;
	}
}

DXGI_FORMAT Native_Color_Format(WW3DFormat format)
{
	switch (format)
	{
	case WW3D_FORMAT_A8R8G8B8: return DXGI_FORMAT_B8G8R8A8_UNORM;
	case WW3D_FORMAT_X8R8G8B8: return DXGI_FORMAT_B8G8R8X8_UNORM;
	case WW3D_FORMAT_R5G6B5: return DXGI_FORMAT_B5G6R5_UNORM;
	case WW3D_FORMAT_X1R5G5B5:
	case WW3D_FORMAT_A1R5G5B5: return DXGI_FORMAT_B5G5R5A1_UNORM;
	case WW3D_FORMAT_A4R4G4B4: return DXGI_FORMAT_B4G4R4A4_UNORM;
	case WW3D_FORMAT_A8: return DXGI_FORMAT_A8_UNORM;
	case WW3D_FORMAT_L8: return DXGI_FORMAT_R8_UNORM;
	case WW3D_FORMAT_A8L8: return DXGI_FORMAT_R8G8_UNORM;
	case WW3D_FORMAT_U8V8: return DXGI_FORMAT_R8G8_SNORM;
	case WW3D_FORMAT_DXT1: return DXGI_FORMAT_BC1_UNORM;
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3: return DXGI_FORMAT_BC2_UNORM;
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5: return DXGI_FORMAT_BC3_UNORM;
	default: return DXGI_FORMAT_B8G8R8A8_UNORM;
	}
}

bool Is_Natively_Supported_Format(WW3DFormat format)
{
	switch (format)
	{
	case WW3D_FORMAT_A8R8G8B8:
	case WW3D_FORMAT_X8R8G8B8:
	case WW3D_FORMAT_R5G6B5:
	case WW3D_FORMAT_X1R5G5B5:
	case WW3D_FORMAT_A1R5G5B5:
	case WW3D_FORMAT_A4R4G4B4:
	case WW3D_FORMAT_A8:
	case WW3D_FORMAT_L8:
	case WW3D_FORMAT_A8L8:
	case WW3D_FORMAT_U8V8:
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		return true;
	default:
		return false;
	}
}

DXGI_FORMAT Native_Depth_Format(WW3DZFormat format)
{
	switch (format)
	{
	case WW3D_ZFORMAT_D16_LOCKABLE:
	case WW3D_ZFORMAT_D16: return DXGI_FORMAT_D16_UNORM;
	case WW3D_ZFORMAT_D24S8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case WW3D_ZFORMAT_D32: return DXGI_FORMAT_D32_FLOAT;
	case WW3D_ZFORMAT_D15S1:
	case WW3D_ZFORMAT_D24X8:
	case WW3D_ZFORMAT_D24X4S4:
	default: return DXGI_FORMAT_D24_UNORM_S8_UINT;
	}
}

unsigned Depth_Bytes(WW3DZFormat format)
{
	return format == WW3D_ZFORMAT_D16 || format == WW3D_ZFORMAT_D16_LOCKABLE ||
		format == WW3D_ZFORMAT_D15S1 ? 2u : 4u;
}

DX11Surface::DX11Surface(unsigned width_, unsigned height_, WW3DFormat format_) :
	width(width_), height(height_), format(format_), pitch(Row_Bytes(format_, width_)),
	pixels(Surface_Bytes(format_, width_, height_))
{
}

DX11Surface::~DX11Surface()
{
	Release_Com(owner_context);
	Release_Com(owner_staging);
	Release_Com(owner_resource);
}

DX11Texture::~DX11Texture()
{
	for (DX11TextureLock &lock : active_locks)
	{
		Release_Com(lock.staging);
	}
	Release_Com(depth_stencil_view);
	Release_Com(render_target_depth_stencil_view);
	Release_Com(render_target_depth_resource);
	Release_Com(render_target_view);
	Release_Com(shader_resource_view);
	Release_Com(resource);
	Release_Com(volume_resource);
}

DX11VertexBuffer::~DX11VertexBuffer()
{
	Release_Com(process_unordered_access_view);
	Release_Com(process_shader_resource_view);
	Release_Com(resource);
}

DX11IndexBuffer::~DX11IndexBuffer()
{
	Release_Com(resource);
}

DX11Surface *As_DX11_Surface(RenderBackendSurface *surface)
{
	return static_cast<DX11Surface *>(surface);
}

const DX11Surface *As_DX11_Surface(const RenderBackendSurface *surface)
{
	return static_cast<const DX11Surface *>(surface);
}

DX11Texture *As_DX11_Texture(RenderBackendTextureHandle texture)
{
	return reinterpret_cast<DX11Texture *>(texture);
}

const DX11Texture *As_DX11_Texture_Const(RenderBackendTextureHandle texture)
{
	return reinterpret_cast<const DX11Texture *>(texture);
}

ID3D11Resource *Texture_Resource(DX11Texture *texture)
{
	if (texture == nullptr)
	{
		return nullptr;
	}
	return texture->resource != nullptr ?
		static_cast<ID3D11Resource *>(texture->resource) :
		static_cast<ID3D11Resource *>(texture->volume_resource);
}

const ID3D11Resource *Texture_Resource(const DX11Texture *texture)
{
	if (texture == nullptr)
	{
		return nullptr;
	}
	return texture->resource != nullptr ?
		static_cast<const ID3D11Resource *>(texture->resource) :
		static_cast<const ID3D11Resource *>(texture->volume_resource);
}

unsigned Texture_Subresource(const DX11Texture *texture, unsigned level, unsigned array_slice)
{
	return D3D11CalcSubresource(level, array_slice,
		texture == nullptr ? 1u : texture->levels);
}

unsigned Cube_Face_Index(RenderBackendCubeFace face)
{
	switch (face)
	{
	case RenderBackendCubeFace::PositiveX: return 0;
	case RenderBackendCubeFace::NegativeX: return 1;
	case RenderBackendCubeFace::PositiveY: return 2;
	case RenderBackendCubeFace::NegativeY: return 3;
	case RenderBackendCubeFace::PositiveZ: return 4;
	case RenderBackendCubeFace::NegativeZ: return 5;
	default: return 0;
	}
}

bool Copy_Surface_Contents(const DX11Surface *source, const RenderBackendRect &source_rect,
	DX11Surface *destination, const RenderBackendPoint &destination_point)
{
	if (source == nullptr || destination == nullptr || source_rect.left < 0 || source_rect.top < 0 ||
		source_rect.right <= source_rect.left || source_rect.bottom <= source_rect.top ||
		Is_Compressed_Format(source->format) || Is_Compressed_Format(destination->format))
	{
		return false;
	}
	const unsigned source_bpp = Format_Bytes_Per_Pixel(source->format);
	const unsigned destination_bpp = Format_Bytes_Per_Pixel(destination->format);
	const unsigned copy_width = static_cast<unsigned>(source_rect.right - source_rect.left);
	const unsigned copy_height = static_cast<unsigned>(source_rect.bottom - source_rect.top);
	if (static_cast<unsigned>(source_rect.right) > source->width ||
		static_cast<unsigned>(source_rect.bottom) > source->height || destination_point.x < 0 ||
		destination_point.y < 0 || static_cast<unsigned>(destination_point.x) + copy_width > destination->width ||
		static_cast<unsigned>(destination_point.y) + copy_height > destination->height)
	{
		return false;
	}
	for (unsigned y = 0; y < copy_height; ++y)
	{
		const unsigned char *source_row = source->pixels.data() +
			(static_cast<unsigned>(source_rect.top) + y) * source->pitch +
			static_cast<unsigned>(source_rect.left) * source_bpp;
		unsigned char *destination_row = destination->pixels.data() +
			(static_cast<unsigned>(destination_point.y) + y) * destination->pitch +
			static_cast<unsigned>(destination_point.x) * destination_bpp;
		if (source->format == destination->format)
		{
			std::memcpy(destination_row, source_row, copy_width * source_bpp);
		}
		else
		{
			for (unsigned x = 0; x < copy_width; ++x)
			{
				BitmapHandlerClass::Copy_Pixel(destination_row + x * destination_bpp,
					destination->format, source_row + x * source_bpp, source->format, nullptr, 0);
			}
		}
	}
	return true;
}

bool Commit_Surface(DX11Surface *surface)
{
	if (surface == nullptr || !surface->owner_writeback)
	{
		return true;
	}
	if (surface->owner_context == nullptr || surface->owner_staging == nullptr ||
		surface->owner_resource == nullptr || Is_Compressed_Format(surface->format))
	{
		return false;
	}

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (FAILED(surface->owner_context->Map(surface->owner_staging,
		surface->owner_subresource, D3D11_MAP_WRITE, 0, &mapped)))
	{
		return false;
	}

	const unsigned rows = surface->height;
	const unsigned row_bytes = surface->pitch;
	for (unsigned row = 0; row < rows; ++row)
	{
		std::memcpy(static_cast<unsigned char *>(mapped.pData) + row * mapped.RowPitch,
			surface->pixels.data() + row * surface->pitch,
			std::min(row_bytes, static_cast<unsigned>(mapped.RowPitch)));
	}
	surface->owner_context->Unmap(surface->owner_staging, surface->owner_subresource);
	surface->owner_context->CopySubresourceRegion(surface->owner_resource,
		surface->owner_subresource, 0, 0, 0, surface->owner_staging,
		surface->owner_subresource, nullptr);
	return true;
}
}
