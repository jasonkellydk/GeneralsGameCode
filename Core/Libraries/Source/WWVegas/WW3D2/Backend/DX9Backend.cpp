/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

#include "DX9Backend.h"
#include "RenderBackend.h"

#include "WW3D2/dx8wrapper.h"
#include "WW3D2/formconv.h"
#include "WW3D2/surfaceclass.h"
#include "WW3D2/texture.h"
#include "WWMath/vector3.h"
#include "WWMath/vector4.h"
#include "WW3D2/lightenvironment.h"
#include "WWDebug/wwdebug.h"

#include <cstring>
#include <d3dx9tex.h>

namespace
{
	class DX9BackendSurface final : public RenderBackendSurface
	{
	public:
		explicit DX9BackendSurface(IDirect3DSurface9 * surface) : Surface(surface) {}

		virtual ~DX9BackendSurface() override
		{
			if (Surface != nullptr)
			{
				Surface->Release();
			}
		}

		IDirect3DSurface9 *Surface;
	};

	class DX9BackendVertexBuffer final : public RenderBackendVertexBuffer
	{
	public:
		DX9BackendVertexBuffer(IDirect3DVertexBuffer9 * buffer,
			RenderBackendVertexFormat format) : Buffer(buffer), Format(format) {}

		virtual ~DX9BackendVertexBuffer() override
		{
			if (Buffer != nullptr)
			{
				Buffer->Release();
			}
		}

		IDirect3DVertexBuffer9 *Buffer;
		RenderBackendVertexFormat Format;
	};

	class DX9BackendIndexBuffer final : public RenderBackendIndexBuffer
	{
	public:
		explicit DX9BackendIndexBuffer(IDirect3DIndexBuffer9 * buffer) : Buffer(buffer) {}

		virtual ~DX9BackendIndexBuffer() override
		{
			if (Buffer != nullptr)
			{
				Buffer->Release();
			}
		}

		IDirect3DIndexBuffer9 *Buffer;
	};

	DX9BackendSurface * To_DX9_Surface(RenderBackendSurface * surface)
	{
		return static_cast<DX9BackendSurface *>(surface);
	}

	DX9BackendVertexBuffer * To_DX9_Vertex_Buffer(RenderBackendVertexBuffer * buffer)
	{
		return static_cast<DX9BackendVertexBuffer *>(buffer);
	}

	DX9BackendIndexBuffer * To_DX9_Index_Buffer(RenderBackendIndexBuffer * buffer)
	{
		return static_cast<DX9BackendIndexBuffer *>(buffer);
	}

	DWORD To_D3D_Vertex_Format(RenderBackendVertexFormat format)
	{
		switch (format)
		{
			case RenderBackendVertexFormat::Position: return D3DFVF_XYZ;
			case RenderBackendVertexFormat::PositionDiffuse: return D3DFVF_XYZ | D3DFVF_DIFFUSE;
			case RenderBackendVertexFormat::PositionTexture: return D3DFVF_XYZ | D3DFVF_TEX1;
			case RenderBackendVertexFormat::PositionDiffuseTexture: return D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;
			case RenderBackendVertexFormat::PositionTexture2: return D3DFVF_XYZ | D3DFVF_TEX2;
			case RenderBackendVertexFormat::PositionDiffuseTexture2: return D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2;
			case RenderBackendVertexFormat::PositionNormal: return D3DFVF_XYZ | D3DFVF_NORMAL;
			case RenderBackendVertexFormat::PositionNormalDiffuse: return D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE;
			case RenderBackendVertexFormat::PositionNormalTexture: return D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1;
			case RenderBackendVertexFormat::PositionNormalDiffuseTexture: return D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1;
			case RenderBackendVertexFormat::PositionNormalTexture2: return D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX2;
			case RenderBackendVertexFormat::PositionNormalDiffuseTexture2: return D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX2;
			case RenderBackendVertexFormat::TransformedPosition: return D3DFVF_XYZRHW;
			case RenderBackendVertexFormat::TransformedPositionDiffuse: return D3DFVF_XYZRHW | D3DFVF_DIFFUSE;
			case RenderBackendVertexFormat::TransformedPositionTexture: return D3DFVF_XYZRHW | D3DFVF_TEX1;
			case RenderBackendVertexFormat::TransformedPositionDiffuseTexture: return D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
			case RenderBackendVertexFormat::TransformedPositionTexture2: return D3DFVF_XYZRHW | D3DFVF_TEX2;
			case RenderBackendVertexFormat::TransformedPositionDiffuseTexture2: return D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX2;
			default:
				WWASSERT(false);
				return D3DFVF_XYZ;
		}
	}

	D3DPRIMITIVETYPE To_D3D_Primitive_Type(RenderBackendPrimitiveType primitive_type)
	{
		switch (primitive_type)
		{
			case RenderBackendPrimitiveType::PointList: return D3DPT_POINTLIST;
			case RenderBackendPrimitiveType::LineList: return D3DPT_LINELIST;
			case RenderBackendPrimitiveType::LineStrip: return D3DPT_LINESTRIP;
			case RenderBackendPrimitiveType::TriangleList: return D3DPT_TRIANGLELIST;
			case RenderBackendPrimitiveType::TriangleStrip: return D3DPT_TRIANGLESTRIP;
			default:
				WWASSERT(false);
				return D3DPT_TRIANGLELIST;
		}
	}

	DWORD To_D3D_Lock_Flags(RenderBackendBufferLockMode mode)
	{
		switch (mode)
		{
			case RenderBackendBufferLockMode::Normal: return 0;
			case RenderBackendBufferLockMode::Discard: return D3DLOCK_DISCARD;
			case RenderBackendBufferLockMode::NoOverwrite: return D3DLOCK_NOOVERWRITE;
			default:
				WWASSERT(false);
				return 0;
		}
	}

	D3DTRANSFORMSTATETYPE To_D3D_Transform(RenderBackendTransform transform)
	{
		switch (transform)
		{
			case RenderBackendTransform::World: return D3DTS_WORLD;
			case RenderBackendTransform::View: return D3DTS_VIEW;
			case RenderBackendTransform::Projection: return D3DTS_PROJECTION;
			case RenderBackendTransform::Texture0: return D3DTS_TEXTURE0;
			case RenderBackendTransform::Texture1: return D3DTS_TEXTURE1;
			case RenderBackendTransform::Texture2: return D3DTS_TEXTURE2;
			case RenderBackendTransform::Texture3: return D3DTS_TEXTURE3;
			case RenderBackendTransform::Texture4: return D3DTS_TEXTURE4;
			case RenderBackendTransform::Texture5: return D3DTS_TEXTURE5;
			case RenderBackendTransform::Texture6: return D3DTS_TEXTURE6;
			case RenderBackendTransform::Texture7: return D3DTS_TEXTURE7;
			default:
				WWASSERT(false);
				return D3DTS_WORLD;
		}
	}

	D3DFILLMODE To_D3D_Fill_Mode(RenderBackendFillMode mode)
	{
		switch (mode)
		{
			case RenderBackendFillMode::Point: return D3DFILL_POINT;
			case RenderBackendFillMode::Wireframe: return D3DFILL_WIREFRAME;
			case RenderBackendFillMode::Solid: return D3DFILL_SOLID;
			default:
				WWASSERT(false);
				return D3DFILL_SOLID;
		}
	}

	D3DCULL To_D3D_Cull_Mode(RenderBackendCullMode mode)
	{
		switch (mode)
		{
			case RenderBackendCullMode::None: return D3DCULL_NONE;
			case RenderBackendCullMode::Clockwise: return D3DCULL_CW;
			case RenderBackendCullMode::CounterClockwise: return D3DCULL_CCW;
			default:
				WWASSERT(false);
				return D3DCULL_NONE;
		}
	}

