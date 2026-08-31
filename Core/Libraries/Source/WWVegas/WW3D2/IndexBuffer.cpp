/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "IndexBuffer.h"

#include "WW3D.h"
#include "Texture.h"
#include "WWDebug/wwmemlog.h"

#define DEFAULT_IB_SIZE 5000

static bool _DynamicSortingIndexArrayInUse = false;
static SortingIndexBufferClass *_DynamicSortingIndexArray = nullptr;
static unsigned short _DynamicSortingIndexArraySize = 0;
static unsigned short _DynamicSortingIndexArrayOffset = 0;

static bool _DynamicRenderIndexBufferInUse = false;
static IndexBufferClass *_DynamicRenderIndexBuffer = nullptr;
static unsigned short _DynamicRenderIndexBufferSize = DEFAULT_IB_SIZE;
static unsigned short _DynamicRenderIndexBufferOffset = 0;

static int _IndexBufferCount;
static int _IndexBufferTotalIndices;
static int _IndexBufferTotalSize;

IndexBufferClass::IndexBufferClass(unsigned short index_count, UsageType usage) :
	IndexBufferClass(BUFFER_TYPE_RENDER, index_count, usage)
{
}

IndexBufferClass::IndexBufferClass(unsigned type_, unsigned short index_count_,
	UsageType usage) :
	type(type_),
	index_count(index_count_),
	engine_refs(0),
	Backend(nullptr),
	BackendBuffer(nullptr)
{
	WWASSERT(index_count);
	WWASSERT(type == BUFFER_TYPE_RENDER || type == BUFFER_TYPE_SORTING);

	if (type == BUFFER_TYPE_RENDER)
	{
		Backend = WW3D::Get_Render_Backend();
		if (Backend != nullptr)
		{
			BackendBuffer = Backend->Create_Index_Buffer(
				index_count * sizeof(unsigned short), static_cast<unsigned>(usage));
			if (BackendBuffer == nullptr)
			{
				TextureClass::Invalidate_Old_Unused_Textures(5000);
				WW3D::_Invalidate_Mesh_Cache();
				BackendBuffer = Backend->Create_Index_Buffer(
					index_count * sizeof(unsigned short), static_cast<unsigned>(usage));
			}
		}
	}

	WWASSERT(type == BUFFER_TYPE_SORTING || BackendBuffer != nullptr);
	_IndexBufferCount++;
	_IndexBufferTotalIndices += index_count;
	_IndexBufferTotalSize += index_count * sizeof(unsigned short);
}

IndexBufferClass::~IndexBufferClass()
{
	if (BackendBuffer != nullptr && Backend != nullptr)
	{
		Backend->Release_Index_Buffer(BackendBuffer);
		BackendBuffer = nullptr;
	}

	_IndexBufferCount--;
	_IndexBufferTotalIndices -= index_count;
	_IndexBufferTotalSize -= index_count * sizeof(unsigned short);
}

unsigned IndexBufferClass::Get_Total_Buffer_Count()
{
	return _IndexBufferCount;
}

unsigned IndexBufferClass::Get_Total_Allocated_Indices()
{
	return _IndexBufferTotalIndices;
}

unsigned IndexBufferClass::Get_Total_Allocated_Memory()
{
	return _IndexBufferTotalSize;
}

void IndexBufferClass::Add_Engine_Ref() const
{
	engine_refs++;
}

void IndexBufferClass::Release_Engine_Ref() const
{
	engine_refs--;
	WWASSERT(engine_refs >= 0);
}

void IndexBufferClass::Copy(unsigned int *indices, unsigned first_index,
	unsigned count)
{
	WWASSERT(indices);
	WWASSERT(first_index + count <= index_count);
	AppendLockClass lock(this, first_index, count);
	if (!lock.Is_Locked()) return;
	unsigned short *destination = lock.Get_Index_Array();
	for (unsigned index = 0; index < count; ++index)
	{
		destination[index] = static_cast<unsigned short>(indices[index]);
	}
}

