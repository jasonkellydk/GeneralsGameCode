/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
**
** API-neutral compile-time backend utilities for WW3D2.  A concrete backend
** is selected outside this header by the active backend binding.
*/

#pragma once

#include "RenderBackendTypes.h"

// This is deliberately only a type transformation. It introduces no
// virtual dispatch and does not know which graphics API supplies BackendT.
template <typename BackendT>
using IRenderBackendType = BackendT;

template <typename BackendT>
class RenderBackendVertexBufferLockT
{
public:
	RenderBackendVertexBufferLockT(BackendT * backend,
		RenderBackendVertexBuffer * buffer, unsigned offset_bytes,
		unsigned size_bytes, RenderBackendBufferLockMode mode) :
		Backend(backend), Buffer(buffer), Data(nullptr), Locked(false)
	{
		if (Backend != nullptr && Buffer != nullptr)
		{
			Locked = Backend->Lock_Vertex_Buffer(Buffer, offset_bytes, size_bytes,
				&Data, mode);
		}
	}

	~RenderBackendVertexBufferLockT()
	{
		if (Locked)
		{
			Backend->Unlock_Vertex_Buffer(Buffer);
		}
	}

	void * Get_Data() const { return Data; }
	bool Is_Locked() const { return Locked; }

private:
	BackendT * Backend;
	RenderBackendVertexBuffer * Buffer;
	void * Data;
	bool Locked;
};

template <typename BackendT>
class RenderBackendIndexBufferLockT
{
public:
	RenderBackendIndexBufferLockT(BackendT * backend,
		RenderBackendIndexBuffer * buffer, unsigned offset_bytes,
		unsigned size_bytes, RenderBackendBufferLockMode mode) :
		Backend(backend), Buffer(buffer), Data(nullptr), Locked(false)
	{
		if (Backend != nullptr && Buffer != nullptr)
		{
			Locked = Backend->Lock_Index_Buffer(Buffer, offset_bytes, size_bytes,
				&Data, mode);
		}
	}

	~RenderBackendIndexBufferLockT()
	{
		if (Locked)
		{
			Backend->Unlock_Index_Buffer(Buffer);
		}
	}

	void * Get_Data() const { return Data; }
	bool Is_Locked() const { return Locked; }

private:
	BackendT * Backend;
	RenderBackendIndexBuffer * Buffer;
	void * Data;
	bool Locked;
};

template <typename BackendT>
inline void RenderBackend_Release_Vertex_Buffer(BackendT * backend,
	RenderBackendVertexBuffer *& buffer)
{
	if (buffer != nullptr)
	{
		if (backend != nullptr)
		{
			backend->Release_Vertex_Buffer(buffer);
		}
		buffer = nullptr;
	}
}

template <typename BackendT>
inline void RenderBackend_Release_Index_Buffer(BackendT * backend,
	RenderBackendIndexBuffer *& buffer)
{
	if (buffer != nullptr)
	{
		if (backend != nullptr)
		{
			backend->Release_Index_Buffer(buffer);
		}
		buffer = nullptr;
	}
}