	D3DSHADEMODE To_D3D_Shade_Mode(RenderBackendShadeMode mode)
	{
		switch (mode)
		{
			case RenderBackendShadeMode::Flat: return D3DSHADE_FLAT;
			case RenderBackendShadeMode::Gouraud: return D3DSHADE_GOURAUD;
			case RenderBackendShadeMode::Phong: return D3DSHADE_PHONG;
			default:
				WWASSERT(false);
				return D3DSHADE_GOURAUD;
		}
	}

	D3DMATERIALCOLORSOURCE To_D3D_Material_Source(RenderBackendMaterialSource source)
	{
		switch (source)
		{
			case RenderBackendMaterialSource::MaterialValue: return D3DMCS_MATERIAL;
			case RenderBackendMaterialSource::Color1: return D3DMCS_COLOR1;
			case RenderBackendMaterialSource::Color2: return D3DMCS_COLOR2;
			default:
				WWASSERT(false);
				return D3DMCS_MATERIAL;
		}
	}

	D3DBLENDOP To_D3D_Blend_Operation(RenderBackendBlendOperation operation)
	{
		switch (operation)
		{
			case RenderBackendBlendOperation::Add: return D3DBLENDOP_ADD;
			case RenderBackendBlendOperation::Subtract: return D3DBLENDOP_SUBTRACT;
			case RenderBackendBlendOperation::ReverseSubtract: return D3DBLENDOP_REVSUBTRACT;
			case RenderBackendBlendOperation::Minimum: return D3DBLENDOP_MIN;
			case RenderBackendBlendOperation::Maximum: return D3DBLENDOP_MAX;
			default:
				WWASSERT(false);
				return D3DBLENDOP_ADD;
		}
	}

	D3DBLEND To_D3D_Blend_Factor(RenderBackendBlendFactor factor)
	{
		switch (factor)
		{
			case RenderBackendBlendFactor::Zero: return D3DBLEND_ZERO;
			case RenderBackendBlendFactor::One: return D3DBLEND_ONE;
			case RenderBackendBlendFactor::SourceColor: return D3DBLEND_SRCCOLOR;
			case RenderBackendBlendFactor::InverseSourceColor: return D3DBLEND_INVSRCCOLOR;
			case RenderBackendBlendFactor::SourceAlpha: return D3DBLEND_SRCALPHA;
			case RenderBackendBlendFactor::InverseSourceAlpha: return D3DBLEND_INVSRCALPHA;
			case RenderBackendBlendFactor::DestinationAlpha: return D3DBLEND_DESTALPHA;
			case RenderBackendBlendFactor::InverseDestinationAlpha: return D3DBLEND_INVDESTALPHA;
			case RenderBackendBlendFactor::DestinationColor: return D3DBLEND_DESTCOLOR;
			case RenderBackendBlendFactor::InverseDestinationColor: return D3DBLEND_INVDESTCOLOR;
			default:
				WWASSERT(false);
				return D3DBLEND_ONE;
		}
	}

	D3DCMPFUNC To_D3D_Compare_Function(RenderBackendCompareFunction function)
	{
		switch (function)
		{
			case RenderBackendCompareFunction::Never: return D3DCMP_NEVER;
			case RenderBackendCompareFunction::Less: return D3DCMP_LESS;
			case RenderBackendCompareFunction::Equal: return D3DCMP_EQUAL;
			case RenderBackendCompareFunction::LessEqual: return D3DCMP_LESSEQUAL;
			case RenderBackendCompareFunction::Greater: return D3DCMP_GREATER;
			case RenderBackendCompareFunction::NotEqual: return D3DCMP_NOTEQUAL;
			case RenderBackendCompareFunction::GreaterEqual: return D3DCMP_GREATEREQUAL;
			case RenderBackendCompareFunction::Always: return D3DCMP_ALWAYS;
			default:
				WWASSERT(false);
				return D3DCMP_ALWAYS;
		}
	}

	D3DSTENCILOP To_D3D_Stencil_Operation(RenderBackendStencilOperation operation)
	{
		switch (operation)
		{
			case RenderBackendStencilOperation::Keep: return D3DSTENCILOP_KEEP;
			case RenderBackendStencilOperation::Zero: return D3DSTENCILOP_ZERO;
			case RenderBackendStencilOperation::Replace: return D3DSTENCILOP_REPLACE;
			case RenderBackendStencilOperation::IncrementSaturate: return D3DSTENCILOP_INCRSAT;
			case RenderBackendStencilOperation::DecrementSaturate: return D3DSTENCILOP_DECRSAT;
			case RenderBackendStencilOperation::Invert: return D3DSTENCILOP_INVERT;
			case RenderBackendStencilOperation::Increment: return D3DSTENCILOP_INCR;
			case RenderBackendStencilOperation::Decrement: return D3DSTENCILOP_DECR;
			default:
				WWASSERT(false);
				return D3DSTENCILOP_KEEP;
		}
	}

	DWORD To_D3D_Color_Write_Mask(RenderBackendColorWriteMask mask)
	{
		const unsigned backend_mask = static_cast<unsigned>(mask);
		DWORD d3d_mask = 0;
		if (backend_mask & static_cast<unsigned>(RenderBackendColorWriteMask::Red))
			d3d_mask |= D3DCOLORWRITEENABLE_RED;
		if (backend_mask & static_cast<unsigned>(RenderBackendColorWriteMask::Green))
			d3d_mask |= D3DCOLORWRITEENABLE_GREEN;
		if (backend_mask & static_cast<unsigned>(RenderBackendColorWriteMask::Blue))
			d3d_mask |= D3DCOLORWRITEENABLE_BLUE;
		if (backend_mask & static_cast<unsigned>(RenderBackendColorWriteMask::Alpha))
			d3d_mask |= D3DCOLORWRITEENABLE_ALPHA;
		return d3d_mask;
	}

	RenderBackendColorWriteMask From_D3D_Color_Write_Mask(DWORD mask)
	{
		unsigned backend_mask = 0;
		if (mask & D3DCOLORWRITEENABLE_RED)
			backend_mask |= static_cast<unsigned>(RenderBackendColorWriteMask::Red);
		if (mask & D3DCOLORWRITEENABLE_GREEN)
			backend_mask |= static_cast<unsigned>(RenderBackendColorWriteMask::Green);
		if (mask & D3DCOLORWRITEENABLE_BLUE)
			backend_mask |= static_cast<unsigned>(RenderBackendColorWriteMask::Blue);
		if (mask & D3DCOLORWRITEENABLE_ALPHA)
			backend_mask |= static_cast<unsigned>(RenderBackendColorWriteMask::Alpha);
		return static_cast<RenderBackendColorWriteMask>(backend_mask);
	}

	DWORD To_D3D_Texture_Operation_Capability(RenderBackendTextureOperation operation)
	{
		switch (operation)
		{
			case RenderBackendTextureOperation::Disable: return 0;
			case RenderBackendTextureOperation::SelectArgument1: return D3DTEXOPCAPS_SELECTARG1;
			case RenderBackendTextureOperation::SelectArgument2: return D3DTEXOPCAPS_SELECTARG2;
			case RenderBackendTextureOperation::Modulate: return D3DTEXOPCAPS_MODULATE;
			case RenderBackendTextureOperation::AddSmooth: return D3DTEXOPCAPS_ADDSMOOTH;
			case RenderBackendTextureOperation::Add: return D3DTEXOPCAPS_ADD;
			case RenderBackendTextureOperation::Subtract: return D3DTEXOPCAPS_SUBTRACT;
			case RenderBackendTextureOperation::BlendTextureAlpha: return D3DTEXOPCAPS_BLENDTEXTUREALPHA;
			case RenderBackendTextureOperation::BlendCurrentAlpha: return D3DTEXOPCAPS_BLENDCURRENTALPHA;
			case RenderBackendTextureOperation::AddSigned: return D3DTEXOPCAPS_ADDSIGNED;
			case RenderBackendTextureOperation::AddSigned2X: return D3DTEXOPCAPS_ADDSIGNED2X;
			case RenderBackendTextureOperation::Modulate2X: return D3DTEXOPCAPS_MODULATE2X;
			case RenderBackendTextureOperation::ModulateAlphaAddColor: return D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR;
			case RenderBackendTextureOperation::BumpEnvironmentMap: return D3DTEXOPCAPS_BUMPENVMAP;
			case RenderBackendTextureOperation::BumpEnvironmentMapLuminance: return D3DTEXOPCAPS_BUMPENVMAPLUMINANCE;
			case RenderBackendTextureOperation::DotProduct3: return D3DTEXOPCAPS_DOTPRODUCT3;
			case RenderBackendTextureOperation::MultiplyAdd: return D3DTEXOPCAPS_MULTIPLYADD;
			default:
				WWASSERT(false);
				return 0;
		}
	}

