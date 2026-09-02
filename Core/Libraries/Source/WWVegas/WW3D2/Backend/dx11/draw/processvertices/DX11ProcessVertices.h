#pragma once

#include <d3d11.h>

class VertexBufferClass;

namespace dx11_backend
{
	struct DX11BackendState;

	class DX11ProcessVertices final
	{
	public:
		DX11ProcessVertices() = default;
		DX11ProcessVertices(const DX11ProcessVertices &) = delete;
		DX11ProcessVertices &operator=(const DX11ProcessVertices &) = delete;

		~DX11ProcessVertices();

		bool Initialize(ID3D11Device *device);
		void Release();
		bool Process(DX11BackendState &state, VertexBufferClass *destination,
			unsigned vertex_count);

	private:
		ID3D11ComputeShader *shader = nullptr;
		ID3D11Buffer *constant_buffer = nullptr;
	};
}
