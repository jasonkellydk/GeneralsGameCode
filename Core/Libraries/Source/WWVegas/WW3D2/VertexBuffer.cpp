/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "VertexBuffer.h"

#include "WW3D.h"
#include "Texture.h"
#include "WWMath/vector2.h"
#include "WWMath/vector3.h"
#include "WWMath/vector4.h"
#include "WWLib/thread.h"
#include "WWDebug/wwmemlog.h"

#define DEFAULT_VB_SIZE 5000

static bool _DynamicSortingVertexArrayInUse = false;
static SortingVertexBufferClass *_DynamicSortingVertexArray = nullptr;
static unsigned short _DynamicSortingVertexArraySize = 0;
static unsigned short _DynamicSortingVertexArrayOffset = 0;

static bool _DynamicRenderVertexBufferInUse = false;
static VertexBufferClass *_DynamicRenderVertexBuffer = nullptr;
static unsigned short _DynamicRenderVertexBufferSize = DEFAULT_VB_SIZE;
static unsigned short _DynamicRenderVertexBufferOffset = 0;

static int _VertexBufferCount;
static int _VertexBufferTotalVertices;
static int _VertexBufferTotalSize;

VertexBufferClass::VertexBufferClass(unsigned type_,
	const RenderBackendVertexLayout &layout, unsigned short vertex_count_,
	UsageType usage) :
	type(type_),
	VertexCount(vertex_count_),
	engine_refs(0),
	FormatInfo(layout),
	Backend(nullptr),
	BackendBuffer(nullptr)
{
	WWMEMLOG(MEM_RENDERER);
	WWASSERT(VertexCount);
	WWASSERT(type == BUFFER_TYPE_RENDER || type == BUFFER_TYPE_SORTING);
	WWASSERT(FormatInfo.Get_Vertex_Size() != 0);

	if (type == BUFFER_TYPE_RENDER)
	{
		Create_Backend_Buffer(usage);
	}

	_VertexBufferCount++;
	_VertexBufferTotalVertices += VertexCount;
	_VertexBufferTotalSize += VertexCount * FormatInfo.Get_Vertex_Size();
}

VertexBufferClass::VertexBufferClass(RenderBackendVertexFormat format,
	unsigned short vertex_count, UsageType usage) :
	VertexBufferClass(BUFFER_TYPE_RENDER, RenderBackend_Vertex_Layout(format),
		vertex_count, usage)
{
}

VertexBufferClass::VertexBufferClass(const RenderBackendVertexLayout &layout,
	unsigned short vertex_count, UsageType usage) :
	VertexBufferClass(BUFFER_TYPE_RENDER, layout, vertex_count, usage)
{
}

VertexBufferClass::VertexBufferClass(const Vector3 *vertices,
	const Vector3 *normals, const Vector2 *tex_coords,
	unsigned short vertex_count, UsageType usage) :
	VertexBufferClass(RenderBackendVertexFormat::PositionNormalTexture,
		vertex_count, usage)
{
	WWASSERT(vertices);
	WWASSERT(normals);
	WWASSERT(tex_coords);
	Copy(vertices, normals, tex_coords, 0, vertex_count);
}

VertexBufferClass::VertexBufferClass(const Vector3 *vertices,
	const Vector3 *normals, const Vector4 *diffuse,
	const Vector2 *tex_coords, unsigned short vertex_count,
	UsageType usage) :
	VertexBufferClass(RenderBackendVertexFormat::PositionNormalDiffuseTexture,
		vertex_count, usage)
{
	WWASSERT(vertices);
	WWASSERT(normals);
	WWASSERT(diffuse);
	WWASSERT(tex_coords);
	Copy(vertices, normals, tex_coords, diffuse, 0, vertex_count);
}

VertexBufferClass::VertexBufferClass(const Vector3 *vertices,
	const Vector4 *diffuse, const Vector2 *tex_coords,
	unsigned short vertex_count, UsageType usage) :
	VertexBufferClass(RenderBackendVertexFormat::PositionDiffuseTexture,
		vertex_count, usage)
{
	WWASSERT(vertices);
	WWASSERT(diffuse);
	WWASSERT(tex_coords);
	Copy(vertices, tex_coords, diffuse, 0, vertex_count);
}