	D3DTEXTUREOP To_D3D_Texture_Operation(RenderBackendTextureOperation operation)
	{
		switch (operation)
		{
			case RenderBackendTextureOperation::Disable: return D3DTOP_DISABLE;
			case RenderBackendTextureOperation::SelectArgument1: return D3DTOP_SELECTARG1;
			case RenderBackendTextureOperation::SelectArgument2: return D3DTOP_SELECTARG2;
			case RenderBackendTextureOperation::Modulate: return D3DTOP_MODULATE;
			case RenderBackendTextureOperation::AddSmooth: return D3DTOP_ADDSMOOTH;
			case RenderBackendTextureOperation::Add: return D3DTOP_ADD;
			case RenderBackendTextureOperation::Subtract: return D3DTOP_SUBTRACT;
			case RenderBackendTextureOperation::BlendTextureAlpha: return D3DTOP_BLENDTEXTUREALPHA;
			case RenderBackendTextureOperation::BlendCurrentAlpha: return D3DTOP_BLENDCURRENTALPHA;
			case RenderBackendTextureOperation::AddSigned: return D3DTOP_ADDSIGNED;
			case RenderBackendTextureOperation::AddSigned2X: return D3DTOP_ADDSIGNED2X;
			case RenderBackendTextureOperation::Modulate2X: return D3DTOP_MODULATE2X;
			case RenderBackendTextureOperation::ModulateAlphaAddColor: return D3DTOP_MODULATEALPHA_ADDCOLOR;
			case RenderBackendTextureOperation::BumpEnvironmentMap: return D3DTOP_BUMPENVMAP;
			case RenderBackendTextureOperation::BumpEnvironmentMapLuminance: return D3DTOP_BUMPENVMAPLUMINANCE;
			case RenderBackendTextureOperation::DotProduct3: return D3DTOP_DOTPRODUCT3;
			case RenderBackendTextureOperation::MultiplyAdd: return D3DTOP_MULTIPLYADD;
			default:
				WWASSERT(false);
				return D3DTOP_DISABLE;
		}
	}

	DWORD To_D3D_Texture_Argument(RenderBackendTextureArgument argument)
	{
		switch (argument)
		{
			case RenderBackendTextureArgument::Current: return D3DTA_CURRENT;
			case RenderBackendTextureArgument::Diffuse: return D3DTA_DIFFUSE;
			case RenderBackendTextureArgument::Texture: return D3DTA_TEXTURE;
			case RenderBackendTextureArgument::TextureFactor: return D3DTA_TFACTOR;
			case RenderBackendTextureArgument::CurrentAlpha: return D3DTA_CURRENT | D3DTA_ALPHAREPLICATE;
			case RenderBackendTextureArgument::DiffuseAlpha: return D3DTA_DIFFUSE | D3DTA_ALPHAREPLICATE;
			case RenderBackendTextureArgument::TextureAlpha: return D3DTA_TEXTURE | D3DTA_ALPHAREPLICATE;
			case RenderBackendTextureArgument::TextureFactorAlpha: return D3DTA_TFACTOR | D3DTA_ALPHAREPLICATE;
			default:
				WWASSERT(false);
				return D3DTA_CURRENT;
		}
	}

	D3DTEXTURETRANSFORMFLAGS To_D3D_Texture_Transform_Flags(RenderBackendTextureTransformFlags flags)
	{
		switch (flags)
		{
			case RenderBackendTextureTransformFlags::Disabled: return D3DTTFF_DISABLE;
			case RenderBackendTextureTransformFlags::Count2: return D3DTTFF_COUNT2;
			case RenderBackendTextureTransformFlags::Count3: return D3DTTFF_COUNT3;
			case RenderBackendTextureTransformFlags::ProjectedCount3:
				return static_cast<D3DTEXTURETRANSFORMFLAGS>(D3DTTFF_PROJECTED | D3DTTFF_COUNT3);
			default:
				WWASSERT(false);
				return D3DTTFF_DISABLE;
		}
	}

	D3DTEXTUREADDRESS To_D3D_Texture_Address_Mode(RenderBackendTextureAddressMode mode)
	{
		switch (mode)
		{
			case RenderBackendTextureAddressMode::Wrap: return D3DTADDRESS_WRAP;
			case RenderBackendTextureAddressMode::Clamp: return D3DTADDRESS_CLAMP;
			default:
				WWASSERT(false);
				return D3DTADDRESS_WRAP;
		}
	}

	D3DTEXTUREFILTERTYPE To_D3D_Texture_Filter(RenderBackendTextureFilter filter)
	{
		switch (filter)
		{
			case RenderBackendTextureFilter::None: return D3DTEXF_NONE;
			case RenderBackendTextureFilter::Point: return D3DTEXF_POINT;
			case RenderBackendTextureFilter::Linear: return D3DTEXF_LINEAR;
			case RenderBackendTextureFilter::Anisotropic: return D3DTEXF_ANISOTROPIC;
			default:
				WWASSERT(false);
				return D3DTEXF_NONE;
		}
	}

	DWORD Float_To_Dword(float value)
	{
		DWORD result;
		std::memcpy(&result, &value, sizeof(result));
		return result;
	}
}

IRenderBackend *Create_Render_Backend(void * window, bool lite)
{
	return DX9Backend::Create(window, lite);
}

DX9Backend::DX9Backend(bool lite) : Lite(lite)
{
}

DX9Backend::~DX9Backend()
{
	if (!Lite)
	{
		DX8Wrapper::Shutdown();
	}
}

DX9Backend *DX9Backend::Create(void * window, bool lite)
{
	Init_D3D_To_WW3_Conversion();
	WWDEBUG_SAY(("Init DX9 backend"));
	if (!DX8Wrapper::Init(window, lite))
	{
		// DX8Wrapper::Init can fail after partially acquiring D3D9 resources.
		DX8Wrapper::Shutdown();
		return nullptr;
	}

	return new DX9Backend(lite);
}

bool DX9Backend::Is_Initted() const
{
	return DX8Wrapper::Is_Initted();
}

bool DX9Backend::Is_Render_To_Texture() const
{
	return DX8Wrapper::Is_Render_To_Texture();
}

bool DX9Backend::Has_Stencil() const
{
	return DX8Wrapper::Has_Stencil();
}

bool DX9Backend::Supports_TnL() const
{
	return DX8Wrapper::Get_Current_Caps()->Support_TnL();
}

bool DX9Backend::Supports_DXTC() const
{
	return DX8Wrapper::Get_Current_Caps()->Support_DXTC();
}

bool DX9Backend::Supports_NPatches() const
{
	return DX8Wrapper::Get_Current_Caps()->Support_NPatches();
}

bool DX9Backend::Supports_Bump_Envmap() const
{
	return DX8Wrapper::Get_Current_Caps()->Support_Bump_Envmap();
}

bool DX9Backend::Supports_Bump_Envmap_Luminance() const
{
	return DX8Wrapper::Get_Current_Caps()->Support_Bump_Envmap_Luminance();
}

bool DX9Backend::Supports_Z_Bias() const
{
	return DX8Wrapper::Get_Current_Caps()->Support_ZBias();
}

bool DX9Backend::Supports_Anisotropic_Filtering() const
{
	return DX8Wrapper::Get_Current_Caps()->Support_Anisotropic_Filtering();
}

