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
#include "Backend/IRenderBackend.h"

class IndexBufferClass;

class IndexBufferClass : public RefCountClass
{
public:
	enum UsageType
	{
		USAGE_DEFAULT = BUFFER_USAGE_DEFAULT,
		USAGE_DYNAMIC = BUFFER_USAGE_DYNAMIC,
		USAGE_SOFTWAREPROCESSING = BUFFER_USAGE_SOFTWARE_PROCESSING,
		USAGE_NPATCHES = BUFFER_USAGE_NPATCHES
	};

	IndexBufferClass(unsigned short index_count, UsageType usage = USAGE_DEFAULT);

protected:
	IndexBufferClass(unsigned type, unsigned short index_count, UsageType usage);
	virtual ~IndexBufferClass() override;

public:
	void Copy(unsigned int *indices, unsigned start_index, unsigned index_count);
	void Copy(unsigned short *indices, unsigned start_index, unsigned index_count);

	unsigned short Get_Index_Count() const { return index_count; }
	unsigned Type() const { return type; }

	RenderBackendIndexBuffer *Get_Backend_Buffer() const { return BackendBuffer; }
	IRenderBackend *Get_Backend() const { return Backend; }
	bool Lock_Backend_Buffer(unsigned offset_bytes, unsigned size_bytes, void **data,
		RenderBackendBufferLockMode mode) const
	{
		return Backend != nullptr && Backend->Lock_Index_Buffer(
			BackendBuffer, offset_bytes, size_bytes, data, mode);
	}
	void Unlock_Backend_Buffer() const
	{
		if (Backend != nullptr) Backend->Unlock_Index_Buffer(BackendBuffer);
	}

	void Add_Engine_Ref() const;
	void Release_Engine_Ref() const;
	unsigned Engine_Refs() const { return engine_refs; }

	class WriteLockClass
	{
		IndexBufferClass *index_buffer;
		unsigned short *indices;
		bool locked;

	public:
		WriteLockClass(IndexBufferClass *index_buffer,
			RenderBackendBufferLockMode mode = RenderBackendBufferLockMode::Normal);
		~WriteLockClass();

		unsigned short *Get_Index_Array() { return indices; }
		bool Is_Locked() const { return locked; }
	};

	class AppendLockClass
	{
		IndexBufferClass *index_buffer;
		unsigned short *indices;
		bool locked;

	public:
		AppendLockClass(IndexBufferClass *index_buffer,
			unsigned start_index, unsigned index_range);
		~AppendLockClass();

		unsigned short *Get_Index_Array() { return indices; }
		bool Is_Locked() const { return locked; }
	};

	static unsigned Get_Total_Buffer_Count();
	static unsigned Get_Total_Allocated_Indices();
	static unsigned Get_Total_Allocated_Memory();

protected:
	unsigned type;
	unsigned short index_count;
	mutable int engine_refs;
	IRenderBackend *Backend;
	RenderBackendIndexBuffer *BackendBuffer;
};

class DynamicIBAccessClass
{
	unsigned TypeValue;
	unsigned short IndexCount;
	unsigned short IndexBufferOffset;
	IndexBufferClass *IndexBuffer;

	void Allocate_Sorting_Dynamic_Buffer();
	void Allocate_Render_Dynamic_Buffer();

public:
	DynamicIBAccessClass(unsigned type, unsigned short index_count);
	~DynamicIBAccessClass();

	unsigned Get_Type() const { return TypeValue; }
	unsigned short Get_Index_Count() const { return IndexCount; }
	unsigned short Get_Index_Buffer_Offset() const { return IndexBufferOffset; }
	IndexBufferClass *Get_Index_Buffer() const { return IndexBuffer; }

	static void _Deinit();
	static void _Reset(bool frame_changed);
	static unsigned short Get_Default_Index_Count();

	class WriteLockClass
	{
		DynamicIBAccessClass *DynamicIBAccess;
		unsigned short *Indices;
		bool Locked;

	public:
		WriteLockClass(DynamicIBAccessClass *ib_access);
		~WriteLockClass();
		unsigned short *Get_Index_Array() { return Indices; }
		bool Is_Locked() const { return Locked; }
	};
};

class SortingIndexBufferClass : public IndexBufferClass
{
	W3DMPO_CODE(SortingIndexBufferClass)

	unsigned short *index_buffer;

protected:
	virtual ~SortingIndexBufferClass() override;

public:
	SortingIndexBufferClass(unsigned short index_count);
	unsigned short *Get_Index_Array() { return index_buffer; }
	const unsigned short *Get_Index_Array() const { return index_buffer; }
};
