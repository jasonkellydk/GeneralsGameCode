/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : ww3d                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/dx9fvf.h                               $*
 *                                                                                             *
 *              Original Author:: Jani Penttinen                                               *
 *                                                                                             *
 *                      $Author:: Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 06/26/02 5:06p                                             $*
 *                                                                                             *
 *                    $Revision:: 7                                                          $*
 *                                                                                             *
 * 06/26/02 KM VB Vertex format update for shaders                                       *
 * 07/17/02 KM VB Vertex format update for displacement mapping                               *
 * 08/01/02 KM VB Vertex format update for cube mapping                               *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "VertexFormatMapper.h"
#include "WW3D2/VertexFormat.h"
#include "WWLib/wwstring.h"
#include <d3dx9core.h>

static unsigned Get_FVF_Vertex_Size(unsigned FVF)
{
	return D3DXGetFVFVertexSize(FVF);
}

FVFInfoClass::FVFInfoClass(unsigned FVF_)
	:
	FVF(FVF_),
	fvf_size(Get_FVF_Vertex_Size(FVF))
{
	location_offset=0;
	blend_offset=location_offset;

	if ((FVF&D3DFVF_XYZ)==D3DFVF_XYZ) blend_offset+=3*sizeof(float);
	normal_offset=blend_offset;

	if ( ((FVF&D3DFVF_XYZB4)==D3DFVF_XYZB4) &&
		  ((FVF&D3DFVF_LASTBETA_UBYTE4)==D3DFVF_LASTBETA_UBYTE4) ) normal_offset+=3*sizeof(float)+sizeof(DWORD);
	diffuse_offset=normal_offset;

	if ((FVF&D3DFVF_NORMAL)==D3DFVF_NORMAL) diffuse_offset+=3*sizeof(float);
	specular_offset=diffuse_offset;

	if ((FVF&D3DFVF_DIFFUSE)==D3DFVF_DIFFUSE) specular_offset+=sizeof(DWORD);
	texcoord_offset[0]=specular_offset;

	if ((FVF&D3DFVF_SPECULAR)==D3DFVF_SPECULAR) texcoord_offset[0]+=sizeof(DWORD);

	for (unsigned int i=1; i<D3DDP_MAXTEXCOORD; i++)
	{
		texcoord_offset[i]=texcoord_offset[i-1];

		if ((int(FVF)&D3DFVF_TEXCOORDSIZE1(i-1))==D3DFVF_TEXCOORDSIZE1(i-1)) texcoord_offset[i]+=sizeof(float);
		else if ((int(FVF)&D3DFVF_TEXCOORDSIZE2(i-1))==D3DFVF_TEXCOORDSIZE2(i-1)) texcoord_offset[i]+=2*sizeof(float);
		else if ((int(FVF)&D3DFVF_TEXCOORDSIZE3(i-1))==D3DFVF_TEXCOORDSIZE3(i-1)) texcoord_offset[i]+=3*sizeof(float);
		else if ((int(FVF)&D3DFVF_TEXCOORDSIZE4(i-1))==D3DFVF_TEXCOORDSIZE4(i-1)) texcoord_offset[i]+=4*sizeof(float);
	}
}

void FVFInfoClass::Get_FVF_Name(StringClass& fvfname) const
{
	switch (Get_FVF()) {
	case DX9_FVF_XYZ: fvfname="D3DFVF_XYZ"; break;
	case DX9_FVF_XYZN: fvfname="D3DFVF_XYZ|D3DFVF_NORMAL"; break;
	case DX9_FVF_XYZNUV1: fvfname="D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1"; break;
	case DX9_FVF_XYZNUV2: fvfname="D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX2"; break;
	case DX9_FVF_XYZNDUV1: fvfname="D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1|D3DFVF_DIFFUSE"; break;
	case DX9_FVF_XYZNDUV2: fvfname="D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX2|D3DFVF_DIFFUSE"; break;
	case DX9_FVF_XYZDUV1: fvfname="D3DFVF_XYZ|D3DFVF_TEX1|D3DFVF_DIFFUSE"; break;
	case DX9_FVF_XYZDUV2: fvfname="D3DFVF_XYZ|D3DFVF_TEX2|D3DFVF_DIFFUSE"; break;
	case DX9_FVF_XYZUV1: fvfname="D3DFVF_XYZ|D3DFVF_TEX1"; break;
	case DX9_FVF_XYZUV2: fvfname="D3DFVF_XYZ|D3DFVF_TEX2"; break;
	case DX9_FVF_XYZNDUV1TG3 : fvfname="(D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_TEX4|D3DFVF_TEXCOORDSIZE2(0)|D3DFVF_TEXCOORDSIZE3(1)|D3DFVF_TEXCOORDSIZE3(2)|D3DFVF_TEXCOORDSIZE3(3))"; break;
	case DX9_FVF_XYZNUV2DMAP :	fvfname="(D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX3|D3DFVF_TEXCOORDSIZE1(0)|D3DFVF_TEXCOORDSIZE4(1)|D3DFVF_TEXCOORDSIZE2(2))"; break;
	case DX9_FVF_XYZNDCUBEMAP : fvfname="(D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_TEX1|D3DFVFTEXCOORDSIZE3(0)"; break;
	default: fvfname="Unknown!";
	}
}