bool DX9Backend::Supports_Modulate_Alpha_Add_Color() const
{
	return DX8Wrapper::Get_Current_Caps()->Support_ModAlphaAddClr();
}

bool DX9Backend::Supports_Dot3() const
{
	return DX8Wrapper::Get_Current_Caps()->Support_Dot3();
}

bool DX9Backend::Supports_Point_Sprites() const
{
	return DX8Wrapper::Get_Current_Caps()->Support_PointSprites();
}

bool DX9Backend::Supports_Cubemaps() const
{
	return DX8Wrapper::Get_Current_Caps()->Support_Cubemaps();
}

bool DX9Backend::Supports_Color_Write_Mask() const
{
	return (DX8Wrapper::Get_Current_Caps()->Get_DX8_Caps().PrimitiveMiscCaps &
		D3DPMISCCAPS_COLORWRITEENABLE) != 0;
}

bool DX9Backend::Supports_Texture_Operation(RenderBackendTextureOperation operation) const
{
	if (operation == RenderBackendTextureOperation::Disable)
		return true;

	return (DX8Wrapper::Get_Current_Caps()->Get_DX8_Caps().TextureOpCaps &
		To_D3D_Texture_Operation_Capability(operation)) != 0;
}

bool DX9Backend::Supports_Texture_Filter(RenderBackendTextureFilterType type,
	RenderBackendTextureFilter filter) const
{
	if (filter == RenderBackendTextureFilter::None)
	{
		return true;
	}

	DWORD capability = 0;
	switch (type)
	{
		case RenderBackendTextureFilterType::Minification:
			switch (filter)
			{
				case RenderBackendTextureFilter::Point: capability = D3DPTFILTERCAPS_MINFPOINT; break;
				case RenderBackendTextureFilter::Linear: capability = D3DPTFILTERCAPS_MINFLINEAR; break;
				case RenderBackendTextureFilter::Anisotropic: capability = D3DPTFILTERCAPS_MINFANISOTROPIC; break;
				default: break;
			}
			break;
		case RenderBackendTextureFilterType::Magnification:
			switch (filter)
			{
				case RenderBackendTextureFilter::Point: capability = D3DPTFILTERCAPS_MAGFPOINT; break;
				case RenderBackendTextureFilter::Linear: capability = D3DPTFILTERCAPS_MAGFLINEAR; break;
				case RenderBackendTextureFilter::Anisotropic: capability = D3DPTFILTERCAPS_MAGFANISOTROPIC; break;
				default: break;
			}
			break;
		case RenderBackendTextureFilterType::MipMap:
			switch (filter)
			{
				case RenderBackendTextureFilter::Point: capability = D3DPTFILTERCAPS_MIPFPOINT; break;
				case RenderBackendTextureFilter::Linear: capability = D3DPTFILTERCAPS_MIPFLINEAR; break;
				default: break;
			}
			break;
		default:
			WWASSERT(false);
			return false;
	}

	return capability != 0 &&
		(DX8Wrapper::Get_Current_Caps()->Get_DX8_Caps().TextureFilterCaps & capability) != 0;
}

bool DX9Backend::Is_Fog_Allowed() const
{
	return DX8Wrapper::Get_Current_Caps()->Is_Fog_Allowed();
}

bool DX9Backend::Is_Fog_Enabled() const
{
	return DX8Wrapper::Get_Fog_Enable();
}

unsigned DX9Backend::Get_Fog_Color() const
{
	return DX8Wrapper::Get_Fog_Color();
}

bool DX9Backend::Supports_Texture_Format(WW3DFormat format) const
{
	return DX8Wrapper::Get_Current_Caps()->Support_Texture_Format(format);
}

bool DX9Backend::Supports_Render_To_Texture_Format(WW3DFormat format) const
{
	return DX8Wrapper::Get_Current_Caps()->Support_Render_To_Texture_Format(format);
}

bool DX9Backend::Supports_Depth_Stencil_Format(WW3DZFormat format) const
{
	return DX8Wrapper::Get_Current_Caps()->Support_Depth_Stencil_Format(format);
}

WW3DFormat DX9Backend::Get_Back_Buffer_Format() const
{
	return DX8Wrapper::getBackBufferFormat();
}

bool DX9Backend::Is_Device_Ready() const
{
	return Get_Device_Status() == RenderBackendDeviceStatus::Ready;
}

RenderBackendDeviceStatus DX9Backend::Get_Device_Status() const
{
	IDirect3DDevice9 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device == nullptr)
	{
		return RenderBackendDeviceStatus::Error;
	}

	switch (device->TestCooperativeLevel())
	{
		case D3D_OK: return RenderBackendDeviceStatus::Ready;
		case D3DERR_DEVICELOST: return RenderBackendDeviceStatus::Lost;
		case D3DERR_DEVICENOTRESET: return RenderBackendDeviceStatus::NeedsReset;
		default: return RenderBackendDeviceStatus::Error;
	}
}

int DX9Backend::Get_Max_Textures_Per_Pass() const
{
	return DX8Wrapper::Get_Current_Caps()->Get_Max_Textures_Per_Pass();
}

bool DX9Backend::Is_3DFX_Voodoo3() const
{
	const DX8Caps *caps = DX8Wrapper::Get_Current_Caps();
	return caps->Get_Vendor() == DX8Caps::VENDOR_3DFX &&
		caps->Get_Device() == DX8Caps::DEVICE_3DFX_VOODOO_3;
}

unsigned DX9Backend::Pack_Color(const Vector4 & color) const
{
	return DX8Wrapper::Convert_Color(color);
}

unsigned DX9Backend::Pack_Color(const Vector3 & color, float alpha) const
{
	return DX8Wrapper::Convert_Color(color, alpha);
}

unsigned DX9Backend::Pack_Color_Clamped(const Vector4 & color) const
{
	return DX8Wrapper::Convert_Color_Clamp(color);
}

Vector4 DX9Backend::Unpack_Color(unsigned color) const
{
	return DX8Wrapper::Convert_Color(color);
}

bool DX9Backend::Is_Triangle_Draw_Enabled() const
{
	return DX8Wrapper::_Is_Triangle_Draw_Enabled();
}

void DX9Backend::Set_Triangle_Draw_Enabled(bool enable)
{
	DX8Wrapper::_Enable_Triangle_Draw(enable);
}

bool DX9Backend::Set_Render_Device(const char * device_name,
	int width, int height, int bits, int windowed, bool resize_window)
{
	return DX8Wrapper::Set_Render_Device(device_name, width, height, bits,
		windowed, resize_window);
}

bool DX9Backend::Set_Render_Device(int device, int width, int height, int bits,
	int windowed, bool resize_window, bool reset_device, bool restore_assets)
{
	return DX8Wrapper::Set_Render_Device(device, width, height, bits, windowed,
		resize_window, reset_device, restore_assets);
}

bool DX9Backend::Set_Any_Render_Device()
{
	return DX8Wrapper::Set_Any_Render_Device();
}

bool DX9Backend::Set_Next_Render_Device()
{
	return DX8Wrapper::Set_Next_Render_Device();
}

bool DX9Backend::Is_Windowed() const
{
	return DX8Wrapper::Is_Windowed();
}

bool DX9Backend::Toggle_Windowed()
{
	return DX8Wrapper::Toggle_Windowed();
}

int DX9Backend::Get_Render_Device() const
{
	return DX8Wrapper::Get_Render_Device();
}

const RenderDeviceDescClass & DX9Backend::Get_Render_Device_Desc(int device) const
{
	return DX8Wrapper::Get_Render_Device_Desc(device);
}

int DX9Backend::Get_Render_Device_Count() const
{
	return DX8Wrapper::Get_Render_Device_Count();
}

const char * DX9Backend::Get_Render_Device_Name(int device) const
{
	return DX8Wrapper::Get_Render_Device_Name(device);
}

bool DX9Backend::Set_Device_Resolution(int width, int height, int bits,
	int windowed, bool resize_window)
{
	return DX8Wrapper::Set_Device_Resolution(width, height, bits, windowed,
		resize_window);
}

