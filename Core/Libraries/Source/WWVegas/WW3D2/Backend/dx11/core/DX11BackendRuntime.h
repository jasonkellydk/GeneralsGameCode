#pragma once

#include <memory>

#include "DX11BackendComponent.h"
#include "../device/DX11DeviceBackend.h"
#include "../state/DX11RenderStateBackend.h"
#include "../resources/DX11ResourceBackend.h"
#include "../draw/DX11DrawBackend.h"
#include "../shaders/DX11ShaderBackend.h"

// The runtime is the compile-time composition root. Each responsibility is a
// CRTP subsystem; this class owns only the shared state and exposes no native
// API implementation of its own.
class DX11BackendRuntime
	: public dx11_backend::DX11DeviceBackend<DX11BackendRuntime>
	, public dx11_backend::DX11RenderStateBackend<DX11BackendRuntime>
	, public dx11_backend::DX11ResourceBackend<DX11BackendRuntime>
	, public dx11_backend::DX11DrawBackend<DX11BackendRuntime>
	, public dx11_backend::DX11ShaderBackend<DX11BackendRuntime>
{
public:
	DX11BackendRuntime();
	~DX11BackendRuntime();

	static DX11BackendRuntime *Create(void *window, bool lite);

protected:
	dx11_backend::DX11BackendState &Backend_State() noexcept { return *state; }
	const dx11_backend::DX11BackendState &Backend_State() const noexcept { return *state; }

	template <typename, typename>
	friend class dx11_backend::DX11BackendComponent;

private:
	std::unique_ptr<dx11_backend::DX11BackendState> state;
};