VertexBufferClass::VertexBufferClass(const Vector3 *vertices,
	const Vector2 *tex_coords, unsigned short vertex_count,
	UsageType usage) :
	VertexBufferClass(RenderBackendVertexFormat::PositionTexture,
		vertex_count, usage)
{
	WWASSERT(vertices);
	WWASSERT(tex_coords);
	Copy(vertices, tex_coords, 0, vertex_count);
}

VertexBufferClass::~VertexBufferClass()
{
	if (BackendBuffer != nullptr && Backend != nullptr)
	{
		Backend->Release_Vertex_Buffer(BackendBuffer);
		BackendBuffer = nullptr;
	}

	_VertexBufferCount--;
	_VertexBufferTotalVertices -= VertexCount;
	_VertexBufferTotalSize -= VertexCount * FormatInfo.Get_Vertex_Size();
}

void VertexBufferClass::Create_Backend_Buffer(UsageType usage)
{
	Backend = WW3D::Get_Render_Backend();
	if (Backend == nullptr)
	{
		WWASSERT(false);
		return;
	}

	BackendBuffer = Backend->Create_Vertex_Buffer(
		VertexCount * FormatInfo.Get_Vertex_Size(), FormatInfo.Get_Layout(), usage);
	if (BackendBuffer == nullptr)
	{
		// Match the old allocation recovery path without allowing the buffer
		// wrapper to know anything about the concrete renderer.
		TextureClass::Invalidate_Old_Unused_Textures(5000);
		WW3D::_Invalidate_Mesh_Cache();
		BackendBuffer = Backend->Create_Vertex_Buffer(
			VertexCount * FormatInfo.Get_Vertex_Size(), FormatInfo.Get_Layout(), usage);
	}

	WWASSERT(BackendBuffer != nullptr);
}

unsigned VertexBufferClass::Get_Total_Buffer_Count()
{
	return _VertexBufferCount;
}

unsigned VertexBufferClass::Get_Total_Allocated_Vertices()
{
	return _VertexBufferTotalVertices;
}

unsigned VertexBufferClass::Get_Total_Allocated_Memory()
{
	return _VertexBufferTotalSize;
}

void VertexBufferClass::Add_Engine_Ref() const
{
	engine_refs++;
}

void VertexBufferClass::Release_Engine_Ref() const
{
	engine_refs--;
	WWASSERT(engine_refs >= 0);
}

VertexBufferClass::WriteLockClass::WriteLockClass(VertexBufferClass *vertex_buffer,
	RenderBackendBufferLockMode mode) :
	VertexBufferLockClass(vertex_buffer)
{
	WWASSERT(VertexBuffer != nullptr);
	WWASSERT(VertexBuffer == nullptr || !VertexBuffer->Engine_Refs());
	if (VertexBuffer == nullptr)
	{
		return;
	}

	VertexBuffer->Add_Ref();
	if (VertexBuffer->Type() == BUFFER_TYPE_RENDER)
	{
		Locked = VertexBuffer->Lock_Backend_Buffer(0, 0, &Vertices, mode);
	}
	else if (VertexBuffer->Type() == BUFFER_TYPE_SORTING)
	{
		Vertices = static_cast<SortingVertexBufferClass *>(VertexBuffer)->Get_Vertex_Array();
		Locked = Vertices != nullptr;
	}
	else
	{
		WWASSERT(false);
	}
}

VertexBufferClass::WriteLockClass::~WriteLockClass()
{
	if (Locked && VertexBuffer != nullptr && VertexBuffer->Type() == BUFFER_TYPE_RENDER &&
		VertexBuffer->Get_Backend() != nullptr)
	{
		VertexBuffer->Unlock_Backend_Buffer();
	}
	if (VertexBuffer != nullptr)
	{
		VertexBuffer->Release_Ref();
	}
}

