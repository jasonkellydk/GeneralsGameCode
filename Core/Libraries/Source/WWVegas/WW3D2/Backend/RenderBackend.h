/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "IRenderBackend.h"
#include "dx11/DX11Backend.h"

// This is the only active-backend binding. IRenderBackend.h remains API
// neutral; this header is the modern GenMD build's concrete selection point.
using IRenderBackend = IRenderBackendType<DX11Backend>;

using RenderBackendVertexBufferLock =
	RenderBackendVertexBufferLockT<IRenderBackend>;
using RenderBackendIndexBufferLock =
	RenderBackendIndexBufferLockT<IRenderBackend>;

template <typename BackendT>
inline BackendT *Create_Render_Backend_Instance(void * window, bool lite)
{
	return BackendT::Create(window, lite);
}

inline IRenderBackend *Create_Render_Backend(void * window, bool lite)
{
	return Create_Render_Backend_Instance<IRenderBackend>(window, lite);
}