void DX9Backend::Get_Device_Resolution(int & width, int & height, int & bits,
	bool & windowed) const
{
	DX8Wrapper::Get_Device_Resolution(width, height, bits, windowed);
}

void DX9Backend::Get_Render_Target_Resolution(int & width, int & height,
	int & bits, bool & windowed) const
{
	DX8Wrapper::Get_Render_Target_Resolution(width, height, bits, windowed);
}

int DX9Backend::Get_Device_Resolution_Width() const
{
	return DX8Wrapper::Get_Device_Resolution_Width();
}

int DX9Backend::Get_Device_Resolution_Height() const
{
	return DX8Wrapper::Get_Device_Resolution_Height();
}

void DX9Backend::Set_Swap_Interval(int swap)
{
	DX8Wrapper::Set_Swap_Interval(swap);
}

int DX9Backend::Get_Swap_Interval() const
{
	return DX8Wrapper::Get_Swap_Interval();
}

bool DX9Backend::Reset_Device(bool reload_assets)
{
	return DX8Wrapper::Reset_Device(reload_assets);
}

bool DX9Backend::Registry_Save_Render_Device(const char * sub_key)
{
	return DX8Wrapper::Registry_Save_Render_Device(sub_key);
}

bool DX9Backend::Registry_Save_Render_Device(const char * sub_key, int device,
	int width, int height, int depth, bool windowed, int texture_depth)
{
	return DX8Wrapper::Registry_Save_Render_Device(sub_key, device, width, height,
		depth, windowed, texture_depth);
}

bool DX9Backend::Registry_Load_Render_Device(const char * sub_key, bool resize_window)
{
	return DX8Wrapper::Registry_Load_Render_Device(sub_key, resize_window);
}

bool DX9Backend::Registry_Load_Render_Device(const char * sub_key, char * device,
	int device_len, int & width, int & height, int & depth, int & windowed,
	int & texture_depth)
{
	return DX8Wrapper::Registry_Load_Render_Device(sub_key, device, device_len,
		width, height, depth, windowed, texture_depth);
}

void DX9Backend::Set_Texture_Bitdepth(int depth)
{
	DX8Wrapper::Set_Texture_Bitdepth(depth);
}

int DX9Backend::Get_Texture_Bitdepth() const
{
	return DX8Wrapper::Get_Texture_Bitdepth();
}

void DX9Backend::Set_Multisample_Mode(RenderBackendMultisampleMode mode)
{
	D3DMULTISAMPLE_TYPE d3d_mode = D3DMULTISAMPLE_NONE;
	switch (mode)
	{
		case RenderBackendMultisampleMode::None: d3d_mode = D3DMULTISAMPLE_NONE; break;
		case RenderBackendMultisampleMode::Samples2: d3d_mode = D3DMULTISAMPLE_2_SAMPLES; break;
		case RenderBackendMultisampleMode::Samples4: d3d_mode = D3DMULTISAMPLE_4_SAMPLES; break;
		case RenderBackendMultisampleMode::Samples8: d3d_mode = D3DMULTISAMPLE_8_SAMPLES; break;
		default:
			WWASSERT(false);
			break;
	}
	DX8Wrapper::Set_MSAA_Mode(d3d_mode);
}

RenderBackendMultisampleMode DX9Backend::Get_Multisample_Mode() const
{
	switch (DX8Wrapper::Get_MSAA_Mode())
	{
		case D3DMULTISAMPLE_2_SAMPLES: return RenderBackendMultisampleMode::Samples2;
		case D3DMULTISAMPLE_4_SAMPLES: return RenderBackendMultisampleMode::Samples4;
		case D3DMULTISAMPLE_8_SAMPLES: return RenderBackendMultisampleMode::Samples8;
		case D3DMULTISAMPLE_NONE:
		default: return RenderBackendMultisampleMode::None;
	}
}

void DX9Backend::Set_Gamma(float gamma, float bright, float contrast,
	bool calibrate, bool uselimit)
{
	DX8Wrapper::Set_Gamma(gamma, bright, contrast, calibrate, uselimit);
}

void DX9Backend::Begin_Scene()
{
	DX8Wrapper::Begin_Scene();
}

void DX9Backend::End_Scene(bool flip_frame)
{
	DX8Wrapper::End_Scene(flip_frame);
}

void DX9Backend::Flip_To_Primary()
{
	DX8Wrapper::Flip_To_Primary();
}

void DX9Backend::Clear(bool clear_color, bool clear_z_stencil,
	const Vector3 & color, float dest_alpha, float z,
	unsigned int stencil)
{
	DX8Wrapper::Clear(clear_color, clear_z_stencil, color, dest_alpha, z, stencil);
}

void DX9Backend::Set_Viewport(const RenderBackendViewport & viewport)
{
	D3DVIEWPORT9 vp;
	vp.X = viewport.x;
	vp.Y = viewport.y;
	vp.Width = viewport.width;
	vp.Height = viewport.height;
	vp.MinZ = viewport.min_z;
	vp.MaxZ = viewport.max_z;
	DX8Wrapper::Set_Viewport(&vp);
}

void DX9Backend::Invalidate_Cached_Render_States()
{
	DX8Wrapper::Invalidate_Cached_Render_States();
}

void DX9Backend::Set_Ambient(const Vector3 & color)
{
	DX8Wrapper::Set_Ambient(color);
}

void DX9Backend::Set_Light_Environment(LightEnvironmentClass * light_env)
{
	DX8Wrapper::Set_Light_Environment(light_env);
}

void DX9Backend::Set_Fog(bool enable, const Vector3 & color, float start, float end)
{
	DX8Wrapper::Set_Fog(enable, color, start, end);
}

void DX9Backend::Set_Material_Values(const RenderBackendMaterial & material)
{
	D3DMATERIAL9 d3d_material = {};
	d3d_material.Diffuse.r = material.diffuse[0];
	d3d_material.Diffuse.g = material.diffuse[1];
	d3d_material.Diffuse.b = material.diffuse[2];
	d3d_material.Diffuse.a = material.diffuse[3];
	d3d_material.Ambient.r = material.ambient[0];
	d3d_material.Ambient.g = material.ambient[1];
	d3d_material.Ambient.b = material.ambient[2];
	d3d_material.Ambient.a = material.ambient[3];
	d3d_material.Specular.r = material.specular[0];
	d3d_material.Specular.g = material.specular[1];
	d3d_material.Specular.b = material.specular[2];
	d3d_material.Specular.a = material.specular[3];
	d3d_material.Emissive.r = material.emissive[0];
	d3d_material.Emissive.g = material.emissive[1];
	d3d_material.Emissive.b = material.emissive[2];
	d3d_material.Emissive.a = material.emissive[3];
	d3d_material.Power = material.power;
	DX8Wrapper::Set_DX8_Material(&d3d_material);
}

void DX9Backend::Set_Fill_Mode(RenderBackendFillMode mode)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_FILLMODE, To_D3D_Fill_Mode(mode));
}

void DX9Backend::Set_Color_Write_Mask(RenderBackendColorWriteMask mask)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE,
		To_D3D_Color_Write_Mask(mask));
}

RenderBackendColorWriteMask DX9Backend::Get_Color_Write_Mask() const
{
	DWORD mask = 0;
	DX8Wrapper::_Get_D3D_Device8()->GetRenderState(D3DRS_COLORWRITEENABLE, &mask);
	return From_D3D_Color_Write_Mask(mask);
}

void DX9Backend::Set_Alpha_Blend_Enabled(bool enable)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Blend_Operation(RenderBackendBlendOperation operation)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_BLENDOP, To_D3D_Blend_Operation(operation));
}

void DX9Backend::Set_Blend_Factors(RenderBackendBlendFactor source,
	RenderBackendBlendFactor destination)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND, To_D3D_Blend_Factor(source));
	DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, To_D3D_Blend_Factor(destination));
}

void DX9Backend::Set_Alpha_Test_Enabled(bool enable)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Alpha_Test_Function(RenderBackendCompareFunction function)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHAFUNC, To_D3D_Compare_Function(function));
}

