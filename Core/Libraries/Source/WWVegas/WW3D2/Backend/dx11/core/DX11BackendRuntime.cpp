/* DX11 runtime lifetime and factory. */
#include "Backend/dx11/core/DX11BackendRuntime.h"
#include "Backend/dx11/core/DX11BackendInternals.h"

DX11BackendRuntime::DX11BackendRuntime() : state(new dx11_backend::DX11BackendState())
{
}

DX11BackendRuntime::~DX11BackendRuntime()
{
	Shutdown();
}

DX11BackendRuntime *DX11BackendRuntime::Create(void *window, bool lite)
{
	DX11BackendRuntime *backend = new DX11BackendRuntime();
	if (!backend->Initialize(window, lite))
	{
		delete backend;
		return nullptr;
	}
	return backend;
}
