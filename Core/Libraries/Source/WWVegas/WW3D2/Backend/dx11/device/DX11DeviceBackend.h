#pragma once

#include "../core/DX11BackendComponent.h"

namespace dx11_backend
{
	template <typename Host>
	class DX11DeviceBackend;

	template <typename Host>
	class DX11DeviceBackend : public DX11BackendComponent<Host, DX11DeviceBackend<Host>>
	{
	public:
		bool Initialize(void *window, bool lite);
		void Shutdown();

		bool Is_Initted() const;
		bool Is_Render_To_Texture() const;
		bool Has_Stencil() const;
		bool Supports_TnL() const;
		bool Supports_DXTC() const;
		bool Supports_NPatches() const;
		bool Supports_Bump_Envmap() const;
		bool Supports_Bump_Envmap_Luminance() const;
		bool Supports_Z_Bias() const;
		bool Supports_Anisotropic_Filtering() const;
		bool Supports_Modulate_Alpha_Add_Color() const;
		bool Supports_Dot3() const;
		bool Supports_Point_Sprites() const;
		bool Supports_Cubemaps() const;
		bool Supports_Color_Write_Mask() const;
		bool Supports_Texture_Operation(RenderBackendTextureOperation operation) const;
		bool Supports_Texture_Filter(RenderBackendTextureFilterType type, RenderBackendTextureFilter filter) const;
		bool Is_Fog_Allowed() const;
		bool Is_Fog_Enabled() const;
		unsigned Get_Fog_Color() const;
		bool Supports_Texture_Format(WW3DFormat format) const;
		bool Supports_Render_To_Texture_Format(WW3DFormat format) const;
		bool Supports_Depth_Stencil_Format(WW3DZFormat format) const;
		WW3DFormat Get_Back_Buffer_Format() const;
		SurfaceClass *Get_Back_Buffer_Surface();
		RenderBackendDeviceStatus Get_Device_Status() const;
		bool Is_Device_Ready() const;
		bool Is_Render_Thread() const;
		bool Get_Adapter_Info(RenderBackendAdapterInfo &info) const;
		bool Get_Texture_Limits(RenderBackendTextureLimits &limits) const;
		int Get_Max_Textures_Per_Pass() const;
		int Get_Pixel_Shader_Major_Version() const;
		int Get_Pixel_Shader_Minor_Version() const;
		bool Is_3DFX_Voodoo3() const;
		unsigned Pack_Color(const Vector4 &color) const;
		unsigned Pack_Color(const Vector3 &color, float alpha) const;
		unsigned Pack_Color_Clamped(const Vector4 &color) const;
		Vector4 Unpack_Color(unsigned color) const;
		bool Is_Triangle_Draw_Enabled() const;
		void Set_Triangle_Draw_Enabled(bool enable);
		RenderBackendDebugSettings &Get_Debug_Settings();
		void Set_Cleanup_Hook(RenderBackendCleanupHook *hook);
		void Invalidate_Renderer_Caches();
		RenderBackendFont *Create_Font(int height, const char *face_name, bool bold = false, int width = 0);
		void Release_Font(RenderBackendFont *font);
		bool Get_Font_Metrics(RenderBackendFont *font, RenderBackendFontMetrics &metrics) const;
		bool Get_Font_Glyph(RenderBackendFont *font, unsigned int character, RenderBackendFontGlyph &glyph);
		void Draw_Font(RenderBackendFont *font, const char *text, unsigned text_length, const RenderBackendRect &rect, unsigned flags, unsigned color);
		bool Initialize_Browser(const char *bad_page_url = nullptr, const char *loading_page_url = nullptr, const char *mouse_filename = nullptr, const char *mouse_busy_filename = nullptr);
		void Shutdown_Browser();
		void Update_Browser();
		void Render_Browser(int backbuffer_index);
		void Create_Browser(const char *browser_name, const char *url, int x, int y, int width, int height, int update_ticks = 0, unsigned options = RenderBackendBrowserOptionScrollbars | RenderBackendBrowserOption3DBorder, void *game_dispatch = nullptr);
		void Destroy_Browser(const char *browser_name);
		bool Is_Browser_Open(const char *browser_name) const;
		void Navigate_Browser(const char *browser_name, const char *url);
		bool Set_Render_Device(const char *device_name, int width, int height, int bits, int windowed, bool resize_window);
		bool Set_Render_Device(int device, int width, int height, int bits, int windowed, bool resize_window, bool reset_device, bool restore_assets);
		void Set_Fullscreen_Mode(RenderBackendFullscreenMode mode);
		RenderBackendFullscreenMode Get_Fullscreen_Mode() const;
		bool Set_Any_Render_Device();
		bool Set_Next_Render_Device();
		bool Is_Windowed() const;
		bool Toggle_Windowed();
		int Get_Render_Device() const;
		const RenderDeviceDescClass &Get_Render_Device_Desc(int device) const;
		int Get_Render_Device_Count() const;
		const char *Get_Render_Device_Name(int device) const;
		bool Set_Device_Resolution(int width, int height, int bits, int windowed, bool resize_window);
		void Get_Device_Resolution(int &width, int &height, int &bits, bool &windowed) const;
		void Get_Render_Target_Resolution(int &width, int &height, int &bits, bool &windowed) const;
		int Get_Device_Resolution_Width() const;
		int Get_Device_Resolution_Height() const;
		void Set_Swap_Interval(int swap);
		int Get_Swap_Interval() const;
		bool Reset_Device(bool reload_assets = true);
		bool Registry_Save_Render_Device(const char *sub_key);
		bool Registry_Save_Render_Device(const char *sub_key, int device, int width, int height, int depth, bool windowed, int texture_depth);
		bool Registry_Load_Render_Device(const char *sub_key, bool resize_window);
		bool Registry_Load_Render_Device(const char *sub_key, char *device, int device_len, int &width, int &height, int &depth, int &windowed, int &texture_depth);
		void Set_Texture_Bitdepth(int depth);
		int Get_Texture_Bitdepth() const;
		void Set_Multisample_Mode(RenderBackendMultisampleMode mode);
		RenderBackendMultisampleMode Get_Multisample_Mode() const;
		void Set_Gamma(float gamma, float bright, float contrast, bool calibrate = true, bool uselimit = true);
		void Begin_Scene();
		void End_Scene(bool flip_frame = true);
		void Flip_To_Primary();
		void Clear(bool clear_color, bool clear_z_stencil, const Vector3 &color, float dest_alpha = 0.0f, float z = 1.0f, unsigned int stencil = 0);
		void Set_Viewport(const RenderBackendViewport &viewport);
		bool Get_Viewport(RenderBackendViewport &viewport) const;
		void Get_Render_Target(RenderBackendRenderTargetState &target) const;
		void Show_Cursor(bool show);
		bool Set_Cursor_Properties(int hotspot_x, int hotspot_y, SurfaceClass *surface);
		void Set_Cursor_Position(int x, int y);
	};
}
