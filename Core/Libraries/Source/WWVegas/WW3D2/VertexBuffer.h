/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#pragma once

#include "WWLib/always.h"
#include "WWDebug/wwdebug.h"
#include "Buffer.h"
#include "VertexFormat.h"
#include "Backend/IRenderBackend.h"

class Vector2;
class Vector3;
class Vector4;

class VertexBufferClass;

class VertexBufferLockClass
{
protected:
	VertexBufferClass *VertexBuffer;
	void *Vertices;
	bool Locked;

	VertexBufferLockClass(VertexBufferClass *vertex_buffer_) :
		VertexBuffer(vertex_buffer_),
		Vertices(nullptr),
		Locked(false)
	{
	}

public:
	void *Get_Vertex_Array() { return Vertices; }
	bool Is_Locked() const { return Locked; }
};

/**
** VertexBufferClass is the WW3D buffer abstraction. Its resource is owned by
** the active IRenderBackend and is intentionally opaque to callers.
*/
class VertexBufferClass : public RefCountClass
{
public:
	enum UsageType
	{
		USAGE_DEFAULT = BUFFER_USAGE_DEFAULT,
		USAGE_DYNAMIC = BUFFER_USAGE_DYNAMIC,
		USAGE_SOFTWAREPROCESSING = BUFFER_USAGE_SOFTWARE_PROCESSING,
		USAGE_NPATCHES = BUFFER_USAGE_NPATCHES
	};

protected:
	VertexBufferClass(unsigned type, const RenderBackendVertexLayout &layout,
		unsigned short vertex_count, UsageType usage);
	virtual ~VertexBufferClass() override;

public:

	VertexBufferClass(RenderBackendVertexFormat format, unsigned short vertex_count,
		UsageType usage = USAGE_DEFAULT);
	VertexBufferClass(const RenderBackendVertexLayout &layout, unsigned short vertex_count,
		UsageType usage = USAGE_DEFAULT);
	VertexBufferClass(const Vector3 *vertices, const Vector3 *normals,
		const Vector2 *tex_coords, unsigned short vertex_count,
		UsageType usage = USAGE_DEFAULT);
	VertexBufferClass(const Vector3 *vertices, const Vector3 *normals,
		const Vector4 *diffuse, const Vector2 *tex_coords,
		unsigned short vertex_count, UsageType usage = USAGE_DEFAULT);
	VertexBufferClass(const Vector3 *vertices, const Vector4 *diffuse,
		const Vector2 *tex_coords, unsigned short vertex_count,
		UsageType usage = USAGE_DEFAULT);
	VertexBufferClass(const Vector3 *vertices, const Vector2 *tex_coords,
		unsigned short vertex_count, UsageType usage = USAGE_DEFAULT);

	const VertexFormatInfoClass &Get_Format_Info() const { return FormatInfo; }
	const RenderBackendVertexLayout &Get_Format_Layout() const { return FormatInfo.Get_Layout(); }
	RenderBackendVertexFormat Get_Format() const { return FormatInfo.Get_Format(); }
	unsigned short Get_Vertex_Count() const { return VertexCount; }
	unsigned Get_Vertex_Size() const { return FormatInfo.Get_Vertex_Size(); }
	unsigned Type() const { return type; }

	// The returned value is an opaque IRenderBackend resource. It is only
	// consumed by the backend implementation and never exposes a native API.
	RenderBackendVertexBuffer *Get_Backend_Buffer() const { return BackendBuffer; }
	IRenderBackend *Get_Backend() const { return Backend; }
	bool Lock_Backend_Buffer(unsigned offset_bytes, unsigned size_bytes, void **data,
		RenderBackendBufferLockMode mode) const
	{
		return Backend != nullptr && Backend->Lock_Vertex_Buffer(
			BackendBuffer, offset_bytes, size_bytes, data, mode);
	}
	void Unlock_Backend_Buffer() const
	{
		if (Backend != nullptr) Backend->Unlock_Vertex_Buffer(BackendBuffer);
	}