VertexBufferClass::AppendLockClass::AppendLockClass(VertexBufferClass *vertex_buffer,
	unsigned start_index, unsigned index_range) :
	VertexBufferLockClass(vertex_buffer)
{
	WWASSERT(VertexBuffer != nullptr);
	WWASSERT(VertexBuffer == nullptr || !VertexBuffer->Engine_Refs());
	WWASSERT(VertexBuffer == nullptr || start_index + index_range <= VertexBuffer->Get_Vertex_Count());
	if (VertexBuffer == nullptr)
	{
		return;
	}

	VertexBuffer->Add_Ref();
	if (VertexBuffer->Type() == BUFFER_TYPE_RENDER)
	{
		const unsigned offset = start_index * VertexBuffer->Get_Vertex_Size();
		const unsigned size = index_range * VertexBuffer->Get_Vertex_Size();
		Locked = VertexBuffer->Lock_Backend_Buffer(offset, size, &Vertices,
			RenderBackendBufferLockMode::Normal);
	}
	else if (VertexBuffer->Type() == BUFFER_TYPE_SORTING)
	{
		Vertices = static_cast<SortingVertexBufferClass *>(VertexBuffer)->Get_Vertex_Array() + start_index;
		Locked = Vertices != nullptr;
	}
	else
	{
		WWASSERT(false);
	}
}

VertexBufferClass::AppendLockClass::~AppendLockClass()
{
	if (Locked && VertexBuffer != nullptr && VertexBuffer->Type() == BUFFER_TYPE_RENDER &&
		VertexBuffer->Get_Backend() != nullptr)
	{
		VertexBuffer->Unlock_Backend_Buffer();
	}
	if (VertexBuffer != nullptr)
	{
		VertexBuffer->Release_Ref();
	}
}

SortingVertexBufferClass::SortingVertexBufferClass(unsigned short vertex_count) :
	VertexBufferClass(BUFFER_TYPE_SORTING,
		RenderBackend_Vertex_Layout(RenderBackend_Dynamic_Vertex_Format),
		vertex_count, USAGE_DEFAULT),
	VertexBuffer(nullptr)
{
	WWMEMLOG(MEM_RENDERER);
	VertexBuffer = W3DNEWARRAY VertexFormatXYZNDUV2[vertex_count];
}

SortingVertexBufferClass::~SortingVertexBufferClass()
{
	delete[] VertexBuffer;
}

void VertexBufferClass::Copy(const Vector3 *loc, unsigned first_vertex, unsigned count)
{
	WWASSERT(loc);
	WWASSERT(count <= VertexCount);
	WWASSERT(Get_Format() == RenderBackendVertexFormat::Position);
	VertexBufferClass::AppendLockClass lock(this, first_vertex, count);
	if (!lock.Is_Locked()) return;
	VertexFormatXYZ *verts = static_cast<VertexFormatXYZ *>(lock.Get_Vertex_Array());
	for (unsigned v = 0; v < count; ++v)
	{
		verts[v].x = (*loc)[0];
		verts[v].y = (*loc)[1];
		verts[v].z = (*loc++)[2];
	}
}

void VertexBufferClass::Copy(const Vector3 *loc, const Vector2 *uv,
	unsigned first_vertex, unsigned count)
{
	WWASSERT(loc);
	WWASSERT(uv);
	WWASSERT(count <= VertexCount);
	WWASSERT(Get_Format() == RenderBackendVertexFormat::PositionTexture);
	VertexBufferClass::AppendLockClass lock(this, first_vertex, count);
	if (!lock.Is_Locked()) return;
	VertexFormatXYZUV1 *verts = static_cast<VertexFormatXYZUV1 *>(lock.Get_Vertex_Array());
	for (unsigned v = 0; v < count; ++v)
	{
		verts[v].x = (*loc)[0];
		verts[v].y = (*loc)[1];
		verts[v].z = (*loc++)[2];
		verts[v].u1 = (*uv)[0];
		verts[v].v1 = (*uv++)[1];
	}
}