void DX9Backend::Set_Alpha_Test_Reference(unsigned reference)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHAREF, reference);
}

void DX9Backend::Set_Fog_Enabled(bool enable)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGENABLE, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Fog_Color(unsigned color)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGCOLOR, color);
}

void DX9Backend::Set_Depth_Bias(unsigned bias)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZBIAS, bias);
}

void DX9Backend::Set_Texture_Factor(unsigned color)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_TEXTUREFACTOR, color);
}

void DX9Backend::Set_Depth_Test_Enabled(bool enable)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Depth_Write_Enabled(bool enable)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Depth_Function(RenderBackendCompareFunction function)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC, To_D3D_Compare_Function(function));
}

void DX9Backend::Set_Cull_Mode(RenderBackendCullMode mode)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_CULLMODE, To_D3D_Cull_Mode(mode));
}

void DX9Backend::Set_Shade_Mode(RenderBackendShadeMode mode)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_SHADEMODE, To_D3D_Shade_Mode(mode));
}

void DX9Backend::Set_Lighting_Enabled(bool enable)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_LIGHTING, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Normalize_Normals(bool enable)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_NORMALIZENORMALS, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Specular_Enabled(bool enable)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_SPECULARENABLE, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Material_Color_Sources(RenderBackendMaterialSource ambient,
	RenderBackendMaterialSource diffuse,
	RenderBackendMaterialSource emissive)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_AMBIENTMATERIALSOURCE,
		To_D3D_Material_Source(ambient));
	DX8Wrapper::Set_DX8_Render_State(D3DRS_DIFFUSEMATERIALSOURCE,
		To_D3D_Material_Source(diffuse));
	DX8Wrapper::Set_DX8_Render_State(D3DRS_EMISSIVEMATERIALSOURCE,
		To_D3D_Material_Source(emissive));
}

void DX9Backend::Set_NPatch_Segments(float segments)
{
	// The DX9 compatibility wrapper deliberately treats its legacy
	// D3DRS_PATCHSEGMENTS token as unsupported. Keep that behavior behind the
	// backend boundary until the DX9 tessellation path is implemented.
	DX8Wrapper::Set_DX8_Render_State(D3DRS_PATCHSEGMENTS, Float_To_Dword(segments));
}

void DX9Backend::Set_Stencil_Enabled(bool enable)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Stencil_Function(RenderBackendCompareFunction function)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILFUNC, To_D3D_Compare_Function(function));
}

void DX9Backend::Set_Stencil_Reference(unsigned reference)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILREF, reference);
}

void DX9Backend::Set_Stencil_Read_Mask(unsigned mask)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILMASK, mask);
}

void DX9Backend::Set_Stencil_Write_Mask(unsigned mask)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILWRITEMASK, mask);
}

void DX9Backend::Set_Stencil_Z_Fail_Operation(RenderBackendStencilOperation operation)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILZFAIL, To_D3D_Stencil_Operation(operation));
}

void DX9Backend::Set_Stencil_Fail_Operation(RenderBackendStencilOperation operation)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILFAIL, To_D3D_Stencil_Operation(operation));
}

void DX9Backend::Set_Stencil_Pass_Operation(RenderBackendStencilOperation operation)
{
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILPASS, To_D3D_Stencil_Operation(operation));
}

void DX9Backend::Set_Texture_Operation(unsigned stage,
	RenderBackendTextureComponent component,
	RenderBackendTextureOperation operation)
{
	const D3DTEXTURESTAGESTATETYPE state = component == RenderBackendTextureComponent::Color
		? D3DTSS_COLOROP
		: D3DTSS_ALPHAOP;
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, state, To_D3D_Texture_Operation(operation));
}

void DX9Backend::Set_Texture_Argument(unsigned stage,
	RenderBackendTextureComponent component,
	unsigned argument_index,
	RenderBackendTextureArgument argument)
{
	D3DTEXTURESTAGESTATETYPE state;
	if (component == RenderBackendTextureComponent::Color)
	{
		WWASSERT(argument_index <= 2);
		// COLORARG0 is a separate D3D9 state for triadic operations; it is
		// not numerically adjacent to COLORARG1 and COLORARG2.
		switch (argument_index)
		{
			case 0: state = D3DTSS_COLORARG0; break;
			case 1: state = D3DTSS_COLORARG1; break;
			case 2: state = D3DTSS_COLORARG2; break;
			default:
				WWASSERT(false);
				state = D3DTSS_COLORARG1;
				break;
		}
	}
	else
	{
		WWASSERT(argument_index == 1 || argument_index == 2);
		state = static_cast<D3DTEXTURESTAGESTATETYPE>(D3DTSS_ALPHAARG1 + argument_index - 1);
	}
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, state, To_D3D_Texture_Argument(argument));
}

void DX9Backend::Set_Texture_Coordinate_Source(unsigned stage,
	RenderBackendTextureCoordinateSource source,
	unsigned uv_array_index)
{
	DWORD value;
	switch (source)
	{
		case RenderBackendTextureCoordinateSource::PassThrough:
			value = D3DTSS_TCI_PASSTHRU | uv_array_index;
			break;
		case RenderBackendTextureCoordinateSource::CameraSpacePosition:
			value = D3DTSS_TCI_CAMERASPACEPOSITION;
			break;
		case RenderBackendTextureCoordinateSource::CameraSpaceNormal:
			value = D3DTSS_TCI_CAMERASPACENORMAL;
			break;
		case RenderBackendTextureCoordinateSource::CameraSpaceReflectionVector:
			value = D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR;
			break;
		default:
			WWASSERT(false);
			value = D3DTSS_TCI_PASSTHRU;
			break;
	}
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXCOORDINDEX, value);
}

void DX9Backend::Set_Texture_Transform_Flags(unsigned stage,
	RenderBackendTextureTransformFlags flags)
{
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXTURETRANSFORMFLAGS,
		To_D3D_Texture_Transform_Flags(flags));
}

void DX9Backend::Set_Texture_Address_Mode(unsigned stage,
	bool u_coordinate,
	RenderBackendTextureAddressMode mode)
{
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage,
		u_coordinate ? D3DTSS_ADDRESSU : D3DTSS_ADDRESSV,
		To_D3D_Texture_Address_Mode(mode));
}

void DX9Backend::Set_Texture_Filter(unsigned stage,
	RenderBackendTextureFilterType type,
	RenderBackendTextureFilter filter)
{
	D3DTEXTURESTAGESTATETYPE state;
	switch (type)
	{
		case RenderBackendTextureFilterType::Minification: state = D3DTSS_MINFILTER; break;
		case RenderBackendTextureFilterType::Magnification: state = D3DTSS_MAGFILTER; break;
		case RenderBackendTextureFilterType::MipMap: state = D3DTSS_MIPFILTER; break;
		default:
			WWASSERT(false);
			state = D3DTSS_MINFILTER;
			break;
	}
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, state, To_D3D_Texture_Filter(filter));
}

void DX9Backend::Set_Texture_Max_Anisotropy(unsigned stage, unsigned level)
{
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MAXANISOTROPY, level);
}

void DX9Backend::Set_Texture_Bump_Environment_Matrix(unsigned stage,
	float m00, float m01, float m10, float m11)
{
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_BUMPENVMAT00, Float_To_Dword(m00));
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_BUMPENVMAT01, Float_To_Dword(m01));
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_BUMPENVMAT10, Float_To_Dword(m10));
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_BUMPENVMAT11, Float_To_Dword(m11));
}

TextureClass * DX9Backend::Create_Render_Target(int width, int height, WW3DFormat format)
{
	return DX8Wrapper::Create_Render_Target(width, height, format);
}

void DX9Backend::Create_Render_Target(int width, int height, WW3DFormat format,
	WW3DZFormat depth_format, TextureClass ** target, ZTextureClass ** depth_target)
{
	DX8Wrapper::Create_Render_Target(width, height, format, depth_format, target, depth_target);
}

