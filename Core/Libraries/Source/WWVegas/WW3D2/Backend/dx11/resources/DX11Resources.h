/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2025 Electronic Arts Inc.
**
** D3D11-owned WW3D2 resource objects. The public backend contract only sees
** the opaque RenderBackend* types declared by the neutral backend contract.
*/

#pragma once

#include <d3d11.h>

#include <vector>

#include "../../RenderBackendTypes.h"

namespace dx11_backend
{
	unsigned Format_Bytes_Per_Pixel(WW3DFormat format);
	bool Is_Compressed_Format(WW3DFormat format);
	unsigned Compressed_Block_Bytes(WW3DFormat format);
	unsigned Row_Bytes(WW3DFormat format, unsigned width);
	unsigned Surface_Bytes(WW3DFormat format, unsigned width, unsigned height);
	WW3DFormat Storage_Color_Format(WW3DFormat format);
	DXGI_FORMAT Native_Color_Format(WW3DFormat format);
	bool Is_Natively_Supported_Format(WW3DFormat format);
	DXGI_FORMAT Native_Depth_Format(WW3DZFormat format);
	unsigned Depth_Bytes(WW3DZFormat format);

	struct DX11Surface final : RenderBackendSurface
	{
		unsigned width;
		unsigned height;
		WW3DFormat format;
		unsigned pitch;
		std::vector<unsigned char> pixels;
		bool locked = false;

		// A surface returned from a texture subresource owns a CPU staging copy.
		// Unlocking it commits the copy back to the original subresource.
		ID3D11Texture2D *owner_resource = nullptr;
		ID3D11Texture2D *owner_staging = nullptr;
		ID3D11DeviceContext *owner_context = nullptr;
		unsigned owner_subresource = 0;
		bool owner_writeback = false;

		DX11Surface(unsigned width_, unsigned height_, WW3DFormat format_);
		~DX11Surface() override;
	};

	struct DX11TextureLock final
	{
		unsigned level = 0;
		unsigned subresource = 0;
		ID3D11Resource *staging = nullptr;
		D3D11_MAPPED_SUBRESOURCE mapped{};
		bool read_only = false;
	};

	struct DX11Texture final
	{
		unsigned references = 1;
		unsigned width = 0;
		unsigned height = 0;
		unsigned depth = 1;
		unsigned array_size = 1;
		unsigned levels = 1;
		unsigned lod = 0;
		unsigned priority = 0;
		WW3DFormat format = WW3D_FORMAT_UNKNOWN;
		WW3DZFormat depth_format = WW3D_ZFORMAT_UNKNOWN;
		RenderBackendTextureKind kind = RenderBackendTextureKind::Texture2D;
		DXGI_FORMAT native_format = DXGI_FORMAT_UNKNOWN;
		bool render_target = false;
		bool is_cube = false;
		bool missing = false;
		ID3D11Texture2D *resource = nullptr;
		ID3D11Texture3D *volume_resource = nullptr;
		ID3D11ShaderResourceView *shader_resource_view = nullptr;
		ID3D11RenderTargetView *render_target_view = nullptr;
		// Off-screen targets need a depth/stencil surface with matching
		// dimensions; the swap-chain DSV cannot be used for them.
		ID3D11Texture2D *render_target_depth_resource = nullptr;
		ID3D11DepthStencilView *render_target_depth_stencil_view = nullptr;
		ID3D11DepthStencilView *depth_stencil_view = nullptr;
		std::vector<DX11TextureLock> active_locks;

		~DX11Texture();
	};

	struct DX11TextureRegistration final
	{
		TextureBaseClass *texture = nullptr;
		RenderBackendTextureKind kind = RenderBackendTextureKind::Texture2D;
		unsigned width = 0;
		unsigned height = 0;
		unsigned depth = 1;
		WW3DFormat format = WW3D_FORMAT_UNKNOWN;
		WW3DZFormat depth_format = WW3D_ZFORMAT_UNKNOWN;
		unsigned mip_levels = 1;
		bool render_target = false;
	};

	struct DX11VertexBuffer final : RenderBackendVertexBuffer
	{
		ID3D11Buffer *resource = nullptr;
		unsigned size = 0;
		RenderBackendVertexLayout layout;
		bool dynamic = false;
		bool locked = false;
	bool gpu_mapped = false;
	unsigned mapped_offset = 0;
	unsigned mapped_size = 0;
	std::vector<unsigned char> shadow;
	D3D11_MAPPED_SUBRESOURCE mapped{};
	ID3D11ShaderResourceView *process_shader_resource_view = nullptr;
	ID3D11UnorderedAccessView *process_unordered_access_view = nullptr;

	~DX11VertexBuffer() override;
	};

	struct DX11IndexBuffer final : RenderBackendIndexBuffer
	{
		ID3D11Buffer *resource = nullptr;
		unsigned size = 0;
		bool dynamic = false;
		bool locked = false;
		bool gpu_mapped = false;
		unsigned mapped_offset = 0;
		unsigned mapped_size = 0;
		std::vector<unsigned char> shadow;
		D3D11_MAPPED_SUBRESOURCE mapped{};

		~DX11IndexBuffer() override;
	};

	DX11Surface *As_DX11_Surface(RenderBackendSurface *surface);
	const DX11Surface *As_DX11_Surface(const RenderBackendSurface *surface);
	DX11Texture *As_DX11_Texture(RenderBackendTextureHandle texture);
	const DX11Texture *As_DX11_Texture_Const(RenderBackendTextureHandle texture);
	ID3D11Resource *Texture_Resource(DX11Texture *texture);
	const ID3D11Resource *Texture_Resource(const DX11Texture *texture);
	unsigned Texture_Subresource(const DX11Texture *texture, unsigned level, unsigned array_slice = 0);
	unsigned Cube_Face_Index(RenderBackendCubeFace face);

	bool Copy_Surface_Contents(const DX11Surface *source, const RenderBackendRect &source_rect,
		DX11Surface *destination, const RenderBackendPoint &destination_point);
	bool Commit_Surface(DX11Surface *surface);
}