void VertexBufferClass::Copy(const Vector3 *loc, const Vector3 *norm,
	unsigned first_vertex, unsigned count)
{
	WWASSERT(loc);
	WWASSERT(norm);
	WWASSERT(count <= VertexCount);
	WWASSERT(Get_Format() == RenderBackendVertexFormat::PositionNormal);
	VertexBufferClass::AppendLockClass lock(this, first_vertex, count);
	if (!lock.Is_Locked()) return;
	VertexFormatXYZN *verts = static_cast<VertexFormatXYZN *>(lock.Get_Vertex_Array());
	for (unsigned v = 0; v < count; ++v)
	{
		verts[v].x = (*loc)[0];
		verts[v].y = (*loc)[1];
		verts[v].z = (*loc++)[2];
		verts[v].nx = (*norm)[0];
		verts[v].ny = (*norm)[1];
		verts[v].nz = (*norm++)[2];
	}
}

void VertexBufferClass::Copy(const Vector3 *loc, const Vector3 *norm,
	const Vector2 *uv, unsigned first_vertex, unsigned count)
{
	WWASSERT(loc);
	WWASSERT(norm);
	WWASSERT(uv);
	WWASSERT(count <= VertexCount);
	WWASSERT(Get_Format() == RenderBackendVertexFormat::PositionNormalTexture);
	VertexBufferClass::AppendLockClass lock(this, first_vertex, count);
	if (!lock.Is_Locked()) return;
	VertexFormatXYZNUV1 *verts = static_cast<VertexFormatXYZNUV1 *>(lock.Get_Vertex_Array());
	for (unsigned v = 0; v < count; ++v)
	{
		verts[v].x = (*loc)[0];
		verts[v].y = (*loc)[1];
		verts[v].z = (*loc++)[2];
		verts[v].nx = (*norm)[0];
		verts[v].ny = (*norm)[1];
		verts[v].nz = (*norm++)[2];
		verts[v].u1 = (*uv)[0];
		verts[v].v1 = (*uv++)[1];
	}
}

void VertexBufferClass::Copy(const Vector3 *loc, const Vector3 *norm,
	const Vector2 *uv, const Vector4 *diffuse, unsigned first_vertex,
	unsigned count)
{
	WWASSERT(loc);
	WWASSERT(norm);
	WWASSERT(uv);
	WWASSERT(diffuse);
	WWASSERT(count <= VertexCount);
	WWASSERT(Get_Format() == RenderBackendVertexFormat::PositionNormalDiffuseTexture);
	VertexBufferClass::AppendLockClass lock(this, first_vertex, count);
	if (!lock.Is_Locked()) return;
	VertexFormatXYZNDUV1 *verts = static_cast<VertexFormatXYZNDUV1 *>(lock.Get_Vertex_Array());
	for (unsigned v = 0; v < count; ++v)
	{
		verts[v].x = (*loc)[0];
		verts[v].y = (*loc)[1];
		verts[v].z = (*loc++)[2];
		verts[v].nx = (*norm)[0];
		verts[v].ny = (*norm)[1];
		verts[v].nz = (*norm++)[2];
		verts[v].u1 = (*uv)[0];
		verts[v].v1 = (*uv++)[1];
		verts[v].diffuse = WW3D::Get_Render_Backend()->Pack_Color(diffuse[v]);
	}
}

void VertexBufferClass::Copy(const Vector3 *loc, const Vector2 *uv,
	const Vector4 *diffuse, unsigned first_vertex, unsigned count)
{
	WWASSERT(loc);
	WWASSERT(uv);
	WWASSERT(diffuse);
	WWASSERT(count <= VertexCount);
	WWASSERT(Get_Format() == RenderBackendVertexFormat::PositionDiffuseTexture);
	VertexBufferClass::AppendLockClass lock(this, first_vertex, count);
	if (!lock.Is_Locked()) return;
	VertexFormatXYZDUV1 *verts = static_cast<VertexFormatXYZDUV1 *>(lock.Get_Vertex_Array());
	for (unsigned v = 0; v < count; ++v)
	{
		verts[v].x = (*loc)[0];
		verts[v].y = (*loc)[1];
		verts[v].z = (*loc++)[2];
		verts[v].u1 = (*uv)[0];
		verts[v].v1 = (*uv++)[1];
		verts[v].diffuse = WW3D::Get_Render_Backend()->Pack_Color(diffuse[v]);
	}
}

