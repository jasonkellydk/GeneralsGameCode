#pragma once

#include "../RenderBackendTypes.h"

namespace dx11_backend
{
	struct DX11BackendState;

	// CRTP access point shared by the backend subsystems. There is no virtual
	// dispatch and no runtime backend interface hidden behind this type.
	template <typename Host, typename Owner>
	class DX11BackendComponent
	{
	protected:
		Host &Backend() noexcept
		{
			return *static_cast<Host *>(static_cast<Owner *>(this));
		}

		const Host &Backend() const noexcept
		{
			return *static_cast<const Host *>(static_cast<const Owner *>(this));
		}

		DX11BackendState &State() noexcept
		{
			return Backend().Backend_State();
		}

		const DX11BackendState &State() const noexcept
		{
			return Backend().Backend_State();
		}
	};
}
