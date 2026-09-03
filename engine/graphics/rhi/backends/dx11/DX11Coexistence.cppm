module;

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
	unsigned int width,
	unsigned int height);

export extern "C" bool Graphics_DX11_Update_Shared_Frame(
	void *device,
	void *context,
	void *swap_chain,
	void *back_buffer,
	void *back_buffer_view,
	void *depth_buffer,
	void *depth_buffer_view,
	unsigned int width,
	unsigned int height);

export extern "C" bool Graphics_DX11_Begin_Frame() noexcept;
export extern "C" bool Graphics_DX11_Begin_Modern_Phase() noexcept;
export extern "C" bool Graphics_DX11_End_Frame() noexcept;
export extern "C" bool Graphics_DX11_Present() noexcept;
export extern "C" void Graphics_DX11_Abort_Frame() noexcept;
export extern "C" unsigned int Graphics_DX11_Frame_Invalid_Operation_Count() noexcept;
export extern "C" void Graphics_DX11_Shutdown_Shared_Frame() noexcept;

}
