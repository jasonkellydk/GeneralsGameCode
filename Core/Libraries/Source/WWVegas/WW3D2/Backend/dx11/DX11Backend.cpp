/*
** Command & Conquer Generals Zero Hour(tm)
**
** Thin public adapter for the partitioned DX11 runtime.
*/

#include "Backend/dx11/DX11Backend.h"

DX11Backend::DX11Backend() = default;

DX11Backend::~DX11Backend() = default;

DX11Backend *DX11Backend::Create(void *window, bool lite)
{
	DX11Backend *backend = new DX11Backend();
	if (!backend->Initialize(window, lite))
	{
		delete backend;
		return nullptr;
	}
	return backend;
}
