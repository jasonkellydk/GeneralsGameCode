/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	DX9-only vertex-format encoding helpers. WW3D consumers must use
**	VertexFormat.h and must not include this file.
*/

#pragma once

#include "WWLib/always.h"
#include "WW3D2/VertexFormat.h"
#include <d3d9.h>

#ifdef WWDEBUG
#include "WWDebug/wwdebug.h"
#endif

class StringClass;

// These constants are retained only while the DX9 mesh renderer is being
// reduced to the neutral VertexFormat contract. They must not escape this
// backend directory.
enum
{
	DX9_FVF_XYZ = D3DFVF_XYZ,
	DX9_FVF_XYZN = D3DFVF_XYZ | D3DFVF_NORMAL,
	DX9_FVF_XYZNUV1 = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1,
	DX9_FVF_XYZNUV2 = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX2,
	DX9_FVF_XYZNDUV1 = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1 | D3DFVF_DIFFUSE,
	DX9_FVF_XYZNDUV2 = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX2 | D3DFVF_DIFFUSE,
	DX9_FVF_XYZDUV1 = D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE,
	DX9_FVF_XYZDUV2 = D3DFVF_XYZ | D3DFVF_TEX2 | D3DFVF_DIFFUSE,
	DX9_FVF_XYZUV1 = D3DFVF_XYZ | D3DFVF_TEX1,
	DX9_FVF_XYZUV2 = D3DFVF_XYZ | D3DFVF_TEX2,
	DX9_FVF_XYZNDUV1TG3 = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX4 |
		D3DFVF_TEXCOORDSIZE2(0) | D3DFVF_TEXCOORDSIZE3(1) |
		D3DFVF_TEXCOORDSIZE3(2) | D3DFVF_TEXCOORDSIZE3(3),
	DX9_FVF_XYZNUV2DMAP = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX3 |
		D3DFVF_TEXCOORDSIZE1(0) | D3DFVF_TEXCOORDSIZE4(1) |
		D3DFVF_TEXCOORDSIZE2(2),
	DX9_FVF_XYZNDCUBEMAP = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE
};

// This parser is backend-private. It understands the native DX9 FVF bitmask
// used by the legacy mesh renderer; it is not part of the WW3D API.
class FVFInfoClass
{
	W3DMPO_CODE(FVFInfoClass)

	mutable unsigned FVF;
	mutable unsigned fvf_size;
	unsigned location_offset;
	unsigned normal_offset;
	unsigned blend_offset;
	unsigned texcoord_offset[D3DDP_MAXTEXCOORD];
	unsigned diffuse_offset;
	unsigned specular_offset;

public:
	explicit FVFInfoClass(unsigned fvf);

	unsigned Get_Location_Offset() const { return location_offset; }
	unsigned Get_Normal_Offset() const { return normal_offset; }
#ifdef WWDEBUG
	unsigned Get_Tex_Offset(unsigned index) const
	{
		WWASSERT(index < D3DDP_MAXTEXCOORD);
		return texcoord_offset[index];
	}
#else
	unsigned Get_Tex_Offset(unsigned index) const { return texcoord_offset[index]; }
#endif
	unsigned Get_Diffuse_Offset() const { return diffuse_offset; }
	unsigned Get_Specular_Offset() const { return specular_offset; }
	unsigned Get_FVF() const { return FVF; }
	unsigned Get_FVF_Size() const { return fvf_size; }

	void Get_FVF_Name(StringClass &name) const;
	void Set_FVF(unsigned fvf) const { FVF = fvf; }
	void Set_FVF_Size(unsigned size) const { fvf_size = size; }
};

// Converts a native fixed-function vertex format into the neutral WW3D
// layout used by VertexBufferClass. This is intentionally implemented and
// declared only in the concrete backend.
RenderBackendVertexLayout RenderBackend_Vertex_Layout_From_Native_FVF(unsigned fvf);