RenderBackendVertexLayout RenderBackend_Vertex_Layout_From_Native_FVF(unsigned fvf)
{
	RenderBackendVertexLayout layout;
	layout.transformed = (fvf & D3DFVF_XYZRHW) == D3DFVF_XYZRHW;
	layout.has_blend = (fvf & D3DFVF_XYZB4) == D3DFVF_XYZB4;
	layout.has_normal = (fvf & D3DFVF_NORMAL) == D3DFVF_NORMAL;
	layout.has_diffuse = (fvf & D3DFVF_DIFFUSE) == D3DFVF_DIFFUSE;
	layout.has_specular = (fvf & D3DFVF_SPECULAR) == D3DFVF_SPECULAR;
	layout.texture_count = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	if (layout.texture_count > RENDER_BACKEND_MAX_TEXTURE_COORDINATES)
	{
		layout.texture_count = RENDER_BACKEND_MAX_TEXTURE_COORDINATES;
	}

	for (unsigned index = 0; index < layout.texture_count; ++index)
	{
		if ((fvf & D3DFVF_TEXCOORDSIZE1(index)) == D3DFVF_TEXCOORDSIZE1(index))
		{
			layout.texture_dimensions[index] = 1;
		}
		else if ((fvf & D3DFVF_TEXCOORDSIZE3(index)) == D3DFVF_TEXCOORDSIZE3(index))
		{
			layout.texture_dimensions[index] = 3;
		}
		else if ((fvf & D3DFVF_TEXCOORDSIZE4(index)) == D3DFVF_TEXCOORDSIZE4(index))
		{
			layout.texture_dimensions[index] = 4;
		}
		else
		{
			layout.texture_dimensions[index] = 2;
		}
	}

	// Preserve the well-known WW3D formats where possible. Arbitrary mesh
	// formats remain Unknown but retain their complete neutral layout.
	switch (fvf)
	{
		case DX9_FVF_XYZ: layout.format = RenderBackendVertexFormat::Position; break;
		case DX9_FVF_XYZN: layout.format = RenderBackendVertexFormat::PositionNormal; break;
		case DX9_FVF_XYZNUV1: layout.format = RenderBackendVertexFormat::PositionNormalTexture; break;
		case DX9_FVF_XYZNUV2: layout.format = RenderBackendVertexFormat::PositionNormalTexture2; break;
		case DX9_FVF_XYZNDUV1: layout.format = RenderBackendVertexFormat::PositionNormalDiffuseTexture; break;
		case DX9_FVF_XYZNDUV2: layout.format = RenderBackendVertexFormat::PositionNormalDiffuseTexture2; break;
		case DX9_FVF_XYZDUV1: layout.format = RenderBackendVertexFormat::PositionDiffuseTexture; break;
		case DX9_FVF_XYZDUV2: layout.format = RenderBackendVertexFormat::PositionDiffuseTexture2; break;
		case DX9_FVF_XYZUV1: layout.format = RenderBackendVertexFormat::PositionTexture; break;
		case DX9_FVF_XYZUV2: layout.format = RenderBackendVertexFormat::PositionTexture2; break;
		case DX9_FVF_XYZNDUV1TG3:
			layout.format = RenderBackendVertexFormat::PositionNormalDiffuseTexture4TangentBasis;
			break;
		case DX9_FVF_XYZNUV2DMAP:
			layout.format = RenderBackendVertexFormat::PositionNormalTextureDisplacement;
			break;
		case DX9_FVF_XYZNDCUBEMAP:
			layout.format = RenderBackendVertexFormat::PositionNormalDiffuseCube;
			break;
		default:
			break;
	}

	return layout;
}