void DX9Backend::Set_Render_Target(TextureClass * render_target, ZTextureClass * depth_target)
{
	if (render_target != nullptr)
	{
		DX8Wrapper::Set_Render_Target_With_Z(render_target, depth_target);
	}
	else
	{
		DX8Wrapper::Set_Render_Target(static_cast<IDirect3DSurface9 *>(nullptr));
	}
}

RenderBackendSurface *DX9Backend::Create_System_Memory_Surface(unsigned width,
	unsigned height, WW3DFormat format)
{
	IDirect3DSurface9 *surface = DX8Wrapper::_Create_DX8_Surface(width, height, format);
	return surface != nullptr ? new DX9BackendSurface(surface) : nullptr;
}

bool DX9Backend::Lock_Surface(RenderBackendSurface * surface,
	RenderBackendLockedSurface & locked_surface)
{
	DX9BackendSurface *dx9_surface = To_DX9_Surface(surface);
	if (dx9_surface == nullptr || dx9_surface->Surface == nullptr)
	{
		return false;
	}

	D3DLOCKED_RECT d3d_locked_surface = {};
	const HRESULT result = dx9_surface->Surface->LockRect(
		&d3d_locked_surface, nullptr, D3DLOCK_NO_DIRTY_UPDATE);
	if (FAILED(result))
	{
		return false;
	}

	locked_surface.bits = d3d_locked_surface.pBits;
	locked_surface.pitch = static_cast<unsigned int>(d3d_locked_surface.Pitch);
	return true;
}

void DX9Backend::Unlock_Surface(RenderBackendSurface * surface)
{
	DX9BackendSurface *dx9_surface = To_DX9_Surface(surface);
	if (dx9_surface != nullptr && dx9_surface->Surface != nullptr)
	{
		dx9_surface->Surface->UnlockRect();
	}
}

void DX9Backend::Release_Surface(RenderBackendSurface * surface)
{
	delete To_DX9_Surface(surface);
}

void DX9Backend::Copy_Surface_Rect(RenderBackendSurface * source,
	const RenderBackendRect & source_rect,
	SurfaceClass * destination,
	const RenderBackendPoint & destination_point)
{
	DX9BackendSurface *dx9_source = To_DX9_Surface(source);
	if (dx9_source == nullptr || dx9_source->Surface == nullptr || destination == nullptr)
	{
		return;
	}

	const RECT d3d_source_rect =
	{
		source_rect.left,
		source_rect.top,
		source_rect.right,
		source_rect.bottom
	};
	const POINT d3d_destination_point =
	{
		destination_point.x,
		destination_point.y
	};
	DX8Wrapper::_Copy_DX8_Rects(
		dx9_source->Surface,
		&d3d_source_rect,
		1,
		destination->Peek_D3D_Surface(),
		&d3d_destination_point);
}

bool DX9Backend::Copy_Surface_Rect(SurfaceClass * source,
	const RenderBackendRect & source_rect,
	RenderBackendSurface * destination,
	const RenderBackendPoint & destination_point)
{
	DX9BackendSurface *dx9_destination = To_DX9_Surface(destination);
	IDirect3DDevice9 *device = DX8Wrapper::_Get_D3D_Device8();
	if (source == nullptr || source->Peek_D3D_Surface() == nullptr ||
		dx9_destination == nullptr || dx9_destination->Surface == nullptr ||
		device == nullptr)
	{
		return false;
	}

	IDirect3DSurface9 *source_surface = source->Peek_D3D_Surface();
	D3DSURFACE_DESC source_desc = {};
	D3DSURFACE_DESC destination_desc = {};
	if (FAILED(source_surface->GetDesc(&source_desc)) ||
		FAILED(dx9_destination->Surface->GetDesc(&destination_desc)))
	{
		return false;
	}

	const unsigned source_width = static_cast<unsigned>(source_rect.right - source_rect.left);
	const unsigned source_height = static_cast<unsigned>(source_rect.bottom - source_rect.top);
	const bool full_surface_copy = source_rect.left == 0 && source_rect.top == 0 &&
		source_rect.right == static_cast<int>(source_desc.Width) &&
		source_rect.bottom == static_cast<int>(source_desc.Height) &&
		destination_point.x == 0 && destination_point.y == 0 &&
		source_width == destination_desc.Width && source_height == destination_desc.Height;

	// GetRenderTargetData is the D3D9 path for reading a render target into a
	// system-memory surface. D3DXLoadSurfaceFromSurface cannot reliably perform
	// this DEFAULT -> SYSTEMMEM transfer on all drivers.
	if (full_surface_copy && source_desc.Pool == D3DPOOL_DEFAULT &&
		destination_desc.Pool == D3DPOOL_SYSTEMMEM)
	{
		return SUCCEEDED(device->GetRenderTargetData(source_surface,
			dx9_destination->Surface));
	}

	const RECT d3d_source_rect =
	{
		source_rect.left,
		source_rect.top,
		source_rect.right,
		source_rect.bottom
	};
	const RECT d3d_destination_rect =
	{
		destination_point.x,
		destination_point.y,
		destination_point.x + static_cast<int>(source_width),
		destination_point.y + static_cast<int>(source_height)
	};
	return SUCCEEDED(D3DXLoadSurfaceFromSurface(dx9_destination->Surface,
		nullptr, &d3d_destination_rect, source_surface, nullptr,
		&d3d_source_rect, D3DX_FILTER_NONE, 0));
}

RenderBackendVertexBuffer *DX9Backend::Create_Vertex_Buffer(unsigned size_bytes,
	RenderBackendVertexFormat format, bool dynamic)
{
	IDirect3DDevice9 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device == nullptr || size_bytes == 0)
	{
		return nullptr;
	}

	IDirect3DVertexBuffer9 *buffer = nullptr;
	const DWORD usage = D3DUSAGE_WRITEONLY | (dynamic ? D3DUSAGE_DYNAMIC : 0);
	const D3DPOOL pool = dynamic ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
	if (FAILED(device->CreateVertexBuffer(size_bytes, usage,
		To_D3D_Vertex_Format(format), pool, &buffer, nullptr)))
	{
		return nullptr;
	}

	return new DX9BackendVertexBuffer(buffer, format);
}

RenderBackendIndexBuffer *DX9Backend::Create_Index_Buffer(unsigned size_bytes,
	bool dynamic)
{
	IDirect3DDevice9 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device == nullptr || size_bytes == 0)
	{
		return nullptr;
	}

	IDirect3DIndexBuffer9 *buffer = nullptr;
	const DWORD usage = D3DUSAGE_WRITEONLY | (dynamic ? D3DUSAGE_DYNAMIC : 0);
	const D3DPOOL pool = dynamic ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
	if (FAILED(device->CreateIndexBuffer(size_bytes, usage, D3DFMT_INDEX16,
		pool, &buffer, nullptr)))
	{
		return nullptr;
	}

	return new DX9BackendIndexBuffer(buffer);
}

bool DX9Backend::Lock_Vertex_Buffer(RenderBackendVertexBuffer * buffer,
	unsigned offset_bytes, unsigned size_bytes, void ** data,
	RenderBackendBufferLockMode mode)
{
	DX9BackendVertexBuffer *dx9_buffer = To_DX9_Vertex_Buffer(buffer);
	if (dx9_buffer == nullptr || dx9_buffer->Buffer == nullptr || data == nullptr)
	{
		return false;
	}

	return SUCCEEDED(dx9_buffer->Buffer->Lock(offset_bytes, size_bytes, data,
		To_D3D_Lock_Flags(mode)));
}

bool DX9Backend::Lock_Index_Buffer(RenderBackendIndexBuffer * buffer,
	unsigned offset_bytes, unsigned size_bytes, void ** data,
	RenderBackendBufferLockMode mode)
{
	DX9BackendIndexBuffer *dx9_buffer = To_DX9_Index_Buffer(buffer);
	if (dx9_buffer == nullptr || dx9_buffer->Buffer == nullptr || data == nullptr)
	{
		return false;
	}

	return SUCCEEDED(dx9_buffer->Buffer->Lock(offset_bytes, size_bytes, data,
		To_D3D_Lock_Flags(mode)));
}

