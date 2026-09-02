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

// Explicit pass scope for off-screen rendering.  It owns only neutral WW3D2
// state: attachments, viewport, and winding.  It never begins a frame,
// presents the swap chain, or invokes a legacy ShaderClass global.
class RenderBackendPassScope final
{
public:
	RenderBackendPassScope(IRenderBackend *backend, TextureBaseClass *color_target,
		ZTextureClass *depth_target, RenderBackendCullMode cull_mode,
		bool clear_color, bool clear_depth, const Vector3 &clear_value) :
		m_backend(backend),
		m_active(false),
		m_cull_override_active(false),
		m_saved_target(),
		m_saved_viewport(),
		m_pass_viewport()
	{
		if (m_backend == nullptr)
		{
			return;
		}

		m_backend->Get_Render_Target(m_saved_target);
		m_backend->Get_Viewport(m_saved_viewport);
		m_backend->Set_Render_Target(color_target, depth_target);

		RenderBackendRenderTargetState installed_target;
		m_backend->Get_Render_Target(installed_target);
		if (installed_target.color != color_target ||
			installed_target.depth != depth_target)
		{
			m_backend->Set_Render_Target(m_saved_target.color,
				m_saved_target.depth);
			m_backend->Set_Viewport(m_saved_viewport);
			return;
		}

		m_backend->Get_Viewport(m_pass_viewport);
		m_backend->Clear(clear_color, clear_depth, clear_value);
		m_backend->Push_Cull_Mode_Override(cull_mode);
		m_cull_override_active = true;
		m_active = true;
	}

	~RenderBackendPassScope()
	{
		End();
	}

	RenderBackendPassScope(const RenderBackendPassScope &) = delete;
	RenderBackendPassScope &operator=(const RenderBackendPassScope &) = delete;

	bool Is_Active() const
	{
		return m_active;
	}

	const RenderBackendViewport &Get_Pass_Viewport() const
	{
		return m_pass_viewport;
	}

	void End()
	{
		if (!m_active)
		{
			return;
		}

		if (m_cull_override_active)
		{
			m_backend->Pop_Cull_Mode_Override();
			m_cull_override_active = false;
		}
		m_backend->Set_Render_Target(m_saved_target.color,
			m_saved_target.depth);
		m_backend->Set_Viewport(m_saved_viewport);
		m_active = false;
	}

private:
	IRenderBackend *m_backend;
	bool m_active;
	bool m_cull_override_active;
	RenderBackendRenderTargetState m_saved_target;
	RenderBackendViewport m_saved_viewport;
	RenderBackendViewport m_pass_viewport;
};

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