void IndexBufferClass::Copy(unsigned short *indices, unsigned first_index,
	unsigned count)
{
	WWASSERT(indices);
	WWASSERT(first_index + count <= index_count);
	AppendLockClass lock(this, first_index, count);
	if (!lock.Is_Locked()) return;
	unsigned short *destination = lock.Get_Index_Array();
	for (unsigned index = 0; index < count; ++index)
	{
		destination[index] = indices[index];
	}
}

IndexBufferClass::WriteLockClass::WriteLockClass(IndexBufferClass *index_buffer_,
	RenderBackendBufferLockMode mode) :
	index_buffer(index_buffer_),
	indices(nullptr),
	locked(false)
{
	WWASSERT(index_buffer != nullptr);
	WWASSERT(index_buffer == nullptr || !index_buffer->Engine_Refs());
	if (index_buffer == nullptr)
	{
		return;
	}

	index_buffer->Add_Ref();
	if (index_buffer->Type() == BUFFER_TYPE_RENDER)
	{
		locked = index_buffer->Lock_Backend_Buffer(
				0,
				index_buffer->Get_Index_Count() * sizeof(unsigned short),
				reinterpret_cast<void **>(&indices), mode);
	}
	else if (index_buffer->Type() == BUFFER_TYPE_SORTING)
	{
		indices = static_cast<SortingIndexBufferClass *>(index_buffer)->Get_Index_Array();
		locked = indices != nullptr;
	}
	else
	{
		WWASSERT(false);
	}
}

IndexBufferClass::WriteLockClass::~WriteLockClass()
{
	if (locked && index_buffer != nullptr && index_buffer->Type() == BUFFER_TYPE_RENDER &&
		index_buffer->Get_Backend() != nullptr)
	{
		index_buffer->Unlock_Backend_Buffer();
	}
	if (index_buffer != nullptr)
	{
		index_buffer->Release_Ref();
	}
}

IndexBufferClass::AppendLockClass::AppendLockClass(IndexBufferClass *index_buffer_,
	unsigned start_index, unsigned index_range) :
	index_buffer(index_buffer_),
	indices(nullptr),
	locked(false)
{
	WWASSERT(index_buffer != nullptr);
	WWASSERT(index_buffer == nullptr || !index_buffer->Engine_Refs());
	WWASSERT(index_buffer == nullptr || start_index + index_range <= index_buffer->Get_Index_Count());
	if (index_buffer == nullptr)
	{
		return;
	}

	index_buffer->Add_Ref();
	if (index_buffer->Type() == BUFFER_TYPE_RENDER)
	{
		locked = index_buffer->Lock_Backend_Buffer(
				start_index * sizeof(unsigned short),
				index_range * sizeof(unsigned short),
				reinterpret_cast<void **>(&indices),
				RenderBackendBufferLockMode::Normal);
	}
	else if (index_buffer->Type() == BUFFER_TYPE_SORTING)
	{
		indices = static_cast<SortingIndexBufferClass *>(index_buffer)->Get_Index_Array() +
			start_index;
		locked = indices != nullptr;
	}
	else
	{
		WWASSERT(false);
	}
}

IndexBufferClass::AppendLockClass::~AppendLockClass()
{
	if (locked && index_buffer != nullptr && index_buffer->Type() == BUFFER_TYPE_RENDER &&
		index_buffer->Get_Backend() != nullptr)
	{
		index_buffer->Unlock_Backend_Buffer();
	}
	if (index_buffer != nullptr)
	{
		index_buffer->Release_Ref();
	}
}

SortingIndexBufferClass::SortingIndexBufferClass(unsigned short index_count) :
	IndexBufferClass(BUFFER_TYPE_SORTING, index_count, USAGE_DEFAULT),
	index_buffer(nullptr)
{
	WWMEMLOG(MEM_RENDERER);
	index_buffer = W3DNEWARRAY unsigned short[index_count];
}