void DX9Backend::Unlock_Vertex_Buffer(RenderBackendVertexBuffer * buffer)
{
	DX9BackendVertexBuffer *dx9_buffer = To_DX9_Vertex_Buffer(buffer);
	if (dx9_buffer != nullptr && dx9_buffer->Buffer != nullptr)
	{
		dx9_buffer->Buffer->Unlock();
	}
}

void DX9Backend::Unlock_Index_Buffer(RenderBackendIndexBuffer * buffer)
{
	DX9BackendIndexBuffer *dx9_buffer = To_DX9_Index_Buffer(buffer);
	if (dx9_buffer != nullptr && dx9_buffer->Buffer != nullptr)
	{
		dx9_buffer->Buffer->Unlock();
	}
}

void DX9Backend::Release_Vertex_Buffer(RenderBackendVertexBuffer * buffer)
{
	delete To_DX9_Vertex_Buffer(buffer);
}

void DX9Backend::Release_Index_Buffer(RenderBackendIndexBuffer * buffer)
{
	delete To_DX9_Index_Buffer(buffer);
}

void DX9Backend::Set_Vertex_Buffer(RenderBackendVertexBuffer * buffer,
	unsigned offset_bytes, unsigned stride_bytes, unsigned stream)
{
	IDirect3DDevice9 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device == nullptr)
	{
		return;
	}

	DX9BackendVertexBuffer *dx9_buffer = To_DX9_Vertex_Buffer(buffer);
	device->SetStreamSource(stream,
		dx9_buffer != nullptr ? dx9_buffer->Buffer : nullptr,
		dx9_buffer != nullptr ? offset_bytes : 0,
		 dx9_buffer != nullptr ? stride_bytes : 0);
}

void DX9Backend::Set_Index_Buffer(RenderBackendIndexBuffer * buffer)
{
	IDirect3DDevice9 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device == nullptr)
	{
		return;
	}

	DX9BackendIndexBuffer *dx9_buffer = To_DX9_Index_Buffer(buffer);
	device->SetIndices(dx9_buffer != nullptr ? dx9_buffer->Buffer : nullptr);
}

void DX9Backend::Set_Vertex_Format(RenderBackendVertexFormat format)
{
	IDirect3DDevice9 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device != nullptr)
	{
		device->SetFVF(To_D3D_Vertex_Format(format));
	}
}

void DX9Backend::Draw_Indexed_Primitives(RenderBackendPrimitiveType primitive_type,
	unsigned base_vertex_index, unsigned min_vertex_index,
	unsigned vertex_count, unsigned start_index, unsigned primitive_count)
{
	IDirect3DDevice9 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device != nullptr)
	{
		device->DrawIndexedPrimitive(To_D3D_Primitive_Type(primitive_type),
			base_vertex_index, min_vertex_index, vertex_count, start_index,
			primitive_count);
	}
}

void DX9Backend::Draw_Primitive_Up(RenderBackendPrimitiveType primitive_type,
	unsigned primitive_count, const void * vertices, unsigned stride_bytes,
	RenderBackendVertexFormat format)
{
	IDirect3DDevice9 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device != nullptr && vertices != nullptr)
	{
		device->SetFVF(To_D3D_Vertex_Format(format));
		device->DrawPrimitiveUP(To_D3D_Primitive_Type(primitive_type), primitive_count,
			vertices, stride_bytes);
	}
}

void DX9Backend::Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned stream)
{
	DX8Wrapper::Set_Vertex_Buffer(vb, stream);
}

void DX9Backend::Set_Vertex_Buffer(const DynamicVBAccessClass & vba)
{
	DX8Wrapper::Set_Vertex_Buffer(vba);
}

void DX9Backend::Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset)
{
	DX8Wrapper::Set_Index_Buffer(ib, index_base_offset);
}

void DX9Backend::Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset)
{
	DX8Wrapper::Set_Index_Buffer(iba, index_base_offset);
}

void DX9Backend::Set_Index_Buffer_Index_Offset(unsigned offset)
{
	DX8Wrapper::Set_Index_Buffer_Index_Offset(offset);
}

void DX9Backend::Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix,
	float znear, float zfar)
{
	DX8Wrapper::Set_Projection_Transform_With_Z_Bias(matrix, znear, zfar);
}

void DX9Backend::Set_Transform(RenderBackendTransform transform, const Matrix4x4 & matrix)
{
	DX8Wrapper::Set_Transform(To_D3D_Transform(transform), matrix);
}

void DX9Backend::Set_Transform(RenderBackendTransform transform, const Matrix3D & matrix)
{
	DX8Wrapper::Set_Transform(To_D3D_Transform(transform), matrix);
}

void DX9Backend::Get_Transform(RenderBackendTransform transform, Matrix4x4 & matrix)
{
	DX8Wrapper::Get_Transform(To_D3D_Transform(transform), matrix);
}

void DX9Backend::Set_World_Identity()
{
	DX8Wrapper::Set_World_Identity();
}

void DX9Backend::Set_View_Identity()
{
	DX8Wrapper::Set_View_Identity();
}

bool DX9Backend::Is_World_Identity()
{
	return DX8Wrapper::Is_World_Identity();
}

bool DX9Backend::Is_View_Identity()
{
	return DX8Wrapper::Is_View_Identity();
}

void DX9Backend::Set_Shader(const ShaderClass & shader)
{
	DX8Wrapper::Set_Shader(shader);
}

void DX9Backend::Get_Shader(ShaderClass & shader)
{
	DX8Wrapper::Get_Shader(shader);
}

void DX9Backend::Set_Vertex_Shader(uintptr_t shader)
{
	DX8Wrapper::Set_Vertex_Shader(shader);
}

void DX9Backend::Set_Pixel_Shader(uintptr_t shader)
{
	DX8Wrapper::Set_Pixel_Shader(shader);
}

void DX9Backend::Set_Vertex_Shader_Constant(unsigned reg, const void * data,
	unsigned count)
{
	DX8Wrapper::Set_Vertex_Shader_Constant(static_cast<int>(reg), data,
		static_cast<int>(count));
}

void DX9Backend::Set_Pixel_Shader_Constant(unsigned reg, const void * data,
	unsigned count)
{
	DX8Wrapper::Set_Pixel_Shader_Constant(static_cast<int>(reg), data,
		static_cast<int>(count));
}

void DX9Backend::Set_Texture(unsigned stage, TextureBaseClass * texture)
{
	DX8Wrapper::Set_Texture(stage, texture);
}

void DX9Backend::Set_Texture_Resource(unsigned stage, const TextureBaseClass * texture)
{
	DX8Wrapper::Set_DX8_Texture(stage, texture ? texture->Peek_D3D_Base_Texture() : nullptr);
}

void DX9Backend::Set_Material(const VertexMaterialClass * material)
{
	DX8Wrapper::Set_Material(material);
}

void DX9Backend::Set_Light(unsigned index, const LightClass & light)
{
	DX8Wrapper::Set_Light(index, light);
}

void DX9Backend::Disable_Light(unsigned index)
{
	DX8Wrapper::Set_Light(index, nullptr);
}

void DX9Backend::Apply_Render_State_Changes()
{
	DX8Wrapper::Apply_Render_State_Changes();
}

void DX9Backend::Draw_Triangles(unsigned buffer_type,
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	DX8Wrapper::Draw_Triangles(buffer_type, start_index, polygon_count,
		min_vertex_index, vertex_count);
}

void DX9Backend::Draw_Triangles(unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	DX8Wrapper::Draw_Triangles(start_index, polygon_count, min_vertex_index,
		vertex_count);
}

void DX9Backend::Draw_Strip(unsigned short start_index,
	unsigned short index_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	DX8Wrapper::Draw_Strip(start_index, index_count, min_vertex_index,
		vertex_count);
}
