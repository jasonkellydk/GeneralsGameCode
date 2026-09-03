module;

#include <cstdint>

export module Graphics.RHI.DX11.Coexistence;

export import Graphics.RHI.DX11;
export import Graphics.RHI.Frame;

namespace Graphics
{

export extern "C" bool Graphics_DX11_Initialize_Shared_Frame(
	void *device,
	void *context,
	void *swap_chain,
	void *back_buffer,
	void *back_buffer_view,
	void *depth_buffer,
	void *depth_buffer_view,
	std::uint32_t width,
	std::uint32_t height);

export extern "C" bool Graphics_DX11_Update_Shared_Frame(
	void *device,
	void *context,
	void *swap_chain,
	void *back_buffer,
	void *back_buffer_view,
	void *depth_buffer,
	void *depth_buffer_view,
	std::uint32_t width,
	std::uint32_t height);

export extern "C" bool Graphics_DX11_Begin_Frame() noexcept;
export extern "C" bool Graphics_DX11_Begin_Modern_Phase() noexcept;
export extern "C" bool Graphics_DX11_Set_Modern_View(
	const float *view_projection,
	const float *camera_right,
	const float *camera_up,
	const float *camera_forward) noexcept;
export extern "C" bool Graphics_DX11_End_Frame() noexcept;
export extern "C" bool Graphics_DX11_Present() noexcept;
export extern "C" void Graphics_DX11_Abort_Frame() noexcept;
export extern "C" std::uint32_t Graphics_DX11_Frame_Invalid_Operation_Count() noexcept;
export extern "C" void Graphics_DX11_Shutdown_Shared_Frame() noexcept;

}