SortingIndexBufferClass::~SortingIndexBufferClass()
{
	delete[] index_buffer;
}

DynamicIBAccessClass::DynamicIBAccessClass(unsigned type_,
	unsigned short index_count_) :
	TypeValue(type_),
	IndexCount(index_count_),
	IndexBufferOffset(0),
	IndexBuffer(nullptr)
{
	WWASSERT(TypeValue == BUFFER_TYPE_DYNAMIC_RENDER ||
		TypeValue == BUFFER_TYPE_DYNAMIC_SORTING);
	if (TypeValue == BUFFER_TYPE_DYNAMIC_RENDER)
	{
		Allocate_Render_Dynamic_Buffer();
	}
	else
	{
		Allocate_Sorting_Dynamic_Buffer();
	}
}

DynamicIBAccessClass::~DynamicIBAccessClass()
{
	if (TypeValue == BUFFER_TYPE_DYNAMIC_RENDER)
	{
		_DynamicRenderIndexBufferInUse = false;
		_DynamicRenderIndexBufferOffset += IndexCount;
	}
	else
	{
		_DynamicSortingIndexArrayInUse = false;
		_DynamicSortingIndexArrayOffset += IndexCount;
	}
	REF_PTR_RELEASE(IndexBuffer);
}

void DynamicIBAccessClass::_Deinit()
{
	WWASSERT((_DynamicRenderIndexBuffer == nullptr) ||
		(_DynamicRenderIndexBuffer->Num_Refs() == 1));
	REF_PTR_RELEASE(_DynamicRenderIndexBuffer);
	_DynamicRenderIndexBufferInUse = false;
	_DynamicRenderIndexBufferSize = DEFAULT_IB_SIZE;
	_DynamicRenderIndexBufferOffset = 0;

	WWASSERT((_DynamicSortingIndexArray == nullptr) ||
		(_DynamicSortingIndexArray->Num_Refs() == 1));
	REF_PTR_RELEASE(_DynamicSortingIndexArray);
	_DynamicSortingIndexArrayInUse = false;
	_DynamicSortingIndexArraySize = 0;
	_DynamicSortingIndexArrayOffset = 0;
}

void DynamicIBAccessClass::Allocate_Render_Dynamic_Buffer()
{
	WWMEMLOG(MEM_RENDERER);
	WWASSERT(!_DynamicRenderIndexBufferInUse);
	_DynamicRenderIndexBufferInUse = true;

	if (IndexCount > _DynamicRenderIndexBufferSize)
	{
		REF_PTR_RELEASE(_DynamicRenderIndexBuffer);
		_DynamicRenderIndexBufferSize = IndexCount;
		if (_DynamicRenderIndexBufferSize < DEFAULT_IB_SIZE)
		{
			_DynamicRenderIndexBufferSize = DEFAULT_IB_SIZE;
		}
	}

	if (!_DynamicRenderIndexBuffer)
	{
		unsigned usage = IndexBufferClass::USAGE_DYNAMIC;
		if (WW3D::Get_Render_Backend()->Supports_NPatches())
		{
			usage |= IndexBufferClass::USAGE_NPATCHES;
		}
		_DynamicRenderIndexBuffer = NEW_REF(IndexBufferClass,(
			_DynamicRenderIndexBufferSize,
			static_cast<IndexBufferClass::UsageType>(usage)));
		_DynamicRenderIndexBufferOffset = 0;
	}

	if (static_cast<unsigned>(IndexCount) + _DynamicRenderIndexBufferOffset >
		_DynamicRenderIndexBufferSize)
	{
		_DynamicRenderIndexBufferOffset = 0;
	}

	REF_PTR_SET(IndexBuffer, _DynamicRenderIndexBuffer);
	IndexBufferOffset = _DynamicRenderIndexBufferOffset;
}

