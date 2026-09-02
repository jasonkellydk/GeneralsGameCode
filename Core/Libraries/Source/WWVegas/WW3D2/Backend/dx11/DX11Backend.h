/*
** Command & Conquer Generals Zero Hour(tm)
**
** Public DX11 backend entry point. The implementation is partitioned below
** this file; WW3D2 only sees the backend contract through RenderBackend.h.
*/

#pragma once

#include "core/DX11BackendRuntime.h"

// Keep the public backend name stable while the implementation is split into
// device, state, resource, draw, and shader subsystems under dx11/.
class DX11Backend final : public DX11BackendRuntime
{
public:
	DX11Backend();
	~DX11Backend();

	static DX11Backend *Create(void *window, bool lite);
};
