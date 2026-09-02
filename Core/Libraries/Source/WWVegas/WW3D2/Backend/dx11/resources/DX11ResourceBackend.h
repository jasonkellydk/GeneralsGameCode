#pragma once

#include "../core/DX11BackendComponent.h"

namespace dx11_backend
{
	template <typename Host>
	class DX11ResourceBackend;

	template <typename Host>
	class DX11ResourceBackend : public DX11BackendComponent<Host, DX11ResourceBackend<Host>>
	{
	public:
		TextureClass *Create_Render_Target(int width, int height, WW3DFormat format = WW3D_FORMAT_UNKNOWN);
		void Create_Render_Target(int width, int height, WW3DFormat format, WW3DZFormat depth_format, TextureClass **target, ZTextureClass **depth_target);
		void Set_Render_Target(TextureClass *render_target, ZTextureClass *depth_target = nullptr);
		RenderBackendSurface *Create_System_Memory_Surface(unsigned width, unsigned height, WW3DFormat format);
		SurfaceClass *Create_Surface(unsigned width, unsigned height, WW3DFormat format);
		RenderBackendSurface *Create_Surface_From_File(const char *filename);
		bool Get_Surface_Description(RenderBackendSurface *surface, RenderBackendSurfaceDescription &description) const;
		bool Lock_Surface(RenderBackendSurface *surface, RenderBackendLockedSurface &locked_surface, const RenderBackendRect *rect = nullptr, RenderBackendSurfaceLockMode mode = RenderBackendSurfaceLockMode::ReadWrite);
		void Unlock_Surface(RenderBackendSurface *surface);
		void Release_Surface(RenderBackendSurface *surface);
		void Copy_Surface_Rect(RenderBackendSurface *source, const RenderBackendRect &source_rect, SurfaceClass *destination, const RenderBackendPoint &destination_point);
		bool Copy_Surface_Rect(SurfaceClass *source, const RenderBackendRect &source_rect, RenderBackendSurface *destination, const RenderBackendPoint &destination_point);
		bool Copy_Surface(SurfaceClass *source, SurfaceClass *destination);
		bool Copy_Surface_Rect(SurfaceClass *source, const RenderBackendRect &source_rect, SurfaceClass *destination, const RenderBackendPoint &destination_point);
		bool Copy_Surface_Stretch(SurfaceClass *source, const RenderBackendRect &source_rect, SurfaceClass *destination, const RenderBackendRect &destination_rect);
		int Read_Back_Buffer_Rect(void *buffer, int buffer_size, int x, int y, int width, int height);
		RenderBackendTextureHandle Create_Transient_Render_Texture(unsigned width, unsigned height, WW3DFormat format);
		bool Copy_Back_Buffer_To_Texture(RenderBackendTextureHandle texture);
		bool Copy_Texture_To_Surface(RenderBackendTextureHandle texture, SurfaceClass *destination);
		bool Copy_Render_Target_To_Surface(TextureClass *source, SurfaceClass *destination);
		void Release_Transient_Render_Texture(RenderBackendTextureHandle texture);
		RenderBackendTextureHandle Create_Texture_Handle(unsigned width, unsigned height, WW3DFormat format, unsigned mip_levels, bool dynamic, bool render_target = false);
		RenderBackendTextureHandle Create_ZTexture_Handle(unsigned width, unsigned height, WW3DZFormat format, unsigned mip_levels);
		RenderBackendTextureHandle Create_Surface_Handle(unsigned width, unsigned height, WW3DFormat format);
		RenderBackendTextureHandle Create_Surface_Handle(const char *filename);
		RenderBackendTextureHandle Create_Texture_Handle_Pooled(unsigned width, unsigned height, WW3DFormat format, unsigned mip_levels, RenderBackendTexturePool pool, bool render_target);
		RenderBackendTextureHandle Create_ZTexture_Handle_Pooled(unsigned width, unsigned height, WW3DZFormat format, unsigned mip_levels, RenderBackendTexturePool pool);
		RenderBackendTextureHandle Create_Cube_Texture_Handle(unsigned width, unsigned height, WW3DFormat format, unsigned mip_levels, RenderBackendTexturePool pool, bool render_target);
		RenderBackendTextureHandle Create_Volume_Texture_Handle(unsigned width, unsigned height, unsigned depth, WW3DFormat format, unsigned mip_levels, RenderBackendTexturePool pool);
		RenderBackendTextureHandle Create_Texture_From_Surface(RenderBackendSurface *surface, unsigned mip_levels);
		SurfaceClass *Get_Texture_Surface_Level(RenderBackendTextureHandle texture, unsigned level);
		RenderBackendTextureHandle Create_Texture_From_File_Handle(const char *filename, unsigned mip_levels);
		RenderBackendTextureHandle Add_Texture_Reference(RenderBackendTextureHandle texture);
		void Release_Texture_Handle(RenderBackendTextureHandle texture);
		unsigned Get_Texture_Level_Count(RenderBackendTextureHandle texture) const;
		bool Get_Texture_Description(RenderBackendTextureHandle texture, unsigned level, RenderBackendTextureDescription &description) const;
		bool Lock_Texture(RenderBackendTextureHandle texture, unsigned level, RenderBackendTextureLock &locked_texture, bool read_only = false);
		void Unlock_Texture(RenderBackendTextureHandle texture, unsigned level);
		bool Lock_Cube_Texture(RenderBackendTextureHandle texture, RenderBackendCubeFace face, unsigned level, RenderBackendTextureLock &locked_texture, bool read_only = false);
		void Unlock_Cube_Texture(RenderBackendTextureHandle texture, RenderBackendCubeFace face, unsigned level);
		bool Lock_Volume_Texture(RenderBackendTextureHandle texture, unsigned level, RenderBackendTextureLock &locked_texture, bool read_only = false);
		void Unlock_Volume_Texture(RenderBackendTextureHandle texture, unsigned level);
		bool Update_Texture(RenderBackendTextureHandle source, RenderBackendTextureHandle destination);
		bool Generate_Texture_Mipmaps(RenderBackendTextureHandle texture);
		void Set_Texture_LOD(RenderBackendTextureHandle texture, unsigned lod);
		unsigned Get_Texture_Priority(RenderBackendTextureHandle texture) const;
		unsigned Set_Texture_Priority(RenderBackendTextureHandle texture, unsigned priority);
		bool Is_Missing_Texture_Handle(RenderBackendTextureHandle texture) const;
		RenderBackendTextureHandle Create_Missing_Texture();
		RenderBackendSurface *Create_Missing_Surface();
		void Register_Texture(TextureBaseClass *texture, RenderBackendTextureKind kind, unsigned width, unsigned height, unsigned depth, WW3DFormat format, WW3DZFormat depth_format, unsigned mip_levels, bool render_target);
		void Unregister_Texture(TextureBaseClass *texture);
		RenderBackendVertexBuffer *Create_Vertex_Buffer(unsigned size_bytes, const RenderBackendVertexLayout &layout, unsigned usage = BUFFER_USAGE_DEFAULT);
		RenderBackendVertexBuffer *Create_Vertex_Buffer(unsigned size_bytes, RenderBackendVertexFormat format, bool dynamic)
		{
			return Create_Vertex_Buffer(size_bytes, RenderBackend_Vertex_Layout(format),
				dynamic ? BUFFER_USAGE_DYNAMIC : BUFFER_USAGE_DEFAULT);
		}
		RenderBackendIndexBuffer *Create_Index_Buffer(unsigned size_bytes, unsigned usage = BUFFER_USAGE_DEFAULT);
		RenderBackendIndexBuffer *Create_Index_Buffer(unsigned size_bytes, bool dynamic)
		{
			return Create_Index_Buffer(size_bytes,
				dynamic ? BUFFER_USAGE_DYNAMIC : BUFFER_USAGE_DEFAULT);
		}
		bool Lock_Vertex_Buffer(RenderBackendVertexBuffer *buffer, unsigned offset_bytes, unsigned size_bytes, void **data, RenderBackendBufferLockMode mode);
		bool Lock_Index_Buffer(RenderBackendIndexBuffer *buffer, unsigned offset_bytes, unsigned size_bytes, void **data, RenderBackendBufferLockMode mode);
		void Unlock_Vertex_Buffer(RenderBackendVertexBuffer *buffer);
		void Unlock_Index_Buffer(RenderBackendIndexBuffer *buffer);
		void Release_Vertex_Buffer(RenderBackendVertexBuffer *buffer);
		void Release_Index_Buffer(RenderBackendIndexBuffer *buffer);
	};
}