void DynamicIBAccessClass::Allocate_Sorting_Dynamic_Buffer()
{
	WWMEMLOG(MEM_RENDERER);
	WWASSERT(!_DynamicSortingIndexArrayInUse);
	_DynamicSortingIndexArrayInUse = true;

	const unsigned new_index_count = _DynamicSortingIndexArrayOffset + IndexCount;
	WWASSERT(new_index_count < 65536);
	if (new_index_count > _DynamicSortingIndexArraySize)
	{
		REF_PTR_RELEASE(_DynamicSortingIndexArray);
		_DynamicSortingIndexArraySize = new_index_count;
		if (_DynamicSortingIndexArraySize < DEFAULT_IB_SIZE)
		{
			_DynamicSortingIndexArraySize = DEFAULT_IB_SIZE;
		}
	}

	if (!_DynamicSortingIndexArray)
	{
		_DynamicSortingIndexArray = NEW_REF(SortingIndexBufferClass,(
			_DynamicSortingIndexArraySize));
		_DynamicSortingIndexArrayOffset = 0;
	}

	REF_PTR_SET(IndexBuffer, _DynamicSortingIndexArray);
	IndexBufferOffset = _DynamicSortingIndexArrayOffset;
}

DynamicIBAccessClass::WriteLockClass::WriteLockClass(
	DynamicIBAccessClass *ib_access_) :
	DynamicIBAccess(ib_access_),
	Indices(nullptr),
	Locked(false)
{
	WWASSERT(DynamicIBAccess != nullptr);
	if (DynamicIBAccess == nullptr || DynamicIBAccess->IndexBuffer == nullptr)
	{
		return;
	}

	DynamicIBAccess->IndexBuffer->Add_Ref();
	if (DynamicIBAccess->Get_Type() == BUFFER_TYPE_DYNAMIC_RENDER)
	{
		const RenderBackendBufferLockMode mode = DynamicIBAccess->IndexBufferOffset == 0 ?
			RenderBackendBufferLockMode::Discard : RenderBackendBufferLockMode::NoOverwrite;
		Locked = DynamicIBAccess->IndexBuffer->Lock_Backend_Buffer(
				DynamicIBAccess->IndexBufferOffset * sizeof(unsigned short),
				DynamicIBAccess->Get_Index_Count() * sizeof(unsigned short),
				reinterpret_cast<void **>(&Indices), mode);
	}
	else if (DynamicIBAccess->Get_Type() == BUFFER_TYPE_DYNAMIC_SORTING)
	{
		Indices = static_cast<SortingIndexBufferClass *>(
			DynamicIBAccess->IndexBuffer)->Get_Index_Array() +
			DynamicIBAccess->IndexBufferOffset;
		Locked = Indices != nullptr;
	}
	else
	{
		WWASSERT(false);
	}
}

DynamicIBAccessClass::WriteLockClass::~WriteLockClass()
{
	if (Locked && DynamicIBAccess != nullptr &&
		DynamicIBAccess->Get_Type() == BUFFER_TYPE_DYNAMIC_RENDER &&
		DynamicIBAccess->IndexBuffer != nullptr &&
		DynamicIBAccess->IndexBuffer->Get_Backend() != nullptr)
	{
		DynamicIBAccess->IndexBuffer->Unlock_Backend_Buffer();
	}
	if (DynamicIBAccess != nullptr && DynamicIBAccess->IndexBuffer != nullptr)
	{
		DynamicIBAccess->IndexBuffer->Release_Ref();
	}
}

void DynamicIBAccessClass::_Reset(bool frame_changed)
{
	_DynamicSortingIndexArrayOffset = 0;
	if (frame_changed)
	{
		_DynamicRenderIndexBufferOffset = 0;
	}
}

unsigned short DynamicIBAccessClass::Get_Default_Index_Count()
{
	return _DynamicRenderIndexBufferSize;
}