DynamicVBAccessClass::DynamicVBAccessClass(unsigned type_,
	RenderBackendVertexFormat format_, unsigned short vertex_count_) :
	Format(format_),
	TypeValue(type_),
	VertexCount(vertex_count_),
	VertexBufferOffset(0),
	VertexBuffer(nullptr)
{
	WWASSERT(Format == RenderBackend_Dynamic_Vertex_Format);
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

DynamicVBAccessClass::~DynamicVBAccessClass()
{
	if (TypeValue == BUFFER_TYPE_DYNAMIC_RENDER)
	{
		_DynamicRenderVertexBufferInUse = false;
		_DynamicRenderVertexBufferOffset += VertexCount;
	}
	else
	{
		_DynamicSortingVertexArrayInUse = false;
		_DynamicSortingVertexArrayOffset += VertexCount;
	}
	REF_PTR_RELEASE(VertexBuffer);
}

const VertexFormatInfoClass &DynamicVBAccessClass::Get_Format_Info() const
{
	WWASSERT(VertexBuffer != nullptr);
	return VertexBuffer->Get_Format_Info();
}

void DynamicVBAccessClass::_Deinit()
{
	WWASSERT((_DynamicRenderVertexBuffer == nullptr) ||
		(_DynamicRenderVertexBuffer->Num_Refs() == 1));
	REF_PTR_RELEASE(_DynamicRenderVertexBuffer);
	_DynamicRenderVertexBufferInUse = false;
	_DynamicRenderVertexBufferSize = DEFAULT_VB_SIZE;
	_DynamicRenderVertexBufferOffset = 0;

	WWASSERT((_DynamicSortingVertexArray == nullptr) ||
		(_DynamicSortingVertexArray->Num_Refs() == 1));
	REF_PTR_RELEASE(_DynamicSortingVertexArray);
	_DynamicSortingVertexArrayInUse = false;
	_DynamicSortingVertexArraySize = 0;
	_DynamicSortingVertexArrayOffset = 0;
}

void DynamicVBAccessClass::Allocate_Render_Dynamic_Buffer()
{
	WWMEMLOG(MEM_RENDERER);
	WWASSERT(!_DynamicRenderVertexBufferInUse);
	_DynamicRenderVertexBufferInUse = true;

	if (VertexCount > _DynamicRenderVertexBufferSize)
	{
		REF_PTR_RELEASE(_DynamicRenderVertexBuffer);
		_DynamicRenderVertexBufferSize = VertexCount;
		if (_DynamicRenderVertexBufferSize < DEFAULT_VB_SIZE)
		{
			_DynamicRenderVertexBufferSize = DEFAULT_VB_SIZE;
		}
	}

	if (!_DynamicRenderVertexBuffer)
	{
		unsigned usage = VertexBufferClass::USAGE_DYNAMIC;
		if (WW3D::Get_Render_Backend()->Supports_NPatches())
		{
			usage |= VertexBufferClass::USAGE_NPATCHES;
		}
		_DynamicRenderVertexBuffer = NEW_REF(VertexBufferClass,(
			RenderBackend_Dynamic_Vertex_Format,
			_DynamicRenderVertexBufferSize,
			static_cast<VertexBufferClass::UsageType>(usage)));
		_DynamicRenderVertexBufferOffset = 0;
	}

	if (static_cast<unsigned>(VertexCount) + _DynamicRenderVertexBufferOffset >
		_DynamicRenderVertexBufferSize)
	{
		_DynamicRenderVertexBufferOffset = 0;
	}

	REF_PTR_SET(VertexBuffer, _DynamicRenderVertexBuffer);
	VertexBufferOffset = _DynamicRenderVertexBufferOffset;
}

void DynamicVBAccessClass::Allocate_Sorting_Dynamic_Buffer()
{
	WWMEMLOG(MEM_RENDERER);
	WWASSERT(!_DynamicSortingVertexArrayInUse);
	_DynamicSortingVertexArrayInUse = true;

	const unsigned new_vertex_count = _DynamicSortingVertexArrayOffset + VertexCount;
	WWASSERT(new_vertex_count < 65536);
	if (new_vertex_count > _DynamicSortingVertexArraySize)
	{
		REF_PTR_RELEASE(_DynamicSortingVertexArray);
		_DynamicSortingVertexArraySize = new_vertex_count;
		if (_DynamicSortingVertexArraySize < DEFAULT_VB_SIZE)
		{
			_DynamicSortingVertexArraySize = DEFAULT_VB_SIZE;
		}
	}

	if (!_DynamicSortingVertexArray)
	{
		_DynamicSortingVertexArray = NEW_REF(SortingVertexBufferClass,(
			_DynamicSortingVertexArraySize));
		_DynamicSortingVertexArrayOffset = 0;
	}

	REF_PTR_SET(VertexBuffer, _DynamicSortingVertexArray);
	VertexBufferOffset = _DynamicSortingVertexArrayOffset;
}

DynamicVBAccessClass::WriteLockClass::WriteLockClass(
	DynamicVBAccessClass *dynamic_vb_access_) :
	DynamicVBAccess(dynamic_vb_access_),
	Vertices(nullptr),
	Locked(false)
{
	WWASSERT(DynamicVBAccess != nullptr);
	if (DynamicVBAccess == nullptr || DynamicVBAccess->VertexBuffer == nullptr)
	{
		return;
	}

	DynamicVBAccess->VertexBuffer->Add_Ref();
	if (DynamicVBAccess->Get_Type() == BUFFER_TYPE_DYNAMIC_RENDER)
	{
		const unsigned offset = DynamicVBAccess->VertexBufferOffset *
			DynamicVBAccess->Get_Format_Info().Get_Vertex_Size();
		const unsigned size = DynamicVBAccess->Get_Vertex_Count() *
			DynamicVBAccess->Get_Format_Info().Get_Vertex_Size();
		const RenderBackendBufferLockMode mode = DynamicVBAccess->VertexBufferOffset == 0 ?
			RenderBackendBufferLockMode::Discard : RenderBackendBufferLockMode::NoOverwrite;
		Locked = DynamicVBAccess->VertexBuffer->Lock_Backend_Buffer(
			offset, size, reinterpret_cast<void **>(&Vertices), mode);
	}
	else if (DynamicVBAccess->Get_Type() == BUFFER_TYPE_DYNAMIC_SORTING)
	{
		Vertices = static_cast<SortingVertexBufferClass *>(
			DynamicVBAccess->VertexBuffer)->Get_Vertex_Array() +
			DynamicVBAccess->VertexBufferOffset;
		Locked = Vertices != nullptr;
	}
	else
	{
		WWASSERT(false);
	}
}

DynamicVBAccessClass::WriteLockClass::~WriteLockClass()
{
	if (Locked && DynamicVBAccess != nullptr &&
		DynamicVBAccess->Get_Type() == BUFFER_TYPE_DYNAMIC_RENDER &&
		DynamicVBAccess->VertexBuffer != nullptr &&
		DynamicVBAccess->VertexBuffer->Get_Backend() != nullptr)
	{
		DynamicVBAccess->VertexBuffer->Unlock_Backend_Buffer();
	}
	if (DynamicVBAccess != nullptr && DynamicVBAccess->VertexBuffer != nullptr)
	{
		DynamicVBAccess->VertexBuffer->Release_Ref();
	}
}

void DynamicVBAccessClass::_Reset(bool frame_changed)
{
	_DynamicSortingVertexArrayOffset = 0;
	if (frame_changed)
	{
		_DynamicRenderVertexBufferOffset = 0;
	}
}

unsigned short DynamicVBAccessClass::Get_Default_Vertex_Count()
{
	return _DynamicRenderVertexBufferSize;
}