	void Add_Engine_Ref() const;
	void Release_Engine_Ref() const;
	unsigned Engine_Refs() const { return engine_refs; }

	class WriteLockClass : public VertexBufferLockClass
	{
	public:
		WriteLockClass(VertexBufferClass *vertex_buffer,
			RenderBackendBufferLockMode mode = RenderBackendBufferLockMode::Normal);
		~WriteLockClass();
	};

	class AppendLockClass : public VertexBufferLockClass
	{
	public:
		AppendLockClass(VertexBufferClass *vertex_buffer,
			unsigned start_index, unsigned index_range);
		~AppendLockClass();
	};

	void Copy(const Vector3 *loc, unsigned first_vertex, unsigned count);
	void Copy(const Vector3 *loc, const Vector2 *uv, unsigned first_vertex, unsigned count);
	void Copy(const Vector3 *loc, const Vector3 *norm, unsigned first_vertex, unsigned count);
	void Copy(const Vector3 *loc, const Vector3 *norm, const Vector2 *uv,
		unsigned first_vertex, unsigned count);
	void Copy(const Vector3 *loc, const Vector3 *norm, const Vector2 *uv,
		const Vector4 *diffuse, unsigned first_vertex, unsigned count);
	void Copy(const Vector3 *loc, const Vector2 *uv, const Vector4 *diffuse,
		unsigned first_vertex, unsigned count);

	static unsigned Get_Total_Buffer_Count();
	static unsigned Get_Total_Allocated_Vertices();
	static unsigned Get_Total_Allocated_Memory();

protected:
	void Create_Backend_Buffer(UsageType usage);

	unsigned type;
	unsigned short VertexCount;
	mutable int engine_refs;
	VertexFormatInfoClass FormatInfo;
	IRenderBackend *Backend;
	RenderBackendVertexBuffer *BackendBuffer;
};

class DynamicVBAccessClass
{
	RenderBackendVertexFormat Format;
	unsigned TypeValue;
	unsigned short VertexCount;
	unsigned short VertexBufferOffset;
	VertexBufferClass *VertexBuffer;

	void Allocate_Sorting_Dynamic_Buffer();
	void Allocate_Render_Dynamic_Buffer();

public:
	DynamicVBAccessClass(unsigned type, RenderBackendVertexFormat format,
		unsigned short vertex_count);
	~DynamicVBAccessClass();

	const VertexFormatInfoClass &Get_Format_Info() const;
	RenderBackendVertexFormat Get_Format() const { return Format; }
	unsigned Get_Type() const { return TypeValue; }
	unsigned short Get_Vertex_Count() const { return VertexCount; }
	unsigned short Get_Vertex_Buffer_Offset() const { return VertexBufferOffset; }
	VertexBufferClass *Get_Vertex_Buffer() const { return VertexBuffer; }

	static void _Deinit();
	static void _Reset(bool frame_changed);
	static unsigned short Get_Default_Vertex_Count();

	class WriteLockClass
	{
		DynamicVBAccessClass *DynamicVBAccess;
		VertexFormatXYZNDUV2 *Vertices;
		bool Locked;

	public:
		WriteLockClass(DynamicVBAccessClass *vb_access);
		~WriteLockClass();

		VertexFormatXYZNDUV2 *Get_Formatted_Vertex_Array() { return Vertices; }
		bool Is_Locked() const { return Locked; }
	};
};

class SortingVertexBufferClass : public VertexBufferClass
{
	W3DMPO_CODE(SortingVertexBufferClass)

	VertexFormatXYZNDUV2 *VertexBuffer;

protected:
	virtual ~SortingVertexBufferClass() override;

public:
	SortingVertexBufferClass(unsigned short vertex_count);
	VertexFormatXYZNDUV2 *Get_Vertex_Array() { return VertexBuffer; }
	const VertexFormatXYZNDUV2 *Get_Vertex_Array() const { return VertexBuffer; }
};
