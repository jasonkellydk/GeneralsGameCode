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
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/dx9wrapper.cpp                         $*
 *                                                                                             *
 *              Original Author:: Jani Penttinen                                               *
 *                                                                                             *
 *                      $Author:: Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 08/05/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 170                                                         $*
 *                                                                                             *
 * 06/26/02 KM Matrix name change to avoid MAX conflicts                                       *
 * 06/27/02 KM Render to shadow buffer texture support														*
 * 06/27/02 KM Shader system updates																				*
 * 08/05/02 KM Texture class redesign
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   DX9Backend::_Update_Texture -- Copies a texture from system memory to video memory        *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define CREATE_D3D9_MULTI_THREADED
//#define CREATE_D3D9_FPU_PRESERVE
#define WW3D_DEVTYPE D3DDEVTYPE_HAL

#if !defined(WINVER) || WINVER < 0x0500
#undef WINVER
#define WINVER 0x0500 // Required to access GetMonitorInfo in VC6.
#endif

#include "Backend/dx9/DX9Backend.h"
#include "VertexFormatMapper.h"
#include "WW3D2/VertexBuffer.h"
#include "WW3D2/IndexBuffer.h"
#include "Renderer.h"
#include "RendererDebugger.h"
#include "WW3D.h"
#include "Camera.h"
#include "WWLib/wwstring.h"
#include "WWMath/matrix4.h"
#include "VertMaterial.h"
#include "RDDesc.h"
#include "LightEnvironment.h"
#include "Statistics.h"
#include "WWLib/registry.h"
#include "BoxRObj.h"
#include "PointGr.h"
#include "Render2D.h"
#include "SortingRenderer.h"
#include "ShatterSystem.h"
#include "Light.h"
#include "AssetMgr.h"
#include "TextureLoader.h"
#include "MissingTexture.h"
#include "WWLib/thread.h"
#include <d3dx9.h>
#include "WWMath/pot.h"
#include "WWDebug/wwprofile.h"
#include "WWLib/ffactory.h"
#include "Caps.h"
#include "FormatConverter.h"
#include "TextureManager.h"
#include "WWLib/bound.h"
#include "WWLib/DbgHelpGuard.h"
#include <intrin.h>
#include <windows.h>
#include <comutil.h>
#include <comip.h>
#include <d3dx9tex.h>
#include "EABrowserEngine/BrowserEngine.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <SDL3/SDL.h>
#include <string>
#include <vector>

#include "ShdLib.h"

namespace
{
	SurfaceClass *Wrap_DX9_Surface(IDirect3DSurface9 *surface);
	IDirect3DSurface9 *Get_DX9_Surface(RenderBackendSurface *surface);
	IDirect3DSurface9 *Get_DX9_Surface(SurfaceClass *surface);

	bool Is_DX9_Out_Of_Memory(unsigned result)
	{
		return result == static_cast<unsigned>(D3DERR_OUTOFVIDEOMEMORY) ||
			result == static_cast<unsigned>(E_OUTOFMEMORY);
	}

	const DWORD *Copy_DX9_Shader_Bytecode_Aligned(const void *bytecode, void **allocation)
	{
		*allocation = nullptr;
		const DWORD *source = static_cast<const DWORD *>(bytecode);
		const UINT size = D3DXGetShaderSize(source);
		if (size == 0)
		{
			return source;
		}

		void *aligned_copy = _aligned_malloc(size, 16);
		if (aligned_copy == nullptr)
		{
			return source;
		}

		std::memcpy(aligned_copy, source, size);
		*allocation = aligned_copy;
		return static_cast<const DWORD *>(aligned_copy);
	}
}

#ifndef D3DENUM_NO_WHQL_LEVEL
#define D3DENUM_NO_WHQL_LEVEL 0
#endif

const int DEFAULT_RESOLUTION_WIDTH = 640;
const int DEFAULT_RESOLUTION_HEIGHT = 480;
const int DEFAULT_BIT_DEPTH = 32;
const int DEFAULT_TEXTURE_BIT_DEPTH = 16;
const D3DMULTISAMPLE_TYPE DEFAULT_MSAA = D3DMULTISAMPLE_NONE;

RenderFrameStatistics DX9Backend::FrameStatistics;

void DX9Backend::Set_Vertex_Shader_Constant(unsigned reg, const void *data, unsigned count)
{
	Set_Vertex_Shader_Constant(static_cast<int>(reg), data, static_cast<int>(count));
}

void DX9Backend::Set_Pixel_Shader_Constant(unsigned reg, const void *data, unsigned count)
{
	Set_Pixel_Shader_Constant(static_cast<int>(reg), data, static_cast<int>(count));
}

void DX9Backend::Set_Pixel_Shader(uintptr_t pixel_shader)
{
	Pixel_Shader = pixel_shader;
	IDirect3DPixelShader9 *const shader = reinterpret_cast<IDirect3DPixelShader9 *>(Pixel_Shader);
	if (!Is_Valid_D3D_Object_Ptr(shader, "Set_Pixel_Shader"))
	{
		DX9CALL(SetPixelShader(nullptr));
		Pixel_Shader = 0;
		return;
	}
	DX9CALL(SetPixelShader(shader));
}

static RenderFrameStatistics LastFrameStatistics;

/***********************************************************************************
**
** DX9Backend Static Variables
**
***********************************************************************************/

static HWND						_Hwnd															= nullptr;
bool								DX9Backend::IsInitted									= false;
bool								DX9Backend::_EnableTriangleDraw						= true;

int								DX9Backend::CurRenderDevice							= -1;
int								DX9Backend::ResolutionWidth							= DEFAULT_RESOLUTION_WIDTH;
int								DX9Backend::ResolutionHeight							= DEFAULT_RESOLUTION_HEIGHT;
int								DX9Backend::BitDepth										= DEFAULT_BIT_DEPTH;
int								DX9Backend::TextureBitDepth							= DEFAULT_TEXTURE_BIT_DEPTH;
bool								DX9Backend::IsWindowed									= false;
RenderBackendFullscreenMode		DX9Backend::FullscreenMode								= RenderBackendFullscreenMode::Exclusive;
DWORD								DX9Backend::PresentationInterval								= D3DPRESENT_INTERVAL_IMMEDIATE;
D3DFORMAT					DX9Backend::DisplayFormat	= D3DFMT_UNKNOWN;
D3DMULTISAMPLE_TYPE DX9Backend::MultiSampleAntiAliasing	= DEFAULT_MSAA;

// shader system additions KJM v
uintptr_t							DX9Backend::Vertex_Shader								= 0;
uintptr_t							DX9Backend::Pixel_Shader								= 0;

Vector4							DX9Backend::Vertex_Shader_Constants[MAX_VERTEX_SHADER_CONSTANTS];
Vector4							DX9Backend::Pixel_Shader_Constants[MAX_PIXEL_SHADER_CONSTANTS];

LightEnvironmentClass*		DX9Backend::Light_Environment							= nullptr;

DWORD								DX9Backend::Vertex_Processing_Behavior				= 0;
ZTextureClass*					DX9Backend::Shadow_Map[MAX_SHADOW_MAPS];

Vector3							DX9Backend::Ambient_Color;
// shader system additions KJM ^

bool								DX9Backend::world_identity;
unsigned							DX9Backend::RenderStates[256];
unsigned							DX9Backend::TextureStageStates[MAX_TEXTURE_STAGES][32];
IDirect3DBaseTexture9 *		DX9Backend::Textures[MAX_TEXTURE_STAGES];
RenderStateStruct				DX9Backend::render_state;
unsigned							DX9Backend::render_state_changed;

bool								DX9Backend::FogEnable									= false;
D3DCOLOR							DX9Backend::FogColor										= 0;

IDirect3D9 *					DX9Backend::D3DInterface								= nullptr;
IDirect3DDevice9 *			DX9Backend::D3DDevice									= nullptr;
IDirect3DVertexDeclaration9 *	DX9Backend::Vertex_Declaration				= nullptr;
IDirect3DSurface9 *			DX9Backend::CurrentRenderTarget						= nullptr;
IDirect3DSurface9 *			DX9Backend::CurrentDepthBuffer						= nullptr;
IDirect3DSurface9 *			DX9Backend::DefaultRenderTarget						= nullptr;
IDirect3DSurface9 *			DX9Backend::DefaultDepthBuffer						= nullptr;
bool								DX9Backend::IsRenderToTexture							= false;

unsigned							DX9Backend::_MainThreadID								= 0;
bool								DX9Backend::CurrentDX9LightEnables[4];
bool								DX9Backend::IsDeviceLost;
int								DX9Backend::ZBias;
float								DX9Backend::ZNear;
float								DX9Backend::ZFar;
D3DMATRIX						DX9Backend::ProjectionMatrix;
D3DMATRIX						DX9Backend::DX9Transforms[D3DTS_WORLD+1];

DX9Caps*							DX9Backend::CurrentCaps = nullptr;

// Hack test... this disables rendering of batches of too few polygons.
unsigned							DX9Backend::DrawPolygonLowBoundLimit=0;

D3DADAPTER_IDENTIFIER9		DX9Backend::CurrentAdapterIdentifier;
HRESULT						DX9Backend::LastCreateDeviceHRESULT = S_OK;
char							DX9Backend::LastSetRenderDeviceError[128] = "none";

bool DX9Backend::Is_Valid_D3D_Object_Ptr(const void* ptr, const char* context)
{
	if (ptr == nullptr) {
		return true;
	}

	const uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
	if (addr < 0x10000U) {
		WWDEBUG_SAY(("DX9Backend invalid D3D object pointer in %s: %p", context ? context : "<unknown>", ptr));
		return false;
	}

	MEMORY_BASIC_INFORMATION mbi = {};
	if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0) {
		WWDEBUG_SAY(("DX9Backend VirtualQuery failed in %s for %p", context ? context : "<unknown>", ptr));
		return false;
	}

	if (mbi.State != MEM_COMMIT) {
		WWDEBUG_SAY(("DX9Backend non-committed D3D object pointer in %s: %p state=0x%lx", context ? context : "<unknown>", ptr, static_cast<unsigned long>(mbi.State)));
		return false;
	}

	const DWORD prot = (mbi.Protect & 0xFFU);
	if (prot == PAGE_NOACCESS || prot == PAGE_GUARD) {
		WWDEBUG_SAY(("DX9Backend inaccessible D3D object pointer in %s: %p prot=0x%lx", context ? context : "<unknown>", ptr, static_cast<unsigned long>(mbi.Protect)));
		return false;
	}

	return true;
}

unsigned long DX9Backend::FrameCount = 0;

bool								_DX9SingleThreaded										= false;

static D3DPRESENT_PARAMETERS								_PresentParameters;
static DynamicVectorClass<StringClass>					_RenderDeviceNameTable;
static DynamicVectorClass<StringClass>					_RenderDeviceShortNameTable;
static DynamicVectorClass<RenderDeviceDescClass>	_RenderDeviceDescriptionTable;


static RenderBackendLight To_Render_Backend_Light(const D3DLIGHT9 & source)
{
	RenderBackendLight target;
	switch (source.Type)
	{
	case D3DLIGHT_POINT: target.type = RenderBackendLightType::Point; break;
	case D3DLIGHT_SPOT: target.type = RenderBackendLightType::Spot; break;
	case D3DLIGHT_DIRECTIONAL: target.type = RenderBackendLightType::Directional; break;
	default: target.type = RenderBackendLightType::Unknown; break;
	}

	target.diffuse[0] = source.Diffuse.r;
	target.diffuse[1] = source.Diffuse.g;
	target.diffuse[2] = source.Diffuse.b;
	target.diffuse[3] = source.Diffuse.a;
	target.specular[0] = source.Specular.r;
	target.specular[1] = source.Specular.g;
	target.specular[2] = source.Specular.b;
	target.specular[3] = source.Specular.a;
	target.ambient[0] = source.Ambient.r;
	target.ambient[1] = source.Ambient.g;
	target.ambient[2] = source.Ambient.b;
	target.ambient[3] = source.Ambient.a;
	target.position[0] = source.Position.x;
	target.position[1] = source.Position.y;
	target.position[2] = source.Position.z;
	target.direction[0] = source.Direction.x;
	target.direction[1] = source.Direction.y;
	target.direction[2] = source.Direction.z;
	target.range = source.Range;
	target.falloff = source.Falloff;
	target.attenuation0 = source.Attenuation0;
	target.attenuation1 = source.Attenuation1;
	target.attenuation2 = source.Attenuation2;
	target.theta = source.Theta;
	target.phi = source.Phi;
	return target;
}

static void To_DX9_Light(const RenderBackendLight & source, D3DLIGHT9 & target)
{
	::ZeroMemory(&target, sizeof(target));
	switch (source.type)
	{
	case RenderBackendLightType::Point: target.Type = D3DLIGHT_POINT; break;
	case RenderBackendLightType::Spot: target.Type = D3DLIGHT_SPOT; break;
	case RenderBackendLightType::Directional: target.Type = D3DLIGHT_DIRECTIONAL; break;
	default: target.Type = D3DLIGHT_POINT; break;
	}

	target.Diffuse.r = source.diffuse[0];
	target.Diffuse.g = source.diffuse[1];
	target.Diffuse.b = source.diffuse[2];
	target.Diffuse.a = source.diffuse[3];
	target.Specular.r = source.specular[0];
	target.Specular.g = source.specular[1];
	target.Specular.b = source.specular[2];
	target.Specular.a = source.specular[3];
	target.Ambient.r = source.ambient[0];
	target.Ambient.g = source.ambient[1];
	target.Ambient.b = source.ambient[2];
	target.Ambient.a = source.ambient[3];
	target.Position.x = source.position[0];
	target.Position.y = source.position[1];
	target.Position.z = source.position[2];
	target.Direction.x = source.direction[0];
	target.Direction.y = source.direction[1];
	target.Direction.z = source.direction[2];
	target.Range = source.range;
	target.Falloff = source.falloff;
	target.Attenuation0 = source.attenuation0;
	target.Attenuation1 = source.attenuation1;
	target.Attenuation2 = source.attenuation2;
	target.Theta = source.theta;
	target.Phi = source.phi;
}

typedef IDirect3D9* (WINAPI *Direct3DCreate9Type) (UINT SDKVersion);
Direct3DCreate9Type	Direct3DCreate9Ptr = nullptr;
HINSTANCE D3D9Lib = nullptr;
static char sDeviceInitStage[96] = "none";

/***********************************************************************************
**
** DX9Backend Implementation
**
***********************************************************************************/

void Log_DX9_ErrorCode(unsigned res, const char* file, int line)
{
	char tmp[256] = "";
	FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		res,
		0,
		tmp,
		sizeof(tmp),
		nullptr);
	WWDEBUG_SAY(("DX9 Error: 0x%08X (%s) at %s:%d", res, tmp, file, line));
}

void Non_Fatal_Log_DX9_ErrorCode(unsigned res,const char * file,int line)
{
	char tmp[256] = "";
	FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		res,
		0,
		tmp,
		sizeof(tmp),
		nullptr);
	WWDEBUG_SAY(("DX9 Error: %s, File: %s, Line: %d",tmp,file,line));
}

void DX9Backend::_Copy_DX9_Rects(
	IDirect3DSurface9* pSourceSurface,
	CONST RECT* pSourceRectsArray,
	UINT cRects,
	IDirect3DSurface9* pDestinationSurface,
	CONST POINT* pDestPointsArray)
{
	if (pSourceSurface == nullptr || pDestinationSurface == nullptr)
	{
		return;
	}

	__try
	{
		D3DSURFACE_DESC src_desc = {};
		D3DSURFACE_DESC dst_desc = {};
		const HRESULT src_desc_hr = pSourceSurface->GetDesc(&src_desc);
		const HRESULT dst_desc_hr = pDestinationSurface->GetDesc(&dst_desc);
		DX9_ErrorCode(src_desc_hr);
		DX9_ErrorCode(dst_desc_hr);

		// D3D9 requires explicit render-target readback for DEFAULT -> SYSTEMMEM.
		if (SUCCEEDED(src_desc_hr) && SUCCEEDED(dst_desc_hr) &&
			pSourceRectsArray == nullptr && pDestPointsArray == nullptr && cRects == 0)
		{
			if (src_desc.Pool == D3DPOOL_DEFAULT && dst_desc.Pool == D3DPOOL_SYSTEMMEM && D3DDevice != nullptr)
			{
				static bool warnedOnce = false;
				if (!warnedOnce)
				{
					warnedOnce = true;
					WWDEBUG_SAY(("DX9Backend::_Copy_DX9_Rects skipping DEFAULT->SYSTEMMEM backbuffer copy to avoid D3D9 crash"));
				}
				return;
			}

			// Preserve the original fast path for SYSTEMMEM -> DEFAULT full-surface copies.
			if (src_desc.Pool == D3DPOOL_SYSTEMMEM && dst_desc.Pool == D3DPOOL_DEFAULT && D3DDevice != nullptr)
			{
				const HRESULT result = D3DDevice->UpdateSurface(pSourceSurface, nullptr, pDestinationSurface, nullptr);
				DX9_ErrorCode(result);
				return;
			}
		}

		auto copy_one = [&](const RECT* src_rect, const POINT* dst_point)
		{
			RECT dst_rect;
			RECT* dst_rect_ptr = nullptr;
			if (src_rect != nullptr && dst_point != nullptr)
			{
				dst_rect.left = dst_point->x;
				dst_rect.top = dst_point->y;
				dst_rect.right = dst_point->x + (src_rect->right - src_rect->left);
				dst_rect.bottom = dst_point->y + (src_rect->bottom - src_rect->top);
				dst_rect_ptr = &dst_rect;
			}

			const HRESULT result = D3DXLoadSurfaceFromSurface(
				pDestinationSurface, nullptr, dst_rect_ptr,
				pSourceSurface, nullptr, src_rect,
				D3DX_FILTER_NONE, 0);
			DX9_ErrorCode(result);
			if (FAILED(result))
			{
				WWDEBUG_SAY(("DX9Backend::_Copy_DX9_Rects failed hr=0x%08X src=%p dst=%p srcRect=%p dstRect=%p",
					result, pSourceSurface, pDestinationSurface, src_rect, dst_rect_ptr));
			}
		};

		if (pSourceRectsArray && pDestPointsArray && cRects > 0)
		{
			for (UINT i = 0; i < cRects; ++i)
			{
				copy_one(&pSourceRectsArray[i], &pDestPointsArray[i]);
			}
			return;
		}

		copy_one(nullptr, nullptr);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		WWDEBUG_SAY(("DX9Backend::_Copy_DX9_Rects caught exception 0x%08X for src=%p dst=%p",
			static_cast<unsigned>(GetExceptionCode()), pSourceSurface, pDestinationSurface));
	}
}

// TheSuperHackers @info helmutbuhler 14/04/2025
// Helper function that moves x and y such that the inner rect fits into the outer rect.
// If the inner rect already is in the outer rect, then this does nothing.
// If the inner rect is larger than the outer rect, then the inner rect will be aligned to the top left of the outer rect.
void MoveRectIntoOtherRect(const RECT& inner, const RECT& outer, int* x, int* y)
{
	int dx = 0;
	if (inner.right > outer.right)
		dx = outer.right-inner.right;
	if (inner.left < outer.left)
		dx = outer.left-inner.left;

	int dy = 0;
	if (inner.bottom > outer.bottom)
		dy = outer.bottom-inner.bottom;
	if (inner.top < outer.top)
		dy = outer.top-inner.top;

	*x += dx;
	*y += dy;
}


bool DX9Backend::Initialize(void * hwnd, bool lite)
{
	WWASSERT(!IsInitted);

	// zero memory
	memset(Textures,0,sizeof(IDirect3DBaseTexture9*)*MAX_TEXTURE_STAGES);
	memset(RenderStates,0,sizeof(unsigned)*256);
	memset(TextureStageStates,0,sizeof(unsigned)*32*MAX_TEXTURE_STAGES);
	memset(Vertex_Shader_Constants,0,sizeof(Vector4)*MAX_VERTEX_SHADER_CONSTANTS);
	memset(Pixel_Shader_Constants,0,sizeof(Vector4)*MAX_PIXEL_SHADER_CONSTANTS);
	render_state=RenderStateStruct();
	memset(Shadow_Map,0,sizeof(ZTextureClass*)*MAX_SHADOW_MAPS);

	/*
	** Initialize all variables!
	*/
	_Hwnd = (HWND)hwnd;
	_MainThreadID=ThreadClass::_Get_Current_Thread_ID();
	WWDEBUG_SAY(("DX9Backend main thread: 0x%x",_MainThreadID));
	CurRenderDevice = -1;
	ResolutionWidth = DEFAULT_RESOLUTION_WIDTH;
	ResolutionHeight = DEFAULT_RESOLUTION_HEIGHT;
	// Initialize Render2DClass Screen Resolution
	Render2DClass::Set_Screen_Resolution( RectClass( 0, 0, ResolutionWidth, ResolutionHeight ) );
	BitDepth = DEFAULT_BIT_DEPTH;
	IsWindowed = false;
	FullscreenMode = RenderBackendFullscreenMode::Exclusive;
	PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	for (int light=0;light<4;++light) CurrentDX9LightEnables[light]=false;

	//old_vertex_shader; TODO
	//old_sr_shader;
	//current_shader;

	//world_identity;
	//CurrentFogColor;

	D3DInterface = nullptr;
	D3DDevice = nullptr;

	WWDEBUG_SAY(("Reset DX9Backend statistics"));
	Reset_Statistics();

	Invalidate_Cached_Render_States();

	if (!lite) {
		D3D9Lib = LoadLibrary("D3D9.DLL");

		if (D3D9Lib == nullptr) return false;	// Return false at this point if init failed

		Direct3DCreate9Ptr = (Direct3DCreate9Type) GetProcAddress(D3D9Lib, "Direct3DCreate9");
		if (Direct3DCreate9Ptr == nullptr) return false;

		/*
		** Create the D3D interface object
		*/
		WWDEBUG_SAY(("Create Direct3D9"));
		{
			// TheSuperHackers @bugfix xezon 13/06/2025 Front load the system dbghelp.dll to prevent
			// the graphics driver from potentially loading the old game dbghelp.dll and then crashing the game process.
			DbgHelpGuard dbgHelpGuard;

			D3DInterface = Direct3DCreate9Ptr(D3D_SDK_VERSION);		// TODO: handle failure cases...
		}
		if (D3DInterface == nullptr) {
			return(false);
		}
		IsInitted = true;

		/*
		** Enumerate the available devices
		*/
		WWDEBUG_SAY(("Enumerate devices"));
		Enumerate_Devices();
		WWDEBUG_SAY(("DX9Backend Init completed"));
	}

	return(true);
}

void DX9Backend::Shutdown()
{
	if (D3DDevice) {
		Release_Render_Target_Cache();
		Release_Device();
	}

	if (D3DInterface) {
		D3DInterface->Release();
		D3DInterface=nullptr;

	}

	if (CurrentCaps)
	{
		int max=CurrentCaps->Get_Max_Textures_Per_Pass();
		for (int i = 0; i < max; i++)
		{
			if (Textures[i])
			{
				Textures[i]->Release();
				Textures[i] = nullptr;
			}
		}
	}

	if (D3D9Lib) {
		FreeLibrary(D3D9Lib);
		D3D9Lib = nullptr;
	}

	_RenderDeviceNameTable.Clear();		 // note - Delete_All() resizes the vector, causing a reallocation.  Clear is better. jba.
	_RenderDeviceShortNameTable.Clear();
	_RenderDeviceDescriptionTable.Clear();

	DX9Caps::Shutdown();
	IsInitted = false;		// 010803 srj
}

void DX9Backend::Do_Onetime_Device_Dependent_Inits()
{
	/*
	** Set Global render states (some of which depend on caps)
	*/
	strlcpy(sDeviceInitStage, "Compute_Caps", ARRAY_SIZE(sDeviceInitStage));
	Compute_Caps(WW3D_Format_From_DX9(DisplayFormat));

   /*
	** Initialize any other subsystems inside of WW3D
	*/
	strlcpy(sDeviceInitStage, "MissingTexture::_Init", ARRAY_SIZE(sDeviceInitStage));
	MissingTexture::_Init();
	strlcpy(sDeviceInitStage, "TextureFilterClass::_Init_Filters", ARRAY_SIZE(sDeviceInitStage));
	TextureFilterClass::_Init_Filters(
		(TextureFilterClass::TextureFilterMode)WW3D::Get_Texture_Filter(),
		(TextureFilterClass::AnisotropicFilterMode)WW3D::Get_Anisotropy_Level()
	);
	strlcpy(sDeviceInitStage, "TheDX9MeshRenderer.Init", ARRAY_SIZE(sDeviceInitStage));
	TheDX9MeshRenderer.Init();
	strlcpy(sDeviceInitStage, "SHD_INIT", ARRAY_SIZE(sDeviceInitStage));
	SHD_INIT;
	strlcpy(sDeviceInitStage, "BoxRenderObjClass::Init", ARRAY_SIZE(sDeviceInitStage));
	try
	{
		BoxRenderObjClass::Init();
	}
	catch (...)
	{
		strlcpy(LastSetRenderDeviceError, "BoxRenderObjClass::Init failed - continuing", ARRAY_SIZE(LastSetRenderDeviceError));
	}
	strlcpy(sDeviceInitStage, "VertexMaterialClass::Init", ARRAY_SIZE(sDeviceInitStage));
	VertexMaterialClass::Init();
	strlcpy(sDeviceInitStage, "PointGroupClass::_Init", ARRAY_SIZE(sDeviceInitStage));
	PointGroupClass::_Init(); // This needs the VertexMaterialClass to be initted
	strlcpy(sDeviceInitStage, "ShatterSystem::Init", ARRAY_SIZE(sDeviceInitStage));
	ShatterSystem::Init();
	strlcpy(sDeviceInitStage, "TextureLoader::Init", ARRAY_SIZE(sDeviceInitStage));
	TextureLoader::Init();

	strlcpy(sDeviceInitStage, "Set_Default_Global_Render_States", ARRAY_SIZE(sDeviceInitStage));
	Set_Default_Global_Render_States();
	strlcpy(sDeviceInitStage, "done", ARRAY_SIZE(sDeviceInitStage));
}

inline DWORD F2DW(float f) { return *((unsigned*)&f); }
void DX9Backend::Set_Default_Global_Render_States()
{
	DX9_THREAD_ASSERT();
	const D3DCAPS9 &caps = Get_Current_Caps()->Get_DX9_Caps();

	Set_DX9_Render_State(D3DRS_RANGEFOGENABLE, (caps.RasterCaps & D3DPRASTERCAPS_FOGRANGE) ? TRUE : FALSE);
	Set_DX9_Render_State(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
	Set_DX9_Render_State(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
	Set_DX9_Render_State(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL);
	Set_DX9_Render_State(D3DRS_COLORVERTEX, TRUE);
	Set_DX9_Render_State(D3DRS_ZBIAS,0);
	Set_DX9_Texture_Stage_State(1, D3DTSS_BUMPENVLSCALE, F2DW(1.0f));
	Set_DX9_Texture_Stage_State(1, D3DTSS_BUMPENVLOFFSET, F2DW(0.0f));
	Set_DX9_Texture_Stage_State(0, D3DTSS_BUMPENVMAT00,F2DW(1.0f));
	Set_DX9_Texture_Stage_State(0, D3DTSS_BUMPENVMAT01,F2DW(0.0f));
	Set_DX9_Texture_Stage_State(0, D3DTSS_BUMPENVMAT10,F2DW(0.0f));
	Set_DX9_Texture_Stage_State(0, D3DTSS_BUMPENVMAT11,F2DW(1.0f));

//	Set_DX9_Render_State(D3DRS_CULLMODE, D3DCULL_CW);
	// Set dither mode here?
}

void DX9Backend::Invalidate_Cached_Render_States()
{
	render_state_changed=0;

	int a;
	for (a=0;a<sizeof(RenderStates)/sizeof(unsigned);++a) {
		RenderStates[a]=0x12345678;
	}
	for (a=0;a<MAX_TEXTURE_STAGES;++a)
	{
		for (int b=0; b<32;b++)
		{
			TextureStageStates[a][b]=0x12345678;
		}
		//Need to explicitly set texture to null, otherwise app will not be able to
		//set it to null because of redundant state checker. MW
		if (_Get_D3D_Device())
			_Get_D3D_Device()->SetTexture(a,nullptr);
		if (Textures[a] != nullptr) {
			Textures[a]->Release();
		}
		Textures[a]=nullptr;
	}

	ShaderClass::Invalidate();

	//Need to explicitly set render_state texture pointers to null. MW
	Release_Render_State();

	// (gth) clear the matrix shadows too
	memset(&DX9Transforms, 0, sizeof(DX9Transforms));
}

void DX9Backend::Do_Onetime_Device_Dependent_Shutdowns()
{
	Release_Vertex_Shader_Input_Layout();

	/*
	** Shutdown ww3d systems
	*/
	int i;
	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		if (render_state.vertex_buffers[i]) render_state.vertex_buffers[i]->Release_Engine_Ref();
		REF_PTR_RELEASE(render_state.vertex_buffers[i]);
	}
	if (render_state.index_buffer) render_state.index_buffer->Release_Engine_Ref();
	REF_PTR_RELEASE(render_state.index_buffer);
	REF_PTR_RELEASE(render_state.material);
	for (i=0;i<CurrentCaps->Get_Max_Textures_Per_Pass();++i) REF_PTR_RELEASE(render_state.Textures[i]);


	TextureLoader::Deinit();
	SortingRendererClass::Deinit();
	DynamicVBAccessClass::_Deinit();
	DynamicIBAccessClass::_Deinit();
	ShatterSystem::Shutdown();
	PointGroupClass::_Shutdown();
	VertexMaterialClass::Shutdown();
	BoxRenderObjClass::Shutdown();
	SHD_SHUTDOWN;
	TheDX9MeshRenderer.Shutdown();
	MissingTexture::_Deinit();

	delete CurrentCaps;
	CurrentCaps=nullptr;

}


bool DX9Backend::Create_Device()
{
	WWASSERT(D3DDevice==nullptr);	// for now, once you've created a device, you're stuck with it!
	LastCreateDeviceHRESULT = S_OK;

	D3DCAPS9 caps;
	if
	(
		FAILED
		(
			D3DInterface->GetDeviceCaps
			(
				CurRenderDevice,
				WW3D_DEVTYPE,
				&caps
			)
		)
	)
	{
		LastCreateDeviceHRESULT = E_FAIL;
		return false;
	}

	::ZeroMemory(&CurrentAdapterIdentifier, sizeof(D3DADAPTER_IDENTIFIER9));

	if
	(
		FAILED
		(
			D3DInterface->GetAdapterIdentifier
			(
				CurRenderDevice,
				D3DENUM_NO_WHQL_LEVEL,
				&CurrentAdapterIdentifier
			)
			)
	)
	{
		LastCreateDeviceHRESULT = E_FAIL;
		return false;
	}

	Vertex_Processing_Behavior=(caps.DevCaps&D3DDEVCAPS_HWTRANSFORMANDLIGHT) ?
		D3DCREATE_MIXED_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING;

	// enable this when all 'get' dx calls are removed KJM
	/*if (caps.DevCaps&D3DDEVCAPS_PUREDEVICE)
	{
		Vertex_Processing_Behavior|=D3DCREATE_PUREDEVICE;
	}*/

#ifdef CREATE_DX9_MULTI_THREADED
	Vertex_Processing_Behavior|=D3DCREATE_MULTITHREADED;
	_DX9SingleThreaded=false;
#else
	_DX9SingleThreaded=true;
#endif

	if (WW3D::Get_Preserve_FPU())
		Vertex_Processing_Behavior |= D3DCREATE_FPU_PRESERVE;

#ifdef CREATE_DX9_FPU_PRESERVE
	Vertex_Processing_Behavior|=D3DCREATE_FPU_PRESERVE;
#endif

	// TheSuperHackers @bugfix xezon 13/06/2025 Front load the system dbghelp.dll to prevent
	// the graphics driver from potentially loading the old game dbghelp.dll and then crashing the game process.
	DbgHelpGuard dbgHelpGuard;

	HRESULT hr=D3DInterface->CreateDevice
	(
		CurRenderDevice,
		WW3D_DEVTYPE,
		_Hwnd,
		Vertex_Processing_Behavior,
		&_PresentParameters,
		&D3DDevice
	);

	if (FAILED(hr))
	{
		LastCreateDeviceHRESULT = hr;
		// The device selection may fail because the device lied that it supports 32 bit zbuffer with 16 bit
		// display. This happens at least on Voodoo2.

		if ((_PresentParameters.BackBufferFormat==D3DFMT_R5G6B5 ||
			_PresentParameters.BackBufferFormat==D3DFMT_X1R5G5B5 ||
			_PresentParameters.BackBufferFormat==D3DFMT_A1R5G5B5) &&
			(_PresentParameters.AutoDepthStencilFormat==D3DFMT_D32 ||
			_PresentParameters.AutoDepthStencilFormat==D3DFMT_D24S8 ||
			_PresentParameters.AutoDepthStencilFormat==D3DFMT_D24X8))
		{
			_PresentParameters.AutoDepthStencilFormat=D3DFMT_D16;
			hr = D3DInterface->CreateDevice
			(
				CurRenderDevice,
				WW3D_DEVTYPE,
				_Hwnd,
				Vertex_Processing_Behavior,
				&_PresentParameters,
				&D3DDevice
			);

			if (FAILED(hr))
			{
				LastCreateDeviceHRESULT = hr;
				return false;
			}
        }
		else
		{
				LastCreateDeviceHRESULT = hr;
				return false;
		}
	}
	LastCreateDeviceHRESULT = hr;

	dbgHelpGuard.deactivate();

	/*
	** Initialize all subsystems
	*/
	try
	{
		Do_Onetime_Device_Dependent_Inits();
	}
	catch (...)
	{
		snprintf(
			LastSetRenderDeviceError,
			ARRAY_SIZE(LastSetRenderDeviceError),
			"Do_Onetime_Device_Dependent_Inits exception at %s",
			sDeviceInitStage);
		return false;
	}
	return true;
}

void DX9Backend::Release_Render_Target_Cache()
{
	if (D3DDevice != nullptr)
	{
		// Reset cannot proceed while a render-to-texture surface is still bound.
		// Restore the swap-chain back buffer first, then release every cached
		// reference before the device is reset.
		IDirect3DSurface9 *default_render_target = DefaultRenderTarget;
		bool release_default_render_target = false;
		if (default_render_target == nullptr && CurrentRenderTarget != nullptr)
		{
			HRESULT result = D3DDevice->GetBackBuffer(
				0, 0, D3DBACKBUFFER_TYPE_MONO, &default_render_target);
			DX9_ErrorCode(result);
			release_default_render_target = SUCCEEDED(result) && default_render_target != nullptr;
		}

		if (default_render_target != nullptr)
		{
			DX9CALL(SetRenderTarget(0, default_render_target));
		}

		if (DefaultDepthBuffer != nullptr)
		{
			DX9CALL(SetDepthStencilSurface(DefaultDepthBuffer));
		}
		else if (CurrentDepthBuffer != nullptr)
		{
			DX9CALL(SetDepthStencilSurface(nullptr));
		}

		if (release_default_render_target)
		{
			default_render_target->Release();
		}
	}

	if (CurrentRenderTarget != nullptr)
	{
		CurrentRenderTarget->Release();
		CurrentRenderTarget = nullptr;
	}
	if (CurrentDepthBuffer != nullptr)
	{
		CurrentDepthBuffer->Release();
		CurrentDepthBuffer = nullptr;
	}
	if (DefaultRenderTarget != nullptr)
	{
		DefaultRenderTarget->Release();
		DefaultRenderTarget = nullptr;
	}
	if (DefaultDepthBuffer != nullptr)
	{
		DefaultDepthBuffer->Release();
		DefaultDepthBuffer = nullptr;
	}

	IsRenderToTexture = false;
}

bool DX9Backend::Reset_Device(bool reload_assets)
{
	WWDEBUG_SAY(("Resetting device."));
	DX9_THREAD_ASSERT();
	if ((IsInitted) && (D3DDevice != nullptr)) {
		// Release all non-MANAGED stuff
		WW3D::_Invalidate_Textures();

		for (unsigned i=0;i<MAX_VERTEX_STREAMS;++i)
		{
			Set_Vertex_Buffer (nullptr,i);
		}
		Set_Index_Buffer (nullptr, 0);
		if (CleanupHook != nullptr)
		{
			CleanupHook->ReleaseResources();
		}
		Release_Render_Target_Cache();
		DynamicVBAccessClass::_Deinit();
		DynamicIBAccessClass::_Deinit();
		DX9TextureManagerClass::Release_Textures();
		SHD_SHUTDOWN_SHADERS;

		// Reset frame count to reflect the flipping chain being reset by Reset()
		FrameCount = 0;

		memset(Vertex_Shader_Constants,0,sizeof(Vector4)*MAX_VERTEX_SHADER_CONSTANTS);
		memset(Pixel_Shader_Constants,0,sizeof(Vector4)*MAX_PIXEL_SHADER_CONSTANTS);
		Release_Vertex_Shader_Input_Layout();

		HRESULT hr=_Get_D3D_Device()->TestCooperativeLevel();
		if (hr != D3DERR_DEVICELOST )
		{	DX9CALL_HRES(Reset(&_PresentParameters),hr)
			if (hr != D3D_OK)
				return false;	//reset failed.
		}
		else
			return false;	//device is lost and can't be reset.

		if (reload_assets)
		{
			DX9TextureManagerClass::Recreate_Textures();
			if (CleanupHook != nullptr)
			{
				CleanupHook->ReAcquireResources();
			}
		}
		Invalidate_Cached_Render_States();
		Set_Default_Global_Render_States();
		SHD_INIT_SHADERS;
		WWDEBUG_SAY(("Device reset completed"));
		return true;
	}
	WWDEBUG_SAY(("Device reset failed"));
	return false;
}

void DX9Backend::Release_Device()
{
	if (D3DDevice) {
		Release_Render_Target_Cache();

		for (int a=0;a<MAX_TEXTURE_STAGES;++a)
		{	//release references to any textures that were used in last rendering call
			DX9CALL(SetTexture(a,nullptr));
		}

		DX9CALL(SetStreamSource(0, nullptr, 0, 0));	//release reference count on last rendered vertex buffer
		DX9CALL(SetIndices(nullptr));	//release reference count on last rendered index buffer


		/*
		** Release the current vertex and index buffers
		*/
		for (unsigned i=0;i<MAX_VERTEX_STREAMS;++i)
		{
			if (render_state.vertex_buffers[i]) render_state.vertex_buffers[i]->Release_Engine_Ref();
			REF_PTR_RELEASE(render_state.vertex_buffers[i]);
		}
		if (render_state.index_buffer) render_state.index_buffer->Release_Engine_Ref();
		REF_PTR_RELEASE(render_state.index_buffer);

		/*
		** Shutdown all subsystems
		*/
		Do_Onetime_Device_Dependent_Shutdowns();

		/*
		** Release the device
		*/

		D3DDevice->Release();
		D3DDevice=nullptr;
	}
}

void DX9Backend::Enumerate_Devices()
{
	DX9_Assert();

	int adapter_count = D3DInterface->GetAdapterCount();
	for (int adapter_index=0; adapter_index<adapter_count; adapter_index++) {

		D3DADAPTER_IDENTIFIER9 id;
		::ZeroMemory(&id, sizeof(D3DADAPTER_IDENTIFIER9));
		HRESULT res = D3DInterface->GetAdapterIdentifier(adapter_index,D3DENUM_NO_WHQL_LEVEL,&id);

		if (res == D3D_OK) {

			/*
			** Set up the render device description
			** TODO: Fill in more fields of the render device description?  (need some lookup tables)
			*/
			RenderDeviceDescClass desc;
			desc.Set_Device_Name(id.Description);
			desc.Set_Driver_Name(id.Driver);

			char buf[64];
			sprintf(buf,"%d.%d.%d.%d", //"%04x.%04x.%04x.%04x",
				HIWORD(id.DriverVersion.HighPart),
				LOWORD(id.DriverVersion.HighPart),
				HIWORD(id.DriverVersion.LowPart),
				LOWORD(id.DriverVersion.LowPart));

			desc.Set_Driver_Version(buf);

			D3DCAPS9 adapter_caps;
			::ZeroMemory(&adapter_caps, sizeof(adapter_caps));
			D3DInterface->GetDeviceCaps(adapter_index,WW3D_DEVTYPE,&adapter_caps);
			D3DADAPTER_IDENTIFIER9 adapter_identifier;
			::ZeroMemory(&adapter_identifier, sizeof(adapter_identifier));
			D3DInterface->GetAdapterIdentifier(adapter_index,D3DENUM_NO_WHQL_LEVEL,&adapter_identifier);

			DX9Caps dx9caps(D3DInterface,adapter_caps,WW3D_FORMAT_UNKNOWN,adapter_identifier);

			/*
			** Enumerate the resolutions
			*/
			desc.Reset_Resolution_List();
			const D3DFORMAT enum_format = D3DFMT_X8R8G8B8;
			int mode_count = D3DInterface->GetAdapterModeCount(adapter_index, enum_format);
			for (int mode_index=0; mode_index<mode_count; mode_index++) {
				D3DDISPLAYMODE d3dmode;
				::ZeroMemory(&d3dmode, sizeof(D3DDISPLAYMODE));
				HRESULT res = D3DInterface->EnumAdapterModes(adapter_index, enum_format, mode_index,&d3dmode);

				if (res == D3D_OK) {
					int bits = 0;
					switch (d3dmode.Format)
					{
						case D3DFMT_R8G8B8:
						case D3DFMT_A8R8G8B8:
						case D3DFMT_X8R8G8B8:		bits = 32; break;

						case D3DFMT_R5G6B5:
						case D3DFMT_X1R5G5B5:		bits = 16; break;
					}

					// Some cards fail in certain modes, DX9Caps keeps list of those.
					if (!dx9caps.Is_Valid_Display_Format(d3dmode.Width,d3dmode.Height,WW3D_Format_From_DX9(d3dmode.Format))) {
						bits=0;
					}

					/*
					** If we recognize the format, add it to the list
					** TODO: should we handle more formats?  will any cards report more than 24 or 16 bit?
					*/
					if (bits != 0) {
						desc.Add_Resolution(d3dmode.Width,d3dmode.Height,bits);
					}
				}
			}

			// IML: If the device has one or more valid resolutions add it to the device list.
			// NOTE: Testing has shown that there are drivers with zero resolutions.
			if (desc.Enumerate_Resolutions().Count() > 0) {

				/*
				** Set up the device name
				*/
				StringClass device_name(id.Description,true);
				_RenderDeviceNameTable.Add(device_name);
				_RenderDeviceShortNameTable.Add(device_name);	// for now, just add the same name to the "pretty name table"

				/*
				** Add the render device to our table
				*/
				_RenderDeviceDescriptionTable.Add(desc);
			}
		}
	}
}

bool DX9Backend::Set_Any_Render_Device()
{
	// Try fullscreen first
	int dev_number = 0;
	for (; dev_number < _RenderDeviceNameTable.Count(); dev_number++) {
		if (Set_Render_Device(dev_number,-1,-1,-1,0,false)) {
			return true;
		}
	}

	// Then windowed
	for (dev_number = 0; dev_number < _RenderDeviceNameTable.Count(); dev_number++) {
		if (Set_Render_Device(dev_number,-1,-1,-1,1,false)) {
			return true;
		}
	}

	return false;
}

bool DX9Backend::Set_Render_Device
(
	const char * dev_name,
	int width,
	int height,
	int bits,
	int windowed,
	bool resize_window
)
{
	for ( int dev_number = 0; dev_number < _RenderDeviceNameTable.Count(); dev_number++) {
		if ( strcmp( dev_name, _RenderDeviceNameTable[dev_number]) == 0) {
			return Set_Render_Device( dev_number, width, height, bits, windowed, resize_window );
		}

		if ( strcmp( dev_name, _RenderDeviceShortNameTable[dev_number]) == 0) {
			return Set_Render_Device( dev_number, width, height, bits, windowed, resize_window );
		}
	}
	return false;
}

void DX9Backend::Get_Format_Name(unsigned int format, StringClass *tex_format)
{
		*tex_format="Unknown";
		switch (format) {
		case D3DFMT_A8R8G8B8: *tex_format="D3DFMT_A8R8G8B8"; break;
		case D3DFMT_R8G8B8: *tex_format="D3DFMT_R8G8B8"; break;
		case D3DFMT_A4R4G4B4: *tex_format="D3DFMT_A4R4G4B4"; break;
		case D3DFMT_A1R5G5B5: *tex_format="D3DFMT_A1R5G5B5"; break;
		case D3DFMT_R5G6B5: *tex_format="D3DFMT_R5G6B5"; break;
		case D3DFMT_L8: *tex_format="D3DFMT_L8"; break;
		case D3DFMT_A8: *tex_format="D3DFMT_A8"; break;
		case D3DFMT_P8: *tex_format="D3DFMT_P8"; break;
		case D3DFMT_X8R8G8B8: *tex_format="D3DFMT_X8R8G8B8"; break;
		case D3DFMT_X1R5G5B5: *tex_format="D3DFMT_X1R5G5B5"; break;
		case D3DFMT_R3G3B2: *tex_format="D3DFMT_R3G3B2"; break;
		case D3DFMT_A8R3G3B2: *tex_format="D3DFMT_A8R3G3B2"; break;
		case D3DFMT_X4R4G4B4: *tex_format="D3DFMT_X4R4G4B4"; break;
		case D3DFMT_A8P8: *tex_format="D3DFMT_A8P8"; break;
		case D3DFMT_A8L8: *tex_format="D3DFMT_A8L8"; break;
		case D3DFMT_A4L4: *tex_format="D3DFMT_A4L4"; break;
		case D3DFMT_V8U8: *tex_format="D3DFMT_V8U8"; break;
		case D3DFMT_L6V5U5: *tex_format="D3DFMT_L6V5U5"; break;
		case D3DFMT_X8L8V8U8: *tex_format="D3DFMT_X8L8V8U8"; break;
		case D3DFMT_Q8W8V8U8: *tex_format="D3DFMT_Q8W8V8U8"; break;
		case D3DFMT_V16U16: *tex_format="D3DFMT_V16U16"; break;
#ifdef D3DFMT_W11V11U10
		case D3DFMT_W11V11U10: *tex_format="D3DFMT_W11V11U10"; break;
#endif
		case D3DFMT_UYVY: *tex_format="D3DFMT_UYVY"; break;
		case D3DFMT_YUY2: *tex_format="D3DFMT_YUY2"; break;
		case D3DFMT_DXT1: *tex_format="D3DFMT_DXT1"; break;
		case D3DFMT_DXT2: *tex_format="D3DFMT_DXT2"; break;
		case D3DFMT_DXT3: *tex_format="D3DFMT_DXT3"; break;
		case D3DFMT_DXT4: *tex_format="D3DFMT_DXT4"; break;
		case D3DFMT_DXT5: *tex_format="D3DFMT_DXT5"; break;
		case D3DFMT_D16_LOCKABLE: *tex_format="D3DFMT_D16_LOCKABLE"; break;
		case D3DFMT_D32: *tex_format="D3DFMT_D32"; break;
		case D3DFMT_D15S1: *tex_format="D3DFMT_D15S1"; break;
		case D3DFMT_D24S8: *tex_format="D3DFMT_D24S8"; break;
		case D3DFMT_D16: *tex_format="D3DFMT_D16"; break;
		case D3DFMT_D24X8: *tex_format="D3DFMT_D24X8"; break;
		case D3DFMT_D24X4S4: *tex_format="D3DFMT_D24X4S4"; break;
		default:	break;
		}
}

void DX9Backend::Resize_And_Position_Window()
{
	// Get the current dimensions of the 'render area' of the window
	RECT rect = { 0 };
	::GetClientRect (_Hwnd, &rect);

	// Is the window the correct size for this resolution?
	if ((rect.right-rect.left) != ResolutionWidth ||
			(rect.bottom-rect.top) != ResolutionHeight) {

		// Calculate what the main window's bounding rectangle should be to
		// accommodate this resolution
		rect.left = 0;
		rect.top = 0;
		rect.right = ResolutionWidth;
		rect.bottom = ResolutionHeight;
		DWORD dwstyle = ::GetWindowLong (_Hwnd, GWL_STYLE);
		AdjustWindowRect (&rect, dwstyle, FALSE);
		int width = rect.right-rect.left;
		int height = rect.bottom-rect.top;

		// Resize the window to fit this resolution
		if (!(IsWindowed || FullscreenMode == RenderBackendFullscreenMode::Borderless))
		{
			::SetWindowPos(_Hwnd, HWND_TOPMOST, 0, 0, width, height, 0);

			DEBUG_LOG(("Window resized to w:%d h:%d", width, height));
		}
		else
		{
			// TheSuperHackers @feature helmutbuhler 14/04/2025
			// Center the window in the workarea of the monitor it is on.
			MONITORINFO mi = {sizeof(MONITORINFO)};
			GetMonitorInfo(MonitorFromWindow(_Hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
			int left = (mi.rcWork.left + mi.rcWork.right - width) / 2;
			int top  = (mi.rcWork.top + mi.rcWork.bottom - height) / 2;

			// TheSuperHackers @feature helmutbuhler 14/04/2025
			// Move the window to try fit it into the monitor area, if one of its dimensions is larger than the work area.
			// Otherwise align the window to the top left edges, if it is even larger than the monitor area.
			RECT rectClient;
			rectClient.left = left - rect.left;
			rectClient.top = top - rect.top;
			rectClient.right = rectClient.left + ResolutionWidth;
			rectClient.bottom = rectClient.top + ResolutionHeight;
			MoveRectIntoOtherRect(rectClient, mi.rcMonitor, &left, &top);

			::SetWindowPos (_Hwnd, nullptr, left, top, width, height, SWP_NOZORDER);

			DEBUG_LOG(("Window positioned to x:%d y:%d, resized to w:%d h:%d", left, top, width, height));
		}
	}
}

bool DX9Backend::Set_Render_Device(int dev, int width, int height, int bits, int windowed,
								   bool resize_window,bool reset_device, bool restore_assets)
{
	WWASSERT(IsInitted);
	WWASSERT(dev >= -1);
	WWASSERT(dev < _RenderDeviceNameTable.Count());
	strlcpy(LastSetRenderDeviceError, "none", ARRAY_SIZE(LastSetRenderDeviceError));
	const char* setDevStage = "begin";
	try
	{

	/*
	** If user has never selected a render device, start out with device 0
	*/
	if ((CurRenderDevice == -1) && (dev == -1)) {
		CurRenderDevice = 0;
	} else if (dev != -1) {
		CurRenderDevice = dev;
	}

	/*
	** If user doesn't want to change res, set the res variables to match the
	** current resolution
	*/
	if (width != -1)		ResolutionWidth = width;
	if (height != -1)		ResolutionHeight = height;

	if (bits != -1)		BitDepth = bits;
	if (windowed != -1)	IsWindowed = (windowed != 0);

	WWDEBUG_SAY(("Attempting Set_Render_Device: name: %s (%s:%s), width: %d, height: %d, windowed: %d",
		_RenderDeviceNameTable[CurRenderDevice].str(),_RenderDeviceDescriptionTable[CurRenderDevice].Get_Driver_Name(),
		_RenderDeviceDescriptionTable[CurRenderDevice].Get_Driver_Version(),ResolutionWidth,ResolutionHeight,(IsWindowed ? 1 : 0)));

#ifdef _WIN32
	// PWG 4/13/2000 - changed so that if you say to resize the window it resizes
	// regardless of whether its windowed or not as OpenGL resizes its self around
	// the caption and edges of the window type you provide, so its important to
	// push the client area to be the size you really want.
	// if ( resize_window && windowed ) {
	if (resize_window) {
		Resize_And_Position_Window();
	}
#endif
	//must be either resetting existing device or creating a new one.
	WWASSERT(reset_device || D3DDevice == nullptr);

	/*
	** Initialize values for D3DPRESENT_PARAMETERS members.
	*/
	::ZeroMemory(&_PresentParameters, sizeof(D3DPRESENT_PARAMETERS));
	setDevStage = "Init D3DPRESENT_PARAMETERS";

	_PresentParameters.BackBufferWidth = ResolutionWidth;
	_PresentParameters.BackBufferHeight = ResolutionHeight;
	const bool presentation_windowed = IsWindowed ||
		FullscreenMode == RenderBackendFullscreenMode::Borderless;
	_PresentParameters.BackBufferCount = presentation_windowed ? 1 : 2;

	//I changed this to discard all the time (even when full-screen) since that the most efficient. 07-16-03 MW:
	_PresentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;//IsWindowed ? D3DSWAPEFFECT_DISCARD : D3DSWAPEFFECT_FLIP;		// Shouldn't this be D3DSWAPEFFECT_FLIP?
	_PresentParameters.hDeviceWindow = _Hwnd;
	_PresentParameters.Windowed = presentation_windowed;

	_PresentParameters.EnableAutoDepthStencil = TRUE;				// Driver will attempt to match Z-buffer depth
	_PresentParameters.Flags=0;											// We're not going to lock the backbuffer

	// Keep presentation synchronization independent from the presentation
	// parameter structure, which is rebuilt for every mode change.
	_PresentParameters.PresentationInterval = PresentationInterval;
	_PresentParameters.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;

	/*
	** Set up the buffer formats.  Several issues here:
	** - if in windowed mode, the backbuffer must use the current display format.
	** - the depth buffer must use
	*/
	if (presentation_windowed) {
		setDevStage = "Windowed mode setup";

		D3DDISPLAYMODE desktop_mode;
		::ZeroMemory(&desktop_mode, sizeof(D3DDISPLAYMODE));
		D3DInterface->GetAdapterDisplayMode( CurRenderDevice, &desktop_mode );

		DisplayFormat=_PresentParameters.BackBufferFormat = desktop_mode.Format;

		// In windowed mode, define the bitdepth from desktop mode (as it can't be changed)
		switch (_PresentParameters.BackBufferFormat) {
		case D3DFMT_X8R8G8B8:
		case D3DFMT_A8R8G8B8:
		case D3DFMT_R8G8B8: BitDepth=32; break;
		case D3DFMT_A4R4G4B4:
		case D3DFMT_A1R5G5B5:
		case D3DFMT_R5G6B5: BitDepth=16; break;
		case D3DFMT_L8:
		case D3DFMT_A8:
		case D3DFMT_P8: BitDepth=8; break;
		default:
			// Unknown backbuffer format probably means the device can't do windowed
			strlcpy(LastSetRenderDeviceError, "unknown windowed backbuffer format", ARRAY_SIZE(LastSetRenderDeviceError));
			return false;
		}

		if (BitDepth==32 && D3DInterface->CheckDeviceType(0,D3DDEVTYPE_HAL,desktop_mode.Format,D3DFMT_A8R8G8B8, TRUE) == D3D_OK)
		{	//promote 32-bit modes to include destination alpha
			_PresentParameters.BackBufferFormat = D3DFMT_A8R8G8B8;
		}

		/*
		** Find a appropriate Z buffer
		*/
		if (!Find_Z_Mode(DisplayFormat,_PresentParameters.BackBufferFormat,&_PresentParameters.AutoDepthStencilFormat))
		{
			// If opening 32 bit mode failed, try 16 bit, even if the desktop happens to be 32 bit
			if (BitDepth==32) {
				BitDepth=16;
				_PresentParameters.BackBufferFormat=D3DFMT_R5G6B5;
				if (!Find_Z_Mode(_PresentParameters.BackBufferFormat,_PresentParameters.BackBufferFormat,&_PresentParameters.AutoDepthStencilFormat)) {
					_PresentParameters.AutoDepthStencilFormat=D3DFMT_UNKNOWN;
				}
			}
			else {
				_PresentParameters.AutoDepthStencilFormat=D3DFMT_UNKNOWN;
			}
		}

	} else {
		setDevStage = "Fullscreen mode setup";

		/*
		** Try to find a mode that matches the user's desired bit-depth.
		*/
		Find_Color_And_Z_Mode(ResolutionWidth,ResolutionHeight,BitDepth,&DisplayFormat,
			&_PresentParameters.BackBufferFormat,&_PresentParameters.AutoDepthStencilFormat);
	}

	/*
	** Set default for depth stencil format if auto Z buffer failed.
	*/
	if (_PresentParameters.AutoDepthStencilFormat==D3DFMT_UNKNOWN) {
		if (BitDepth==32) {
			_PresentParameters.AutoDepthStencilFormat=D3DFMT_D32;
		}
		else {
			_PresentParameters.AutoDepthStencilFormat=D3DFMT_D16;
		}
	}

	/*
	** Check the devices support for the requested MSAA mode then setup the multi sample type
	*/
	if (MultiSampleAntiAliasing > D3DMULTISAMPLE_NONE) {

		HRESULT hrBack = D3DInterface->CheckDeviceMultiSampleType(
			CurRenderDevice,
			D3DDEVTYPE_HAL,
			_PresentParameters.BackBufferFormat,
			presentation_windowed,
			MultiSampleAntiAliasing,
			nullptr
		);

		HRESULT hrDepth = D3DInterface->CheckDeviceMultiSampleType(
			CurRenderDevice,
			D3DDEVTYPE_HAL,
			_PresentParameters.AutoDepthStencilFormat,
			presentation_windowed,
			MultiSampleAntiAliasing,
			nullptr
		);

		if (FAILED(hrBack) || FAILED(hrDepth)) {
			// IF we fail then disable MSAA entirely.
			// External code needs to retrieve the configured MSAA mode after device creation
			WWDEBUG_SAY(("Requested MSAA Mode Not Supported"));
			MultiSampleAntiAliasing = D3DMULTISAMPLE_NONE;
		}
	}

	_PresentParameters.MultiSampleType = MultiSampleAntiAliasing;

	/*
	** Time to actually create the device.
	*/
	StringClass displayFormat;
	StringClass backbufferFormat;

	Get_Format_Name(DisplayFormat,&displayFormat);
	Get_Format_Name(_PresentParameters.BackBufferFormat,&backbufferFormat);

	WWDEBUG_SAY(("Using Display/BackBuffer Formats: %s/%s",displayFormat.str(),backbufferFormat.str()));

	bool ret;

	if (reset_device)
	{
		setDevStage = "Reset_Device";
		WWDEBUG_SAY(("DX9Backend::Set_Render_Device is resetting the device."));
		ret = Reset_Device(restore_assets);	//reset device without restoring data - we're likely switching out of the app.
	}
	else
	{
		setDevStage = "Create_Device";
		ret = Create_Device();
	}

	WWDEBUG_SAY(("Reset/Create_Device done, reset_device=%d, restore_assets=%d", reset_device, restore_assets));
	if (!ret)
	{
		if (strcmp(LastSetRenderDeviceError, "none") == 0)
		{
			snprintf(
				LastSetRenderDeviceError,
				ARRAY_SIZE(LastSetRenderDeviceError),
				"Create/Reset failed, hr=0x%08lX, bbfmt=%u, zfmt=%u",
				static_cast<unsigned long>(LastCreateDeviceHRESULT),
				static_cast<unsigned>(_PresentParameters.BackBufferFormat),
				static_cast<unsigned>(_PresentParameters.AutoDepthStencilFormat));
		}
	}

	if (ret)
	{
		Render2DClass::Set_Screen_Resolution( RectClass( 0, 0, ResolutionWidth, ResolutionHeight ) );
	}

	return ret;
	}
	catch (...)
	{
		snprintf(
			LastSetRenderDeviceError,
			ARRAY_SIZE(LastSetRenderDeviceError),
			"Set_Render_Device threw an exception at %s",
			setDevStage);
		return false;
	}
}

bool DX9Backend::Set_Next_Render_Device()
{
	int new_dev = (CurRenderDevice + 1) % _RenderDeviceNameTable.Count();
	return Set_Render_Device(new_dev);
}

void DX9Backend::Set_Fullscreen_Mode(RenderBackendFullscreenMode mode)
{
	// The caller selects this policy before creating or resetting the device.
	// Keep it independent from IsWindowed: the latter is the logical game
	// display mode, while this controls the swap-chain ownership model.
	FullscreenMode = mode;
}

bool DX9Backend::Toggle_Windowed()
{
#ifdef WW3D_DX9
	// State OK?
	assert (IsInitted);
	if (IsInitted) {

		// Get information about the current render device's resolutions
		const RenderDeviceDescClass &render_device = Get_Render_Device_Desc ();
		const DynamicVectorClass<ResolutionDescClass> &resolutions = render_device.Enumerate_Resolutions ();
		if (resolutions.Count() == 0)
			return false;

		// Loop through all the resolutions supported by the current device.
		// If we aren't currently running under one of these resolutions,
		// then we should probably		 to the closest resolution before
		// toggling the windowed state.
		int curr_res = -1;
		for (int res = 0;
		     (res < resolutions.Count ()) && (curr_res == -1);
			  res ++) {

			// Is this the resolution we are looking for?
			if ((resolutions[res].Width == ResolutionWidth) &&
				 (resolutions[res].Height == ResolutionHeight) &&
				 (resolutions[res].BitDepth == BitDepth)) {
				curr_res = res;
			}
		}

		if (curr_res == -1) {

			// We don't match any of the standard resolutions,
			// so set the first resolution and toggle the windowed state.
			return Set_Device_Resolution (resolutions[0].Width,
								 resolutions[0].Height,
								 resolutions[0].BitDepth,
								 !IsWindowed, true);
		} else {

			// Toggle the windowed state
			return Set_Device_Resolution (-1, -1, -1, !IsWindowed, true);
		}
	}
#endif //WW3D_DX9

	return false;
}

void DX9Backend::Set_Swap_Interval(int swap)
{
	switch (swap) {
		case 0: PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; break;
		case 1: PresentationInterval = D3DPRESENT_INTERVAL_ONE ; break;
		case 2: PresentationInterval = D3DPRESENT_INTERVAL_TWO; break;
		case 3: PresentationInterval = D3DPRESENT_INTERVAL_THREE; break;
		default: PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; break;
	}
	_PresentParameters.PresentationInterval = PresentationInterval;

	WWDEBUG_SAY(("DX9Backend::Set_Swap_Interval is resetting the device."));
	Reset_Device();
}

int DX9Backend::Get_Swap_Interval() const
{
	return PresentationInterval;
}

bool DX9Backend::Has_Stencil() const
{
	bool has_stencil = (_PresentParameters.AutoDepthStencilFormat == D3DFMT_D24S8 ||
						_PresentParameters.AutoDepthStencilFormat == D3DFMT_D24X4S4);
	return has_stencil;
}

int DX9Backend::Get_Render_Device_Count() const
{
	return _RenderDeviceNameTable.Count();

}
int DX9Backend::Get_Render_Device() const
{
	assert(IsInitted);
	return CurRenderDevice;
}

const RenderDeviceDescClass & DX9Backend::Get_Render_Device_Desc(int deviceidx) const
{
	WWASSERT(IsInitted);

	if ((deviceidx == -1) && (CurRenderDevice == -1)) {
		CurRenderDevice = 0;
	}

	// if the device index is -1 then we want the current device
	if (deviceidx == -1) {
		WWASSERT(CurRenderDevice >= 0);
		WWASSERT(CurRenderDevice < _RenderDeviceNameTable.Count());
		return _RenderDeviceDescriptionTable[CurRenderDevice];
	}

	// We can only ask for multiple device information if the devices
	// have been detected.
	WWASSERT(deviceidx >= 0);
	WWASSERT(deviceidx < _RenderDeviceNameTable.Count());
	return _RenderDeviceDescriptionTable[deviceidx];
}

const char * DX9Backend::Get_Render_Device_Name(int device_index) const
{
	device_index = device_index % _RenderDeviceShortNameTable.Count();
	return _RenderDeviceShortNameTable[device_index];
}

bool DX9Backend::Set_Device_Resolution(int width,int height,int bits,int windowed, bool resize_window)
{
	if (D3DDevice == nullptr)
		return false;

	return Set_Render_Device(
		CurRenderDevice, width, height, bits, windowed,
		resize_window, true, true);
}

void DX9Backend::Get_Device_Resolution(int & set_w,int & set_h,int & set_bits,bool & set_windowed) const
{
	WWASSERT(IsInitted);

	set_w = ResolutionWidth;
	set_h = ResolutionHeight;
	set_bits = BitDepth;
	set_windowed = IsWindowed;
}

void DX9Backend::Get_Render_Target_Resolution(int & set_w,int & set_h,int & set_bits,bool & set_windowed) const
{
	WWASSERT(IsInitted);

	if (CurrentRenderTarget != nullptr) {
		D3DSURFACE_DESC info;
		CurrentRenderTarget->GetDesc (&info);

		set_w				= info.Width;
		set_h				= info.Height;
		set_bits			= BitDepth;		// should we get the actual bit depth of the target?
		set_windowed	= IsWindowed;	// this doesn't really make sense for render targets (shouldn't matter)...

	} else {
		Get_Device_Resolution (set_w, set_h, set_bits, set_windowed);
	}
}

bool DX9Backend::Registry_Save_Render_Device( const char * sub_key )
{
	int	width, height, depth;
	bool	windowed;
	Get_Device_Resolution(width, height, depth, windowed);
	return Registry_Save_Render_Device(sub_key, CurRenderDevice, ResolutionWidth, ResolutionHeight, BitDepth, IsWindowed, TextureBitDepth);
}

bool DX9Backend::Registry_Save_Render_Device( const char *sub_key, int device, int width, int height, int depth, bool windowed, int texture_depth)
{
	RegistryClass * registry = W3DNEW RegistryClass( sub_key );
	WWASSERT( registry );

	if ( !registry->Is_Valid() ) {
		delete registry;
		WWDEBUG_SAY(( "Error getting Registry" ));
		return false;
	}

	registry->Set_String( VALUE_NAME_RENDER_DEVICE_NAME,
		_RenderDeviceShortNameTable[device] );
	registry->Set_Int( VALUE_NAME_RENDER_DEVICE_WIDTH,	width );
	registry->Set_Int( VALUE_NAME_RENDER_DEVICE_HEIGHT, height );
	registry->Set_Int( VALUE_NAME_RENDER_DEVICE_DEPTH, depth );
	registry->Set_Int( VALUE_NAME_RENDER_DEVICE_WINDOWED, windowed );
	registry->Set_Int( VALUE_NAME_RENDER_DEVICE_TEXTURE_DEPTH, texture_depth );

	delete registry;
	return true;
}

bool DX9Backend::Registry_Load_Render_Device( const char * sub_key, bool resize_window )
{
	char	name[ 200 ];
	int	width,height,depth,windowed;

	if (	Registry_Load_Render_Device(	sub_key,
													name,
													sizeof(name),
													width,
													height,
													depth,
													windowed,
													TextureBitDepth) &&
			(*name != 0))
	{
		WWDEBUG_SAY(( "Device %s (%d X %d) %d bit windowed:%d", name,width,height,depth,windowed));

		if (TextureBitDepth==16 || TextureBitDepth==32) {
//			WWDEBUG_SAY(( "Texture depth %d", TextureBitDepth));
		} else {
			WWDEBUG_SAY(( "Invalid texture depth %d, switching to 16 bits", TextureBitDepth));
			TextureBitDepth=16;
		}

		if ( Set_Render_Device( name, width,height,depth,windowed, resize_window ) != true) {
			if (depth==16) depth=32;
			else depth=16;
			if ( Set_Render_Device( name, width,height,depth,windowed, resize_window ) == true) {
				return true;
			}
			if (depth==16) depth=32;
			else depth=16;
			// we'll test resolutions down, so if start is 640, increase to begin with...
			if (width==640) {
				width=1024;
				height=768;
			}
			for(;;) {
				if (width>2048) {
					width=2048;
					height=1536;
				}
				else if (width>1920) {
					width=1920;
					height=1440;
				}
				else if (width>1600) {
					width=1600;
					height=1200;
				}
				else if (width>1280) {
					width=1280;
					height=1024;
				}
				else if (width>1024) {
					width=1024;
					height=768;
				}
				else if (width>800) {
					width=800;
					height=600;
				}
				else if (width!=640) {
					width=640;
					height=480;
				}
				else {
					return Set_Any_Render_Device();
				}
				for (int i=0;i<2;++i) {
					if ( Set_Render_Device( name, width,height,depth,windowed, resize_window ) == true) {
						return true;
					}
					if (depth==16) depth=32;
					else depth=16;
				}
			}
		}

		return true;
	}

	WWDEBUG_SAY(( "Error getting Registry" ));

	return Set_Any_Render_Device();
}

bool DX9Backend::Registry_Load_Render_Device( const char * sub_key, char *device, int device_len, int &width, int &height, int &depth, int &windowed, int &texture_depth)
{
	RegistryClass registry( sub_key );

	if ( registry.Is_Valid() ) {
		registry.Get_String( VALUE_NAME_RENDER_DEVICE_NAME,
			device, device_len);

		width =		registry.Get_Int( VALUE_NAME_RENDER_DEVICE_WIDTH, -1 );
		height =		registry.Get_Int( VALUE_NAME_RENDER_DEVICE_HEIGHT, -1 );
		depth =		registry.Get_Int( VALUE_NAME_RENDER_DEVICE_DEPTH, -1 );
		windowed =	registry.Get_Int( VALUE_NAME_RENDER_DEVICE_WINDOWED, -1 );
		texture_depth = registry.Get_Int( VALUE_NAME_RENDER_DEVICE_TEXTURE_DEPTH, -1 );
		return true;
	}
	*device=0;
	width=-1;
	height=-1;
	depth=-1;
	windowed=-1;
	texture_depth=-1;
	return false;
}


bool DX9Backend::Find_Color_And_Z_Mode(int resx,int resy,int bitdepth,D3DFORMAT * set_colorbuffer,D3DFORMAT * set_backbuffer,D3DFORMAT * set_zmode)
{
	static D3DFORMAT _formats16[] =
	{
		D3DFMT_R5G6B5,
		D3DFMT_X1R5G5B5,
		D3DFMT_A1R5G5B5
	};

	static D3DFORMAT _formats32[] =
	{
		D3DFMT_A8R8G8B8,
		D3DFMT_X8R8G8B8,
		D3DFMT_R8G8B8,
	};

	/*
	** Select the table that we're going to use to search for a valid backbuffer format
	*/
	D3DFORMAT * format_table = nullptr;
	int format_count = 0;

	if (BitDepth == 16) {
		format_table = _formats16;
		format_count = sizeof(_formats16) / sizeof(D3DFORMAT);
	} else {
		format_table = _formats32;
		format_count = sizeof(_formats32) / sizeof(D3DFORMAT);
	}

	/*
	** now search for a valid format
	*/
	bool found = false;
	unsigned int mode = 0;

	int format_index=0;
	for (; format_index < format_count; format_index++) {
		found |= Find_Color_Mode(format_table[format_index],resx,resy,&mode);
		if (found) break;
	}

	if (!found) {
		return false;
	} else {
		*set_backbuffer=*set_colorbuffer = format_table[format_index];
	}

	if (bitdepth==32 && *set_colorbuffer == D3DFMT_X8R8G8B8 && D3DInterface->CheckDeviceType(0,D3DDEVTYPE_HAL,*set_colorbuffer,D3DFMT_A8R8G8B8, TRUE) == D3D_OK)
	{	//promote 32-bit modes to include destination alpha when supported
		*set_backbuffer = D3DFMT_A8R8G8B8;
	}

	/*
	** We found a backbuffer format, now find a zbuffer format
	*/
	return Find_Z_Mode(*set_colorbuffer,*set_backbuffer, set_zmode);
};


// find the resolution mode with at least resx,resy with the highest supported
// refresh rate
bool DX9Backend::Find_Color_Mode(D3DFORMAT colorbuffer, int resx, int resy, UINT *mode)
{
	UINT i,j,modemax;
	UINT rx,ry;
	D3DDISPLAYMODE dmode;
	::ZeroMemory(&dmode, sizeof(D3DDISPLAYMODE));

	rx=(unsigned int) resx;
	ry=(unsigned int) resy;

	bool found=false;

	modemax=D3DInterface->GetAdapterModeCount(D3DADAPTER_DEFAULT, colorbuffer);

	i=0;

	while (i<modemax && !found)
	{
		D3DInterface->EnumAdapterModes(D3DADAPTER_DEFAULT, colorbuffer, i, &dmode);
		if (dmode.Width==rx && dmode.Height==ry && dmode.Format==colorbuffer) {
			WWDEBUG_SAY(("Found valid color mode.  Width = %d Height = %d Format = %d",dmode.Width,dmode.Height,dmode.Format));
			found=true;
		}
		i++;
	}

	i--; // this is the first valid mode

	// no match
	if (!found) {
		WWDEBUG_SAY(("Failed to find a valid color mode"));
		return false;
	}

	// go to the highest refresh rate in this mode
	bool stillok=true;

	j=i;
	while (j<modemax && stillok)
	{
		D3DInterface->EnumAdapterModes(D3DADAPTER_DEFAULT, colorbuffer, j, &dmode);
		if (dmode.Width==rx && dmode.Height==ry && dmode.Format==colorbuffer)
			stillok=true; else stillok=false;
		j++;
	}

	if (stillok==false) *mode=j-2;
	else *mode=i;

	return true;
}

// Helper function to find a Z buffer mode for the colorbuffer
// Will look for greatest Z precision
bool DX9Backend::Find_Z_Mode(D3DFORMAT colorbuffer,D3DFORMAT backbuffer, D3DFORMAT *zmode)
{
	//MW: Swapped the next 2 tests so that Stencil modes get tested first.
	if (Test_Z_Mode(colorbuffer,backbuffer,D3DFMT_D24S8))
	{
		*zmode=D3DFMT_D24S8;
		WWDEBUG_SAY(("Found zbuffer mode D3DFMT_D24S8"));
		return true;
	}

	if (Test_Z_Mode(colorbuffer,backbuffer,D3DFMT_D32))
	{
		*zmode=D3DFMT_D32;
		WWDEBUG_SAY(("Found zbuffer mode D3DFMT_D32"));
		return true;
	}

	if (Test_Z_Mode(colorbuffer,backbuffer,D3DFMT_D24X8))
	{
		*zmode=D3DFMT_D24X8;
		WWDEBUG_SAY(("Found zbuffer mode D3DFMT_D24X8"));
		return true;
	}

	if (Test_Z_Mode(colorbuffer,backbuffer,D3DFMT_D24X4S4))
	{
		*zmode=D3DFMT_D24X4S4;
		WWDEBUG_SAY(("Found zbuffer mode D3DFMT_D24X4S4"));
		return true;
	}

	if (Test_Z_Mode(colorbuffer,backbuffer,D3DFMT_D16))
	{
		*zmode=D3DFMT_D16;
		WWDEBUG_SAY(("Found zbuffer mode D3DFMT_D16"));
		return true;
	}

	if (Test_Z_Mode(colorbuffer,backbuffer,D3DFMT_D15S1))
	{
		*zmode=D3DFMT_D15S1;
		WWDEBUG_SAY(("Found zbuffer mode D3DFMT_D15S1"));
		return true;
	}

	// can't find a match
	WWDEBUG_SAY(("Failed to find a valid zbuffer mode"));
	return false;
}

bool DX9Backend::Test_Z_Mode(D3DFORMAT colorbuffer,D3DFORMAT backbuffer, D3DFORMAT zmode)
{
	// See if we have this mode first
	if (FAILED(D3DInterface->CheckDeviceFormat(D3DADAPTER_DEFAULT,WW3D_DEVTYPE,
		colorbuffer,D3DUSAGE_DEPTHSTENCIL,D3DRTYPE_SURFACE,zmode)))
	{
		WWDEBUG_SAY(("CheckDeviceFormat failed.  Colorbuffer format = %d  Zbufferformat = %d",colorbuffer,zmode));
		return false;
	}

	// Then see if it matches the color buffer
	if(FAILED(D3DInterface->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, WW3D_DEVTYPE,
		colorbuffer,backbuffer,zmode)))
	{
		WWDEBUG_SAY(("CheckDepthStencilMatch failed.  Colorbuffer format = %d  Backbuffer format = %d Zbufferformat = %d",colorbuffer,backbuffer,zmode));
		return false;
	}
	return true;
}


void DX9Backend::Reset_Statistics()
{
	FrameStatistics = RenderFrameStatistics();
	LastFrameStatistics = RenderFrameStatistics();
}

void DX9Backend::Begin_Statistics()
{
	FrameStatistics = RenderFrameStatistics();
}

void DX9Backend::End_Statistics()
{
	LastFrameStatistics = FrameStatistics;
}

const RenderFrameStatistics& DX9Backend::Get_Last_Frame_Statistics()
{
	return LastFrameStatistics;
}

unsigned long DX9Backend::Get_FrameCount() {return FrameCount;}

void DX9_Assert()
{
	WWASSERT(DX9Backend::_Get_D3D9());
	DX9_THREAD_ASSERT();
}

void DX9Backend::Begin_Scene()
{
	DX9_THREAD_ASSERT();

	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend != nullptr)
		backend->Update_Browser();

	DX9CALL(BeginScene());
}

void DX9Backend::End_Scene(bool flip_frames)
{
	DX9_THREAD_ASSERT();
	DX9CALL(EndScene());

	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend != nullptr)
		backend->Render_Browser(0);

	if (flip_frames) {
		DX9_Assert();
		HRESULT hr;
		{
			WWPROFILE("DX9Device::Present()");
			hr=_Get_D3D_Device()->Present(nullptr, nullptr, nullptr, nullptr);
		}

		DX9_RECORD_DX9_CALLS();

		if (SUCCEEDED(hr)) {
			IRenderBackend *backend = WW3D::Get_Render_Backend();
			if (backend != nullptr && backend->Get_Debug_Settings().m_sleepTime) {
				::Sleep(backend->Get_Debug_Settings().m_sleepTime);
			}
			IsDeviceLost=false;
			FrameCount++;
		}
		else {
			IsDeviceLost=true;
		}

		// If the device was lost we need to check for cooperative level and possibly reset the device
		if (hr==D3DERR_DEVICELOST) {
			hr=_Get_D3D_Device()->TestCooperativeLevel();
			if (hr==D3DERR_DEVICENOTRESET) {
				WWDEBUG_SAY(("DX9Backend::End_Scene is resetting the device."));
				Reset_Device();
			}
			else {
				// Sleep it not active
				ThreadClass::Sleep_Ms(200);
			}
		}
		else {
			DX9_ErrorCode(hr);
		}
	}

	// Each frame, release all of the buffers and textures.
	Set_Vertex_Buffer(nullptr);
	Set_Index_Buffer(nullptr,0);
	for (int i=0;i<CurrentCaps->Get_Max_Textures_Per_Pass();++i) Set_Texture(i,nullptr);
	Set_Material(nullptr);
}


void DX9Backend::Flip_To_Primary()
{
	// If we are fullscreen and the current frame is odd then we need
	// to force a page flip to ensure that the first buffer in the flipping
	// chain is the one visible.
	if (!(IsWindowed || FullscreenMode == RenderBackendFullscreenMode::Borderless)) {
		DX9_Assert();

		int numBuffers = (_PresentParameters.BackBufferCount + 1);
		int visibleBuffer = (FrameCount % numBuffers);
		int flipCount = ((numBuffers - visibleBuffer) % numBuffers);
		int resetAttempts = 0;

		while ((flipCount > 0) && (resetAttempts < 3)) {
		HRESULT hr = _Get_D3D_Device()->TestCooperativeLevel();

			if (FAILED(hr)) {
				WWDEBUG_SAY(("TestCooperativeLevel Failed!"));

				if (D3DERR_DEVICELOST == hr) {
					IsDeviceLost=true;
					WWDEBUG_SAY(("DEVICELOST: Cannot flip to primary."));
					return;
				}
				IsDeviceLost=false;

				if (D3DERR_DEVICENOTRESET == hr) {
					WWDEBUG_SAY(("DEVICENOTRESET"));
					Reset_Device();
					resetAttempts++;
				}
			} else {
				WWDEBUG_SAY(("Flipping: %ld", FrameCount));
				hr = _Get_D3D_Device()->Present(nullptr, nullptr, nullptr, nullptr);

				if (SUCCEEDED(hr)) {
					IsDeviceLost=false;
					FrameCount++;
					WWDEBUG_SAY(("Flip to primary succeeded %ld", FrameCount));
				}
				else {
					IsDeviceLost=true;
				}
			}

			--flipCount;
		}
	}
}


//**********************************************************************************************
//! Clear current render device
/*! KM
/* 5/17/02 KM Fixed support for render to texture with depth/stencil buffers
*/
void DX9Backend::Clear(bool clear_color, bool clear_z_stencil, const Vector3 &color, float dest_alpha, float z, unsigned int stencil)
{
	DX9_THREAD_ASSERT();

	// If we try to clear a stencil buffer which is not there, the entire call will fail
	// KJM fixed this to get format from back buffer (incase render to texture is used)
	/*bool has_stencil = (	_PresentParameters.AutoDepthStencilFormat == D3DFMT_D15S1 ||
								_PresentParameters.AutoDepthStencilFormat == D3DFMT_D24S8 ||
								_PresentParameters.AutoDepthStencilFormat == D3DFMT_D24X4S4);*/
	bool has_stencil=false;
	IDirect3DSurface9* depthbuffer;

	_Get_D3D_Device()->GetDepthStencilSurface(&depthbuffer);
	DX9_RECORD_DX9_CALLS();

	if (depthbuffer)
	{
		D3DSURFACE_DESC desc;
		depthbuffer->GetDesc(&desc);
		has_stencil=
		(
			desc.Format==D3DFMT_D15S1 ||
			desc.Format==D3DFMT_D24S8 ||
			desc.Format==D3DFMT_D24X4S4
		);

		// release ref
		depthbuffer->Release();
	}

	DWORD flags = 0;
	if (clear_color) flags |= D3DCLEAR_TARGET;
	if (clear_z_stencil) flags |= D3DCLEAR_ZBUFFER;
	if (clear_z_stencil && has_stencil) flags |= D3DCLEAR_STENCIL;
	if (flags)
	{
		DX9CALL(Clear(0, nullptr, flags, Convert_Color(color,dest_alpha), z, stencil));
	}
}

void DX9Backend::Set_Viewport(CONST D3DVIEWPORT9* pViewport)
{
	DX9_THREAD_ASSERT();
	DX9CALL(SetViewport(pViewport));
}

// ----------------------------------------------------------------------------
//
// Set vertex buffer. A reference to previous vertex buffer is released and
// this one is assigned the current vertex buffer. The DX9 vertex buffer will
// actually be set in Apply() which is called by Draw_Indexed_Triangles().
//
// ----------------------------------------------------------------------------

void DX9Backend::Set_Vertex_Buffer(const VertexBufferClass* vb, unsigned stream)
{
	render_state.vba_offset=0;
	render_state.vba_count=0;
	if (render_state.vertex_buffers[stream]) {
		render_state.vertex_buffers[stream]->Release_Engine_Ref();
	}
	REF_PTR_SET(render_state.vertex_buffers[stream],const_cast<VertexBufferClass*>(vb));
	if (vb) {
		vb->Add_Engine_Ref();
		render_state.vertex_buffer_types[stream]=vb->Type();
	}
	else {
		render_state.vertex_buffer_types[stream]=BUFFER_TYPE_INVALID;
	}
	render_state_changed|=VERTEX_BUFFER_CHANGED;
}

// ----------------------------------------------------------------------------
//
// Set index buffer. A reference to previous index buffer is released and
// this one is assigned the current index buffer. The DX9 index buffer will
// actually be set in Apply() which is called by Draw_Indexed_Triangles().
//
// ----------------------------------------------------------------------------

void DX9Backend::Set_Index_Buffer(const IndexBufferClass* ib,unsigned short index_base_offset)
{
	render_state.iba_offset=0;
	if (render_state.index_buffer) {
		render_state.index_buffer->Release_Engine_Ref();
	}
	REF_PTR_SET(render_state.index_buffer,const_cast<IndexBufferClass*>(ib));
	render_state.index_base_offset=index_base_offset;
	if (ib) {
		ib->Add_Engine_Ref();
		render_state.index_buffer_type=ib->Type();
	}
	else {
		render_state.index_buffer_type=BUFFER_TYPE_INVALID;
	}
	render_state_changed|=INDEX_BUFFER_CHANGED;
}

// ----------------------------------------------------------------------------
//
// Set vertex buffer using dynamic access object.
//
// ----------------------------------------------------------------------------

void DX9Backend::Set_Vertex_Buffer(const DynamicVBAccessClass& vba_)
{
	// Release all streams (only one stream allowed in the legacy pipeline)
	for (int i=1;i<MAX_VERTEX_STREAMS;++i) {
		DX9Backend::Set_Vertex_Buffer(nullptr, i);
	}

	if (render_state.vertex_buffers[0]) render_state.vertex_buffers[0]->Release_Engine_Ref();
	DynamicVBAccessClass& vba=const_cast<DynamicVBAccessClass&>(vba_);
	render_state.vertex_buffer_types[0]=vba.Get_Type();
	render_state.vba_offset=vba.Get_Vertex_Buffer_Offset();
	render_state.vba_count=vba.Get_Vertex_Count();
	REF_PTR_SET(render_state.vertex_buffers[0],vba.Get_Vertex_Buffer());
	render_state.vertex_buffers[0]->Add_Engine_Ref();
	render_state_changed|=VERTEX_BUFFER_CHANGED;
	render_state_changed|=INDEX_BUFFER_CHANGED;		// vba_offset changes so index buffer needs to be reset as well.
}

// ----------------------------------------------------------------------------
//
// Set index buffer using dynamic access object.
//
// ----------------------------------------------------------------------------

void DX9Backend::Set_Index_Buffer(const DynamicIBAccessClass& iba_,unsigned short index_base_offset)
{
	if (render_state.index_buffer) render_state.index_buffer->Release_Engine_Ref();

	DynamicIBAccessClass& iba=const_cast<DynamicIBAccessClass&>(iba_);
	render_state.index_base_offset=index_base_offset;
	render_state.index_buffer_type=iba.Get_Type();
	render_state.iba_offset=iba.Get_Index_Buffer_Offset();
	REF_PTR_SET(render_state.index_buffer,iba.Get_Index_Buffer());
	render_state.index_buffer->Add_Engine_Ref();
	render_state_changed|=INDEX_BUFFER_CHANGED;
}

// ----------------------------------------------------------------------------
//
// Private function for the special case of rendering polygons from sorting
// index and vertex buffers.
//
// ----------------------------------------------------------------------------

void DX9Backend::Draw_Sorting_IB_VB(
	unsigned primitive_type,
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	WWASSERT(render_state.vertex_buffer_types[0]==BUFFER_TYPE_SORTING || render_state.vertex_buffer_types[0]==BUFFER_TYPE_DYNAMIC_SORTING);
	WWASSERT(render_state.index_buffer_type==BUFFER_TYPE_SORTING || render_state.index_buffer_type==BUFFER_TYPE_DYNAMIC_SORTING);

	// Fill dynamic vertex buffer with sorting vertex buffer vertices
	DynamicVBAccessClass dyn_vb_access(BUFFER_TYPE_DYNAMIC_RENDER,
		RenderBackend_Dynamic_Vertex_Format, vertex_count);
	{
		DynamicVBAccessClass::WriteLockClass lock(&dyn_vb_access);
		VertexFormatXYZNDUV2* src = static_cast<SortingVertexBufferClass*>(
			render_state.vertex_buffers[0])->Get_Vertex_Array();
		VertexFormatXYZNDUV2* dest= lock.Get_Formatted_Vertex_Array();
		src += render_state.vba_offset + render_state.index_base_offset + min_vertex_index;
		unsigned  size = dyn_vb_access.Get_Format_Info().Get_Vertex_Size() *
			vertex_count / sizeof(unsigned);
		unsigned *dest_u =(unsigned*) dest;
		unsigned *src_u = (unsigned*) src;

		for (unsigned i=0;i<size;++i) {
			*dest_u++=*src_u++;
		}
	}

	Set_Vertex_Buffer(dyn_vb_access.Get_Vertex_Buffer()->Get_Backend_Buffer(),
		0, dyn_vb_access.Get_Format_Info().Get_Vertex_Size());
	// Sorting buffers are copied into a fixed-format dynamic buffer. The
	// legacy path selected that buffer's FVF immediately after binding it;
	// retain that state transition inside the backend.
	Set_Vertex_Format(dyn_vb_access.Get_Format());

	unsigned index_count=0;
	switch (primitive_type) {
	case D3DPT_TRIANGLELIST: index_count=polygon_count*3; break;
	case D3DPT_TRIANGLESTRIP: index_count=polygon_count+2; break;
	case D3DPT_TRIANGLEFAN: index_count=polygon_count+2; break;
	default: WWASSERT(0); break; // Unsupported primitive type
	}

	// Fill dynamic index buffer with sorting index buffer vertices
	DynamicIBAccessClass dyn_ib_access(BUFFER_TYPE_DYNAMIC_RENDER,index_count);
	{
		DynamicIBAccessClass::WriteLockClass lock(&dyn_ib_access);
		unsigned short* dest=lock.Get_Index_Array();
		unsigned short* src=nullptr;
		src=static_cast<SortingIndexBufferClass*>(render_state.index_buffer)->Get_Index_Array();
		src+=render_state.iba_offset+start_index;

		for (unsigned short i=0;i<index_count;++i) {
			unsigned short index=*src++;
			index-=min_vertex_index;
			WWASSERT(index<vertex_count);
			*dest++=index;
		}
	}

	Set_Index_Buffer(dyn_ib_access.Get_Index_Buffer()->Get_Backend_Buffer());

	DX9_RECORD_DRAW_CALLS();
	DX9CALL(DrawIndexedPrimitive(
		D3DPT_TRIANGLELIST,
		dyn_vb_access.Get_Vertex_Buffer_Offset(),
		0,
		vertex_count,
		dyn_ib_access.Get_Index_Buffer_Offset(),
		polygon_count));

	ShaderClass current_shader(render_state.shader_bits);
	RECORD_RENDER(polygon_count,vertex_count,current_shader);
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

void DX9Backend::Draw(
	unsigned primitive_type,
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	if (DrawPolygonLowBoundLimit && DrawPolygonLowBoundLimit>=polygon_count) return;

	DX9_THREAD_ASSERT();
	SNAPSHOT_SAY(("DX9 - draw"));

	Apply_Render_State_Changes();

	// Debug feature to disable triangle drawing...
	if (!_Is_Triangle_Draw_Enabled()) return;

#ifdef MESH_RENDER_SNAPSHOT_ENABLED
	if (WW3D::Is_Snapshot_Activated()) {
		unsigned long passes=0;
		SNAPSHOT_SAY(("ValidateDevice:"));
		HRESULT res=D3DDevice->ValidateDevice(&passes);
		switch (res) {
		case D3D_OK:
			SNAPSHOT_SAY(("OK"));
			break;

		case D3DERR_CONFLICTINGTEXTUREFILTER:
			SNAPSHOT_SAY(("D3DERR_CONFLICTINGTEXTUREFILTER"));
			break;
		case D3DERR_CONFLICTINGTEXTUREPALETTE:
			SNAPSHOT_SAY(("D3DERR_CONFLICTINGTEXTUREPALETTE"));
			break;
		case D3DERR_DEVICELOST:
			SNAPSHOT_SAY(("D3DERR_DEVICELOST"));
			break;
		case D3DERR_TOOMANYOPERATIONS:
			SNAPSHOT_SAY(("D3DERR_TOOMANYOPERATIONS"));
			break;
		case D3DERR_UNSUPPORTEDALPHAARG:
			SNAPSHOT_SAY(("D3DERR_UNSUPPORTEDALPHAARG"));
			break;
		case D3DERR_UNSUPPORTEDALPHAOPERATION:
			SNAPSHOT_SAY(("D3DERR_UNSUPPORTEDALPHAOPERATION"));
			break;
		case D3DERR_UNSUPPORTEDCOLORARG:
			SNAPSHOT_SAY(("D3DERR_UNSUPPORTEDCOLORARG"));
			break;
		case D3DERR_UNSUPPORTEDCOLOROPERATION:
			SNAPSHOT_SAY(("D3DERR_UNSUPPORTEDCOLOROPERATION"));
			break;
		case D3DERR_UNSUPPORTEDFACTORVALUE:
			SNAPSHOT_SAY(("D3DERR_UNSUPPORTEDFACTORVALUE"));
			break;
		case D3DERR_UNSUPPORTEDTEXTUREFILTER:
			SNAPSHOT_SAY(("D3DERR_UNSUPPORTEDTEXTUREFILTER"));
			break;
		case D3DERR_WRONGTEXTUREFORMAT:
			SNAPSHOT_SAY(("D3DERR_WRONGTEXTUREFORMAT"));
			break;
		default:
			SNAPSHOT_SAY(("UNKNOWN Error"));
			break;
		}
	}
#endif	// MESH_RENDER_SNAPSHOT_ENABLED


	SNAPSHOT_SAY(("DX9 - draw %d polygons (%d vertices)",polygon_count,vertex_count));

	if (vertex_count<3) {
		min_vertex_index=0;
		switch (render_state.vertex_buffer_types[0]) {
		case BUFFER_TYPE_RENDER:
		case BUFFER_TYPE_SORTING:
			vertex_count=render_state.vertex_buffers[0]->Get_Vertex_Count()-render_state.index_base_offset-render_state.vba_offset-min_vertex_index;
			break;
		case BUFFER_TYPE_DYNAMIC_RENDER:
		case BUFFER_TYPE_DYNAMIC_SORTING:
			vertex_count=render_state.vba_count;
			break;
		}
	}

	switch (render_state.vertex_buffer_types[0]) {
	case BUFFER_TYPE_RENDER:
	case BUFFER_TYPE_DYNAMIC_RENDER:
		switch (render_state.index_buffer_type) {
		case BUFFER_TYPE_RENDER:
		case BUFFER_TYPE_DYNAMIC_RENDER:
			{
/*				if ((start_index+render_state.iba_offset+polygon_count*3) > render_state.index_buffer->Get_Index_Count())
				{	WWASSERT_PRINT(0,"OVERFLOWING INDEX BUFFER");
					///@todo: MUST FIND OUT WHY THIS HAPPENS WITH LOTS OF PARTICLES ON BIG FIGHT!  -MW
					break;
				}*/
				ShaderClass current_shader(render_state.shader_bits);
				RECORD_RENDER(polygon_count,vertex_count,current_shader);
				DX9_RECORD_DRAW_CALLS();
				DX9CALL(DrawIndexedPrimitive(
					(D3DPRIMITIVETYPE)primitive_type,
					render_state.index_base_offset+render_state.vba_offset,
					min_vertex_index,
					vertex_count,
					start_index+render_state.iba_offset,
					polygon_count));
			}
			break;
		case BUFFER_TYPE_SORTING:
		case BUFFER_TYPE_DYNAMIC_SORTING:
			WWASSERT_PRINT(0,"VB and IB must of same type (sorting or dx9)");
			break;
		case BUFFER_TYPE_INVALID:
			WWASSERT(0);
			break;
		}
		break;
	case BUFFER_TYPE_SORTING:
	case BUFFER_TYPE_DYNAMIC_SORTING:
		switch (render_state.index_buffer_type) {
		case BUFFER_TYPE_RENDER:
		case BUFFER_TYPE_DYNAMIC_RENDER:
			WWASSERT_PRINT(0,"VB and IB must of same type (sorting or dx9)");
			break;
		case BUFFER_TYPE_SORTING:
		case BUFFER_TYPE_DYNAMIC_SORTING:
			Draw_Sorting_IB_VB(primitive_type,start_index,polygon_count,min_vertex_index,vertex_count);
			break;
		case BUFFER_TYPE_INVALID:
			WWASSERT(0);
			break;
		}
		break;
	case BUFFER_TYPE_INVALID:
		WWASSERT(0);
		break;
	}
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

void DX9Backend::Draw_Triangles(
	unsigned buffer_type,
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	if (buffer_type==BUFFER_TYPE_SORTING || buffer_type==BUFFER_TYPE_DYNAMIC_SORTING) {
		SortingRendererClass::Insert_Triangles(start_index,polygon_count,min_vertex_index,vertex_count);
	}
	else {
		Draw(D3DPT_TRIANGLELIST,start_index,polygon_count,min_vertex_index,vertex_count);
	}
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

void DX9Backend::Draw_Triangles(
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	Draw(D3DPT_TRIANGLELIST,start_index,polygon_count,min_vertex_index,vertex_count);
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

void DX9Backend::Draw_Strip(
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	Draw(D3DPT_TRIANGLESTRIP,start_index,polygon_count,min_vertex_index,vertex_count);
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

void DX9Backend::Apply_Render_State_Changes()
{
	SNAPSHOT_SAY(("DX9Backend::Apply_Render_State_Changes()"));

	if (!render_state_changed) return;
	if (render_state_changed&SHADER_CHANGED) {
		SNAPSHOT_SAY(("DX9 - apply shader"));
		ShaderClass current_shader(render_state.shader_bits);
		current_shader.Apply();
	}

	unsigned mask=TEXTURE0_CHANGED;
	int i=0;
	for (;i<CurrentCaps->Get_Max_Textures_Per_Pass();++i,mask<<=1)
	{
		if (render_state_changed&mask)
		{
			SNAPSHOT_SAY(("DX9 - apply texture %d (%s)",i,render_state.Textures[i] ? render_state.Textures[i]->Get_Full_Path().str() : "null"));

			if (render_state.Textures[i])
			{
				render_state.Textures[i]->Apply(i);
			}
			else
			{
				TextureBaseClass::Apply_Null(i);
			}
		}
	}

	if (render_state_changed&MATERIAL_CHANGED)
	{
		SNAPSHOT_SAY(("DX9 - apply material"));
		VertexMaterialClass* material=const_cast<VertexMaterialClass*>(render_state.material);
		if (material)
		{
			material->Apply();
		}
		else VertexMaterialClass::Apply_Null();
	}

	if (render_state_changed&LIGHTS_CHANGED)
	{
		unsigned mask=LIGHT0_CHANGED;
		for (unsigned index=0;index<4;++index,mask<<=1) {
			if (render_state_changed&mask) {
				SNAPSHOT_SAY(("DX9 - apply light %d",index));
				if (render_state.LightEnable[index]) {
#ifdef MESH_RENDER_SNAPSHOT_ENABLED
					if ( WW3D::Is_Snapshot_Activated() ) {
						const RenderBackendLight * state_light = &(render_state.Lights[index]);
						static const char * _light_types[] = { "Unknown", "Point","Spot", "Directional" };
						const unsigned light_type = static_cast<unsigned>(state_light->type);
						WWASSERT(light_type < 4);

						SNAPSHOT_SAY((" type = %s amb = %4.2f,%4.2f,%4.2f  diff = %4.2f,%4.2f,%4.2f spec = %4.2f, %4.2f, %4.2f",
							_light_types[light_type],
							state_light->ambient[0],state_light->ambient[1],state_light->ambient[2],
							state_light->diffuse[0],state_light->diffuse[1],state_light->diffuse[2],
							state_light->specular[0],state_light->specular[1],state_light->specular[2] ));
						SNAPSHOT_SAY((" pos = %f, %f, %f  dir = %f, %f, %f",
							state_light->position[0], state_light->position[1], state_light->position[2],
							state_light->direction[0], state_light->direction[1], state_light->direction[2] ));
					}
#endif

					D3DLIGHT9 d3d_light;
					To_DX9_Light(render_state.Lights[index], d3d_light);
					Set_DX9_Light(index,&d3d_light);
				}
				else {
					Set_DX9_Light(index,nullptr);
					SNAPSHOT_SAY((" clearing light to null"));
				}
			}
		}
	}

	if (render_state_changed&WORLD_CHANGED) {
		SNAPSHOT_SAY(("DX9 - apply world matrix"));
		D3DMATRIX world = To_D3DMATRIX(render_state.world);
		_Set_DX9_Transform(D3DTS_WORLD,world);
	}
	if (render_state_changed&VIEW_CHANGED) {
		SNAPSHOT_SAY(("DX9 - apply view matrix"));
		D3DMATRIX view = To_D3DMATRIX(render_state.view);
		_Set_DX9_Transform(D3DTS_VIEW,view);
	}
	if (render_state_changed&VERTEX_BUFFER_CHANGED) {
		SNAPSHOT_SAY(("DX9 - apply vb change"));
		for (i=0;i<MAX_VERTEX_STREAMS;++i) {
			if (render_state.vertex_buffers[i]) {
				switch (render_state.vertex_buffer_types[i]) {//->Type()) {
				case BUFFER_TYPE_RENDER:
				case BUFFER_TYPE_DYNAMIC_RENDER:
					Set_Vertex_Buffer(render_state.vertex_buffers[i]->Get_Backend_Buffer(),
						0, render_state.vertex_buffers[i]->Get_Vertex_Size(), i);
					break;
				case BUFFER_TYPE_SORTING:
				case BUFFER_TYPE_DYNAMIC_SORTING:
					break;
				default:
					WWASSERT(0);
				}
			} else {
				DX9CALL(SetStreamSource(i,nullptr,0,0));
				DX9_RECORD_VERTEX_BUFFER_CHANGE();
			}
		}
	}
	if (render_state_changed&INDEX_BUFFER_CHANGED) {
		SNAPSHOT_SAY(("DX9 - apply ib change"));
		if (render_state.index_buffer) {
			switch (render_state.index_buffer_type) {//->Type()) {
			case BUFFER_TYPE_RENDER:
			case BUFFER_TYPE_DYNAMIC_RENDER:
				Set_Index_Buffer(render_state.index_buffer->Get_Backend_Buffer());
				break;
			case BUFFER_TYPE_SORTING:
			case BUFFER_TYPE_DYNAMIC_SORTING:
				break;
			default:
				WWASSERT(0);
			}
		}
		else {
			DX9CALL(SetIndices(
				nullptr));
			DX9_RECORD_INDEX_BUFFER_CHANGE();
		}
	}

	render_state_changed&=((unsigned)WORLD_IDENTITY|(unsigned)VIEW_IDENTITY);

	SNAPSHOT_SAY(("DX9Backend::Apply_Render_State_Changes() - finished"));
}

IDirect3DTexture9 * DX9Backend::_Create_DX9_Texture
(
	unsigned int width,
	unsigned int height,
	WW3DFormat format,
	MipCountType mip_level_count,
	D3DPOOL pool,
	bool rendertarget
)
{
	DX9_THREAD_ASSERT();
	DX9_Assert();
	IDirect3DTexture9 *texture = nullptr;

	// Paletted textures not supported!
	WWASSERT(format!=D3DFMT_P8);

	// NOTE: If 'format' is not supported as a texture format, this function will find the closest
	// format that is supported and use that instead.

	// Render target may return NOTAVAILABLE, in
	// which case we return null.
	if (rendertarget) {
		unsigned ret=D3DXCreateTexture(
			DX9Backend::_Get_D3D_Device(),
			width,
			height,
			mip_level_count,
			D3DUSAGE_RENDERTARGET,
			DX9_Format_From_WW3D(format),
			pool,
			&texture);

		if (ret==D3DERR_NOTAVAILABLE) {
			Non_Fatal_Log_DX9_ErrorCode(ret,__FILE__,__LINE__);
			return nullptr;
		}

		// If ran out of texture ram, try invalidating some textures and mesh cache.
		if (Is_DX9_Out_Of_Memory(ret)) {
			WWDEBUG_SAY(("Error: Out of memory while creating render target. Trying to release assets..."));
			// Free all textures that haven't been used in the last 5 seconds
			TextureClass::Invalidate_Old_Unused_Textures(5000);

			// Invalidate the mesh cache
			WW3D::_Invalidate_Mesh_Cache();

			ret=D3DXCreateTexture(
				DX9Backend::_Get_D3D_Device(),
				width,
				height,
				mip_level_count,
				D3DUSAGE_RENDERTARGET,
				DX9_Format_From_WW3D(format),
				pool,
				&texture);

			if (SUCCEEDED(ret)) {
				WWDEBUG_SAY(("...Render target creation successful."));
			}
			else {
				WWDEBUG_SAY(("...Render target creation failed."));
			}
			if (Is_DX9_Out_Of_Memory(ret)) {
				Non_Fatal_Log_DX9_ErrorCode(ret,__FILE__,__LINE__);
				return nullptr;
			}
		}

		DX9_ErrorCode(ret);
		if (ret != D3D_OK || texture == nullptr)
		{
			return nullptr;
		}
		// Just return the texture, no reduction
		// allowed for render targets.
		return texture;
	}

	// We should never run out of video memory when allocating a non-rendertarget texture.
	// However, it seems to happen sometimes when there are a lot of textures in memory and so
	// if it happens we'll release assets and try again (anything is better than crashing).
	unsigned ret=D3DXCreateTexture(
		DX9Backend::_Get_D3D_Device(),
		width,
		height,
		mip_level_count,
		0,
		DX9_Format_From_WW3D(format),
		pool,
		&texture);

	// If ran out of texture ram, try invalidating some textures and mesh cache.
	if (Is_DX9_Out_Of_Memory(ret)) {
		WWDEBUG_SAY(("Error: Out of memory while creating texture. Trying to release assets..."));
		// Free all textures that haven't been used in the last 5 seconds
		TextureClass::Invalidate_Old_Unused_Textures(5000);

		// Invalidate the mesh cache
		WW3D::_Invalidate_Mesh_Cache();

		ret=D3DXCreateTexture(
			DX9Backend::_Get_D3D_Device(),
			width,
			height,
			mip_level_count,
			0,
			DX9_Format_From_WW3D(format),
			pool,
			&texture);
		if (SUCCEEDED(ret)) {
			WWDEBUG_SAY(("...Texture creation successful."));
		}
		else {
			StringClass format_name(0,true);
			Get_WW3D_Format_Name(format, format_name);
			WWDEBUG_SAY(("...Texture creation failed. (%d x %d, format: %s, mips: %d",width,height,format_name.str(),mip_level_count));
		}

	}
	DX9_ErrorCode(ret);
	if (ret != D3D_OK || texture == nullptr)
	{
		return nullptr;
	}

	return texture;
}

IDirect3DTexture9 * DX9Backend::_Create_DX9_Texture
(
	const char *filename,
	MipCountType mip_level_count
)
{
	DX9_THREAD_ASSERT();
	DX9_Assert();
	IDirect3DTexture9 *texture = nullptr;

	// NOTE: If the original image format is not supported as a texture format, it will
	// automatically be converted to an appropriate format.
	// NOTE: It is possible to get the size and format of the original image file from this
	// function as well, so if we later want to second-guess D3DX's format conversion decisions
	// we can do so after this function is called..
	unsigned result = D3DXCreateTextureFromFileExA(
		_Get_D3D_Device(),
		filename,
		D3DX_DEFAULT,
		D3DX_DEFAULT,
		mip_level_count,//create_mipmaps ? 0 : 1,
		0,
		D3DFMT_UNKNOWN,
		D3DPOOL_MANAGED,
		D3DX_FILTER_BOX,
		D3DX_FILTER_BOX,
		0,
		nullptr,
		nullptr,
		&texture);

	if (result != D3D_OK) {
		return reinterpret_cast<IDirect3DTexture9 *>(MissingTexture::_Get_Missing_Texture());
	}

	// Make sure texture wasn't paletted!
	D3DSURFACE_DESC desc;
	texture->GetLevelDesc(0,&desc);
	if (desc.Format==D3DFMT_P8) {
		texture->Release();
		return reinterpret_cast<IDirect3DTexture9 *>(MissingTexture::_Get_Missing_Texture());
	}
	return texture;
}

IDirect3DTexture9 * DX9Backend::_Create_DX9_Texture
(
	IDirect3DSurface9 *surface,
	MipCountType mip_level_count
)
{
	DX9_THREAD_ASSERT();
	DX9_Assert();
	IDirect3DTexture9 *texture = nullptr;
	if (surface == nullptr)
	{
		return nullptr;
	}

	D3DSURFACE_DESC surface_desc;
	::ZeroMemory(&surface_desc, sizeof(D3DSURFACE_DESC));
	if (surface->GetDesc(&surface_desc) != D3D_OK)
	{
		return nullptr;
	}

	// This function will create a texture with a different (but similar) format if the surface is
	// not in a supported texture format.
	WW3DFormat format=WW3D_Format_From_DX9(surface_desc.Format);
	texture = _Create_DX9_Texture(surface_desc.Width, surface_desc.Height, format, mip_level_count);

	// Copy the surface to the texture
	IDirect3DSurface9 *tex_surface = nullptr;
	if (texture == nullptr || texture->GetSurfaceLevel(0, &tex_surface) != D3D_OK || tex_surface == nullptr)
	{
		if (texture != nullptr)
		{
			texture->Release();
		}
		return nullptr;
	}

	HRESULT copy_result = D3DXLoadSurfaceFromSurface(
		tex_surface, nullptr, nullptr, surface, nullptr, nullptr, D3DX_FILTER_BOX, 0);
	if (copy_result != D3D_OK)
	{
		DX9_ErrorCode(copy_result);
		tex_surface->Release();
		texture->Release();
		return nullptr;
	}
	tex_surface->Release();

	// Create mipmaps if needed
	if (mip_level_count!=MIP_LEVELS_1)
	{
		HRESULT filter_result = D3DXFilterTexture(texture, nullptr, 0, D3DX_FILTER_BOX);
		if (filter_result != D3D_OK)
		{
			DX9_ErrorCode(filter_result);
			texture->Release();
			return nullptr;
		}
	}

	return texture;

}

/*!
 * KJM create depth stencil texture
 */
IDirect3DTexture9 * DX9Backend::_Create_DX9_ZTexture
(
	unsigned int width,
	unsigned int height,
	WW3DZFormat zformat,
	MipCountType mip_level_count,
	D3DPOOL pool
)
{
	DX9_THREAD_ASSERT();
	DX9_Assert();
	IDirect3DTexture9* texture = nullptr;

	D3DFORMAT zfmt=DX9_ZFormat_From_WW3D(zformat);

	unsigned ret=DX9Backend::_Get_D3D_Device()->CreateTexture
	(
		width,
		height,
		mip_level_count,
		D3DUSAGE_DEPTHSTENCIL,
		zfmt,
		pool,
		&texture,
		nullptr
	);

	if (ret==D3DERR_NOTAVAILABLE)
	{
		Non_Fatal_Log_DX9_ErrorCode(ret,__FILE__,__LINE__);
		return nullptr;
	}

	// If ran out of texture ram, try invalidating some textures and mesh cache.
	if (Is_DX9_Out_Of_Memory(ret))
	{
		WWDEBUG_SAY(("Error: Out of memory while creating render target. Trying to release assets..."));
		// Free all textures that haven't been used in the last 5 seconds
		TextureClass::Invalidate_Old_Unused_Textures(5000);

		// Invalidate the mesh cache
		WW3D::_Invalidate_Mesh_Cache();

		ret=DX9Backend::_Get_D3D_Device()->CreateTexture
		(
			width,
			height,
			mip_level_count,
			D3DUSAGE_DEPTHSTENCIL,
			zfmt,
			pool,
			&texture,
			nullptr
		);

		if (SUCCEEDED(ret))
		{
			WWDEBUG_SAY(("...Render target creation successful."));
		}
		else
		{
			WWDEBUG_SAY(("...Render target creation failed."));
		}
		if (Is_DX9_Out_Of_Memory(ret))
		{
			Non_Fatal_Log_DX9_ErrorCode(ret,__FILE__,__LINE__);
			return nullptr;
		}
	}

	DX9_ErrorCode(ret);
	if (ret != D3D_OK || texture == nullptr)
	{
		return nullptr;
	}

	texture->AddRef(); // don't release this texture

	// Just return the texture, no reduction
	// allowed for render targets.

	return texture;
}

/*!
 * KJM create cube map texture
 */
IDirect3DCubeTexture9* DX9Backend::_Create_DX9_Cube_Texture
(
	unsigned int width,
	unsigned int height,
	WW3DFormat format,
	MipCountType mip_level_count,
	D3DPOOL pool,
	bool rendertarget
)
{
	WWASSERT(width==height);
	DX9_THREAD_ASSERT();
	DX9_Assert();
	IDirect3DCubeTexture9* texture=nullptr;

	// Paletted textures not supported!
	WWASSERT(format!=D3DFMT_P8);

	// NOTE: If 'format' is not supported as a texture format, this function will find the closest
	// format that is supported and use that instead.

	// Render target may return NOTAVAILABLE, in
	// which case we return null.
	if (rendertarget)
	{
		unsigned ret=D3DXCreateCubeTexture
		(
			DX9Backend::_Get_D3D_Device(),
			width,
			mip_level_count,
			D3DUSAGE_RENDERTARGET,
			DX9_Format_From_WW3D(format),
			pool,
			&texture
		);

		if (ret==D3DERR_NOTAVAILABLE)
		{
			Non_Fatal_Log_DX9_ErrorCode(ret,__FILE__,__LINE__);
			return nullptr;
		}

		// If ran out of texture ram, try invalidating some textures and mesh cache.
		if (Is_DX9_Out_Of_Memory(ret))
		{
			WWDEBUG_SAY(("Error: Out of memory while creating render target. Trying to release assets..."));
			// Free all textures that haven't been used in the last 5 seconds
			TextureClass::Invalidate_Old_Unused_Textures(5000);

			// Invalidate the mesh cache
			WW3D::_Invalidate_Mesh_Cache();

			ret=D3DXCreateCubeTexture
			(
				DX9Backend::_Get_D3D_Device(),
				width,
				mip_level_count,
				D3DUSAGE_RENDERTARGET,
				DX9_Format_From_WW3D(format),
				pool,
				&texture
			);

			if (SUCCEEDED(ret))
			{
				WWDEBUG_SAY(("...Render target creation successful."));
			}
			else
			{
				WWDEBUG_SAY(("...Render target creation failed."));
			}
			if (Is_DX9_Out_Of_Memory(ret))
			{
				Non_Fatal_Log_DX9_ErrorCode(ret,__FILE__,__LINE__);
				return nullptr;
			}
		}

		DX9_ErrorCode(ret);
		if (ret != D3D_OK || texture == nullptr)
		{
			return nullptr;
		}
		// Just return the texture, no reduction
		// allowed for render targets.
		return texture;
	}

	// We should never run out of video memory when allocating a non-rendertarget texture.
	// However, it seems to happen sometimes when there are a lot of textures in memory and so
	// if it happens we'll release assets and try again (anything is better than crashing).
	unsigned ret=D3DXCreateCubeTexture
	(
		DX9Backend::_Get_D3D_Device(),
		width,
		mip_level_count,
		0,
		DX9_Format_From_WW3D(format),
		pool,
		&texture
	);

	// If ran out of texture ram, try invalidating some textures and mesh cache.
	if (Is_DX9_Out_Of_Memory(ret))
	{
		WWDEBUG_SAY(("Error: Out of memory while creating texture. Trying to release assets..."));
		// Free all textures that haven't been used in the last 5 seconds
		TextureClass::Invalidate_Old_Unused_Textures(5000);

		// Invalidate the mesh cache
		WW3D::_Invalidate_Mesh_Cache();

		ret=D3DXCreateCubeTexture
		(
			DX9Backend::_Get_D3D_Device(),
			width,
			mip_level_count,
			0,
			DX9_Format_From_WW3D(format),
			pool,
			&texture
		);
		if (SUCCEEDED(ret))
		{
			WWDEBUG_SAY(("...Texture creation successful."));
		}
		else
		{
			StringClass format_name(0,true);
			Get_WW3D_Format_Name(format, format_name);
			WWDEBUG_SAY(("...Texture creation failed. (%d x %d, format: %s, mips: %d",width,height,format_name.str(),mip_level_count));
		}

	}
	DX9_ErrorCode(ret);
	if (ret != D3D_OK || texture == nullptr)
	{
		return nullptr;
	}

	return texture;
}

/*!
 * KJM create volume texture
 */
IDirect3DVolumeTexture9* DX9Backend::_Create_DX9_Volume_Texture
(
	unsigned int width,
	unsigned int height,
	unsigned int depth,
	WW3DFormat format,
	MipCountType mip_level_count,
	D3DPOOL pool
)
{
	DX9_THREAD_ASSERT();
	DX9_Assert();
	IDirect3DVolumeTexture9* texture=nullptr;

	// Paletted textures not supported!
	WWASSERT(format!=D3DFMT_P8);

	// NOTE: If 'format' is not supported as a texture format, this function will find the closest
	// format that is supported and use that instead.


	// We should never run out of video memory when allocating a non-rendertarget texture.
	// However, it seems to happen sometimes when there are a lot of textures in memory and so
	// if it happens we'll release assets and try again (anything is better than crashing).
	unsigned ret=D3DXCreateVolumeTexture
	(
		DX9Backend::_Get_D3D_Device(),
		width,
		height,
		depth,
		mip_level_count,
		0,
		DX9_Format_From_WW3D(format),
		pool,
		&texture
	);

	// If ran out of texture ram, try invalidating some textures and mesh cache.
	if (Is_DX9_Out_Of_Memory(ret))
	{
		WWDEBUG_SAY(("Error: Out of memory while creating texture. Trying to release assets..."));
		// Free all textures that haven't been used in the last 5 seconds
		TextureClass::Invalidate_Old_Unused_Textures(5000);

		// Invalidate the mesh cache
		WW3D::_Invalidate_Mesh_Cache();

		ret=D3DXCreateVolumeTexture
		(
			DX9Backend::_Get_D3D_Device(),
			width,
			height,
			depth,
			mip_level_count,
			0,
			DX9_Format_From_WW3D(format),
			pool,
			&texture
		);
		if (SUCCEEDED(ret))
		{
			WWDEBUG_SAY(("...Texture creation successful."));
		}
		else
		{
			StringClass format_name(0,true);
			Get_WW3D_Format_Name(format, format_name);
			WWDEBUG_SAY(("...Texture creation failed. (%d x %d, format: %s, mips: %d",width,height,format_name.str(),mip_level_count));
		}

	}
	DX9_ErrorCode(ret);
	if (ret != D3D_OK || texture == nullptr)
	{
		return nullptr;
	}

	return texture;
}


IDirect3DSurface9 * DX9Backend::_Create_DX9_Surface(unsigned int width, unsigned int height, WW3DFormat format)
{
	DX9_THREAD_ASSERT();
	DX9_Assert();

	IDirect3DSurface9 *surface = nullptr;

	// Paletted surfaces not supported!
	WWASSERT(format!=D3DFMT_P8);

	HRESULT create_surface_hr = D3DDevice->CreateOffscreenPlainSurface(
		width,
		height,
		DX9_Format_From_WW3D(format),
		D3DPOOL_SYSTEMMEM,
		&surface,
		nullptr);
	DX9_ErrorCode(create_surface_hr);

	return surface;
}

IDirect3DSurface9 * DX9Backend::_Create_DX9_Surface(const char *filename_)
{
	DX9_THREAD_ASSERT();
	DX9_Assert();

	// Note: Since there is no "D3DXCreateSurfaceFromFile" and no "GetSurfaceInfoFromFile" (the
	// latter is supposed to be added to D3DX in a future version), we create a texture from the
	// file (w/o mipmaps), check that its surface is equal to the original file data (which it
	// will not be if the file is not in a texture-supported format or size). If so, copy its
	// surface (we might be able to just get its surface and add a ref to it but I'm not sure so
	// I'm not going to risk it) and release the texture. If not, create a surface according to
	// the file data and use D3DXLoadSurfaceFromFile. This is a horrible hack, but it saves us
	// having to write file loaders. Will fix this when D3DX provides us with the right functions.
	// Create a surface the size of the file image data
	IDirect3DSurface9 *surface = nullptr;

	{

		file_auto_ptr myfile(_TheFileFactory,filename_);
		// If file not found, create a surface with missing texture in it

		if (!myfile->Is_Available()) {
			// If file not found, try the dds format
			// else create a surface with missing texture in it
			char compressed_name[200];
			strlcpy(compressed_name,filename_, sizeof(compressed_name));
			char *ext = strstr(compressed_name, ".");
			if ( ext && (strlen(ext)==4) &&
				  ( (ext[1] == 't') || (ext[1] == 'T') ) &&
				  ( (ext[2] == 'g') || (ext[2] == 'G') ) &&
				  ( (ext[3] == 'a') || (ext[3] == 'A') ) ) {
				ext[1]='d';
				ext[2]='d';
				ext[3]='s';
			}
			file_auto_ptr myfile2(_TheFileFactory,compressed_name);
			if (!myfile2->Is_Available())
			{
				RenderBackendSurface *missing_surface = MissingTexture::_Create_Missing_Surface();
				IDirect3DSurface9 *native_surface = Get_DX9_Surface(missing_surface);
				if (native_surface != nullptr)
				{
					native_surface->AddRef();
				}
				if (missing_surface != nullptr)
				{
					WW3D::Get_Render_Backend()->Release_Surface(missing_surface);
				}
				return native_surface;
			}
		}
	}

	StringClass filename_string(filename_,true);
	RenderBackendSurface *loaded_surface = TextureLoader::Load_Surface_Immediate(
		filename_string,
		WW3D_FORMAT_UNKNOWN,
		true);
	surface = Get_DX9_Surface(loaded_surface);
	if (surface != nullptr)
	{
		surface->AddRef();
	}
	if (loaded_surface != nullptr)
	{
		WW3D::Get_Render_Backend()->Release_Surface(loaded_surface);
	}
	return surface;
}


/***********************************************************************************************
 * DX9Backend::_Update_Texture -- Copies a texture from system memory to video memory          *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   4/26/2001  hy : Created.                                                                  *
 *=============================================================================================*/
void DX9Backend::_Update_Texture(TextureClass *system, TextureClass *video)
{
	WWASSERT(system);
	WWASSERT(video);
	WWASSERT(system->Get_Pool()==TextureClass::POOL_SYSTEMMEM);
	WWASSERT(video->Get_Pool()==TextureClass::POOL_DEFAULT);
	DX9CALL(UpdateTexture(
		reinterpret_cast<IDirect3DBaseTexture9 *>(system->Peek_Render_Backend_Texture()),
		reinterpret_cast<IDirect3DBaseTexture9 *>(video->Peek_Render_Backend_Texture())));
}

void DX9Backend::Compute_Caps(WW3DFormat display_format)
{
	DX9_THREAD_ASSERT();
	DX9_Assert();
	delete CurrentCaps;
	CurrentCaps=new DX9Caps(_Get_D3D9(),D3DDevice,display_format,Get_Current_Adapter_Identifier());
}


void DX9Backend::Set_Light(unsigned index, const D3DLIGHT9* light)
{
	if (light) {
		render_state.Lights[index]=To_Render_Backend_Light(*light);
		render_state.LightEnable[index]=true;
	}
	else {
		render_state.LightEnable[index]=false;
	}
	render_state_changed|=(LIGHT0_CHANGED<<index);
}

void DX9Backend::Set_Light(unsigned index,const LightClass &light)
{
	D3DLIGHT9 dlight;
	Vector3 temp;
	memset(&dlight,0,sizeof(D3DLIGHT9));

	switch (light.Get_Type())
	{
	case LightClass::POINT:
		{
			dlight.Type=D3DLIGHT_POINT;
		}
		break;
	case LightClass::DIRECTIONAL:
		{
			dlight.Type=D3DLIGHT_DIRECTIONAL;
		}
		break;
	case LightClass::SPOT:
		{
			dlight.Type=D3DLIGHT_SPOT;
		}
		break;
	}

	light.Get_Diffuse(&temp);
	temp*=light.Get_Intensity();
	dlight.Diffuse.r=temp.X;
	dlight.Diffuse.g=temp.Y;
	dlight.Diffuse.b=temp.Z;
	dlight.Diffuse.a=1.0f;

	light.Get_Specular(&temp);
	temp*=light.Get_Intensity();
	dlight.Specular.r=temp.X;
	dlight.Specular.g=temp.Y;
	dlight.Specular.b=temp.Z;
	dlight.Specular.a=1.0f;

	light.Get_Ambient(&temp);
	temp*=light.Get_Intensity();
	dlight.Ambient.r=temp.X;
	dlight.Ambient.g=temp.Y;
	dlight.Ambient.b=temp.Z;
	dlight.Ambient.a=1.0f;

	temp=light.Get_Position();
	dlight.Position=*(D3DVECTOR*) &temp;

	light.Get_Spot_Direction(temp);
	dlight.Direction=*(D3DVECTOR*) &temp;

	dlight.Range=light.Get_Attenuation_Range();
	dlight.Falloff=light.Get_Spot_Exponent();
	dlight.Theta=light.Get_Spot_Angle();
	dlight.Phi=light.Get_Spot_Angle();

	// Inverse linear light 1/(1+D)
	double a,b;
	light.Get_Far_Attenuation_Range(a,b);
	dlight.Attenuation0=1.0f;
	if (fabs(a-b)<1e-5)
		// if the attenuation range is too small assume uniform with cutoff
		dlight.Attenuation1=0.0f;
	else
		// this will cause the light to drop to half intensity at the first far attenuation
		dlight.Attenuation1=(float) 1.0/a;
	dlight.Attenuation2=0.0f;

	Set_Light(index,&dlight);
}

//**********************************************************************************************
//! Set the light environment. This is a lighting model which used up to four
//! directional lights to produce the lighting.
/*! 5/27/02 KJM Added shader light environment support
*/
void DX9Backend::Set_Light_Environment(LightEnvironmentClass* light_env)
{
	// Shader light environment support															*
	Light_Environment=light_env;

	if (light_env)
	{
		int light_count = light_env->Get_Light_Count();
		unsigned int color=Convert_Color(light_env->Get_Equivalent_Ambient(),0.0f);
		if (RenderStates[D3DRS_AMBIENT]!=color)
		{
			Set_DX9_Render_State(D3DRS_AMBIENT,color);
//buggy Radeon 9700 driver doesn't apply new ambient unless the material also changes.
#if 1
			render_state_changed|=MATERIAL_CHANGED;
#endif
		}

		D3DLIGHT9 light;
		int l=0;
		for (;l<light_count;++l) {

			::ZeroMemory(&light, sizeof(D3DLIGHT9));

			light.Type=D3DLIGHT_DIRECTIONAL;
			(Vector3&)light.Diffuse=light_env->Get_Light_Diffuse(l);
			Vector3 dir=-light_env->Get_Light_Direction(l);
			light.Direction=(const D3DVECTOR&)(dir);

			// (gth) TODO: put specular into LightEnvironment?  Much work to be done on lights :-)'
			if (l==0) {
				light.Specular.r = light.Specular.g = light.Specular.b = 1.0f;
			}

			if (light_env->isPointLight(l)) {
				light.Type = D3DLIGHT_POINT;
				(Vector3&)light.Diffuse=light_env->getPointDiffuse(l);
				(Vector3&)light.Ambient=light_env->getPointAmbient(l);
				light.Position = (const D3DVECTOR&)light_env->getPointCenter(l);
				light.Range = light_env->getPointOrad(l);

				// Inverse linear light 1/(1+D)
				double a,b;
				b = light_env->getPointOrad(l);
				a = light_env->getPointIrad(l);

//(gth) CNC3 Generals code for the attenuation factors is causing the lights to over-brighten
//I'm changing the Attenuation0 parameter to 1.0 to avoid this problem.
#if 0
				light.Attenuation0=0.01f;
#else
				light.Attenuation0=1.0f;
#endif
				if (fabs(a-b)<1e-5)
					// if the attenuation range is too small assume uniform with cutoff
					light.Attenuation1=0.0f;
				else
					// this will cause the light to drop to half intensity at the first far attenuation
					light.Attenuation1=(float) 0.1/a;

				light.Attenuation2=8.0f/(b*b);
			}

			Set_Light(l,&light);
		}

		for (;l<4;++l) {
			Set_Light(l,nullptr);
		}
	}
/*	else {
		for (int l=0;l<4;++l) {
			Set_Light(l,nullptr);
		}
	}
*/
}

IDirect3DSurface9 * DX9Backend::_Get_DX9_Front_Buffer()
{
	DX9_THREAD_ASSERT();
	D3DDISPLAYMODE mode;

	DX9CALL(GetDisplayMode(0, &mode));

	IDirect3DSurface9 * fb=nullptr;

	HRESULT front_buffer_hr = D3DDevice->CreateOffscreenPlainSurface(
		mode.Width,
		mode.Height,
		D3DFMT_A8R8G8B8,
		D3DPOOL_SYSTEMMEM,
		&fb,
		nullptr);
	DX9_ErrorCode(front_buffer_hr);
	DX9CALL(GetFrontBufferData(0, fb));
	return fb;
}

SurfaceClass * DX9Backend::_Get_DX9_Back_Buffer(unsigned int num)
{
	DX9_THREAD_ASSERT();

	IDirect3DSurface9 * bb = nullptr;
	SurfaceClass *surf=nullptr;
	HRESULT get_back_buffer_hr = D3D_OK;
	DX9CALL_HRES(GetBackBuffer(0, num, D3DBACKBUFFER_TYPE_MONO, &bb), get_back_buffer_hr);
	if (SUCCEEDED(get_back_buffer_hr) && bb != nullptr)
	{
		surf=Wrap_DX9_Surface(bb);
	}

	return surf;
}


TextureClass *
DX9Backend::Create_Render_Target (int width, int height, WW3DFormat format)
{
	DX9_THREAD_ASSERT();
	DX9_Assert();
	DX9_RECORD_DX9_CALLS();

	// Use the current display format if format isn't specified
	if (format==WW3D_FORMAT_UNKNOWN) {
		D3DDISPLAYMODE mode;
		DX9CALL(GetDisplayMode(0, &mode));
		format=WW3D_Format_From_DX9(mode.Format);
	}

	// If render target format isn't supported return null
	if (!Get_Current_Caps()->Support_Render_To_Texture_Format(format)) {
		WWDEBUG_SAY(("DX9Backend - Render target format is not supported"));
		return nullptr;
	}

	//
	//	Note: We're going to force the width and height to be powers of two and equal
	//
	const D3DCAPS9& dx9caps=Get_Current_Caps()->Get_DX9_Caps();
	float poweroftwosize = width;
	if (height > 0 && height < width) {
		poweroftwosize = height;
	}
	poweroftwosize = ::Find_POT (poweroftwosize);

	if (poweroftwosize>dx9caps.MaxTextureWidth) {
		poweroftwosize=dx9caps.MaxTextureWidth;
	}
	if (poweroftwosize>dx9caps.MaxTextureHeight) {
		poweroftwosize=dx9caps.MaxTextureHeight;
	}

	width = height = poweroftwosize;

	//
	//	Attempt to create the render target
	//
	TextureClass * tex = NEW_REF(TextureClass,(width,height,format,MIP_LEVELS_1,TextureClass::POOL_DEFAULT,true));

	// 3dfx drivers are lying in the CheckDeviceFormat call and claiming
	// that they support render targets!
	if (tex->Peek_Render_Backend_Texture() == 0)
	{
		WWDEBUG_SAY(("DX9Backend - Render target creation failed!"));
		REF_PTR_RELEASE(tex);
	}

	return tex;
}

//**********************************************************************************************
//! Create render target with associated depth stencil buffer
/*! KJM
*/
void DX9Backend::Create_Render_Target
(
	int width,
	int height,
	WW3DFormat format,
	WW3DZFormat zformat,
	TextureClass** target,
	ZTextureClass** depth_buffer
)
{
	DX9_THREAD_ASSERT();
	DX9_Assert();
	DX9_RECORD_DX9_CALLS();

	// Use the current display format if format isn't specified
	if (format==WW3D_FORMAT_UNKNOWN)
	{
		*target=nullptr;
		*depth_buffer=nullptr;
		return;
/*		D3DDISPLAYMODE mode;
		DX9CALL(GetDisplayMode(0, &mode));
		format=WW3D_Format_From_DX9(mode.Format);*/
	}

	// If render target format isn't supported return null
	if (!Get_Current_Caps()->Support_Render_To_Texture_Format(format) ||
		 !Get_Current_Caps()->Support_Depth_Stencil_Format(zformat))
	{
		WWDEBUG_SAY(("DX9Backend - Render target with depth format is not supported"));
		return;
	}

	//	Note: We're going to force the width and height to be powers of two and equal
	const D3DCAPS9& dx9caps=Get_Current_Caps()->Get_DX9_Caps();
	float poweroftwosize = width;
	if (height > 0 && height < width)
	{
		poweroftwosize = height;
	}
	poweroftwosize = ::Find_POT (poweroftwosize);

	if (poweroftwosize>dx9caps.MaxTextureWidth)
	{
		poweroftwosize=dx9caps.MaxTextureWidth;
	}

	if (poweroftwosize>dx9caps.MaxTextureHeight)
	{
		poweroftwosize=dx9caps.MaxTextureHeight;
	}

	width = height = poweroftwosize;

	//	Attempt to create the render target
	TextureClass* tex=NEW_REF(TextureClass,(width,height,format,MIP_LEVELS_1,TextureClass::POOL_DEFAULT,true));

	// 3dfx drivers are lying in the CheckDeviceFormat call and claiming
	// that they support render targets!
	if (tex->Peek_Render_Backend_Texture() == 0)
	{
		WWDEBUG_SAY(("DX9Backend - Render target creation failed!"));
		REF_PTR_RELEASE(tex);
	}

	*target=tex;

	// attempt to create the depth stencil buffer
	*depth_buffer=NEW_REF
	(
		ZTextureClass,
		(
			width,
			height,
			zformat,
			MIP_LEVELS_1,
			TextureClass::POOL_DEFAULT
		)
	);
}

/*!
 * Set render target
 * KM Added optional custom z target
 */
void DX9Backend::Set_Render_Target_With_Z
(
	TextureClass* texture,
	ZTextureClass* ztexture
)
{
	WWASSERT(texture!=nullptr);
	SurfaceClass *surface = texture->Get_Surface_Level();
	IDirect3DSurface9 * d3d_surf = Get_DX9_Surface(surface);
	WWASSERT(d3d_surf != nullptr);

	IDirect3DSurface9* d3d_zbuf=nullptr;
	SurfaceClass *depth_surface = nullptr;
	if (ztexture!=nullptr)
	{

		depth_surface = ztexture->Get_Surface_Level();
		d3d_zbuf=Get_DX9_Surface(depth_surface);
		WWASSERT(d3d_zbuf!=nullptr);
		Set_Render_Target(d3d_surf,d3d_zbuf);
	}
	else
	{
		Set_Render_Target(d3d_surf,true);
	}
	REF_PTR_RELEASE(depth_surface);
	REF_PTR_RELEASE(surface);

	IsRenderToTexture = true;
}

void
DX9Backend::Set_Render_Target(IDirect3DSwapChain9 *swap_chain)
{
	DX9_THREAD_ASSERT();
	WWASSERT (swap_chain != nullptr);

	//
	//	Get the back buffer for the swap chain
	//
	LPDIRECT3DSURFACE9 render_target = nullptr;
	swap_chain->GetBackBuffer (0, D3DBACKBUFFER_TYPE_MONO, &render_target);

	//
	//	Set this back buffer as the render target
	//
	Set_Render_Target (render_target, true);

	//
	//	Release our hold on the back buffer
	//
	if (render_target != nullptr) {
		render_target->Release ();
		render_target = nullptr;
	}

	IsRenderToTexture = false;
}

void
DX9Backend::Set_Render_Target(IDirect3DSurface9 *render_target, bool use_default_depth_buffer)
{
	DX9_THREAD_ASSERT();
	DX9_Assert();
	if (!Is_Valid_D3D_Object_Ptr(render_target, "Set_Render_Target(surface,bool):render_target")) {
		render_target = nullptr;
	}

	//
	//	Should we restore the default render target set a new one?
	//
	if (render_target == nullptr || render_target == DefaultRenderTarget)
	{
		// If there is currently a custom render target, default must NOT be null.
		if (CurrentRenderTarget)
		{
			WWASSERT(DefaultRenderTarget!=nullptr);
		}

		//
		//	Restore the default render target
		//
		if (DefaultRenderTarget != nullptr)
		{
			DX9CALL(SetRenderTarget(0, DefaultRenderTarget));
			DX9CALL(SetDepthStencilSurface(DefaultDepthBuffer));
			DefaultRenderTarget->Release ();
			DefaultRenderTarget = nullptr;
			if (DefaultDepthBuffer)
			{
				DefaultDepthBuffer->Release ();
				DefaultDepthBuffer = nullptr;
			}
		}

		//
		//	Release our hold on the "current" render target
		//
		if (CurrentRenderTarget != nullptr)
		{
			CurrentRenderTarget->Release ();
			CurrentRenderTarget = nullptr;
		}

		if (CurrentDepthBuffer!=nullptr)
		{
			CurrentDepthBuffer->Release();
			CurrentDepthBuffer=nullptr;
		}

	}
	else if (render_target != CurrentRenderTarget)
	{
		WWASSERT(DefaultRenderTarget==nullptr);

		//
		//	We'll need the depth buffer later...
		//
		if (DefaultDepthBuffer == nullptr)
		{
//		IDirect3DSurface9 *depth_buffer = nullptr;
			DX9CALL(GetDepthStencilSurface (&DefaultDepthBuffer));
		}

		//
		//	Get a pointer to the default render target (if necessary)
		//
		if (DefaultRenderTarget == nullptr)
		{
			DX9CALL(GetRenderTarget(0, &DefaultRenderTarget));
		}

		//
		//	Release our hold on the old "current" render target
		//
		if (CurrentRenderTarget != nullptr)
		{
			CurrentRenderTarget->Release ();
			CurrentRenderTarget = nullptr;
		}

		if (CurrentDepthBuffer!=nullptr)
		{
			CurrentDepthBuffer->Release();
			CurrentDepthBuffer=nullptr;
		}

		//
		//	Keep a copy of the current render target (for housekeeping)
		//
		CurrentRenderTarget = render_target;
		WWASSERT (CurrentRenderTarget != nullptr);
		if (CurrentRenderTarget != nullptr)
		{
			CurrentRenderTarget->AddRef ();

			//
			//	Switch render targets
			//
			if (use_default_depth_buffer)
			{
				DX9CALL(SetRenderTarget(0, CurrentRenderTarget));
				DX9CALL(SetDepthStencilSurface(DefaultDepthBuffer));
			}
			else
			{
				DX9CALL(SetRenderTarget(0, CurrentRenderTarget));
				DX9CALL(SetDepthStencilSurface(nullptr));
			}
		}
	}

	//
	//	Free our hold on the depth buffer
	//
//	if (depth_buffer != nullptr) {
//		depth_buffer->Release ();
//		depth_buffer = nullptr;
//	}

	IsRenderToTexture = false;
}


//**********************************************************************************************
//! Set render target with depth stencil buffer
/*! KJM
*/
void DX9Backend::Set_Render_Target
(
	IDirect3DSurface9* render_target,
	IDirect3DSurface9* depth_buffer
)
{
	DX9_THREAD_ASSERT();
	DX9_Assert();
	if (!Is_Valid_D3D_Object_Ptr(render_target, "Set_Render_Target(surface,surface):render_target")) {
		render_target = nullptr;
	}
	if (!Is_Valid_D3D_Object_Ptr(depth_buffer, "Set_Render_Target(surface,surface):depth_buffer")) {
		depth_buffer = nullptr;
	}

	//
	//	Should we restore the default render target set a new one?
	//
	if (render_target == nullptr || render_target == DefaultRenderTarget)
	{
		// If there is currently a custom render target, default must NOT be null.
		if (CurrentRenderTarget)
		{
			WWASSERT(DefaultRenderTarget!=nullptr);
		}

		//
		//	Restore the default render target
		//
		if (DefaultRenderTarget != nullptr)
		{
			DX9CALL(SetRenderTarget(0, DefaultRenderTarget));
			DX9CALL(SetDepthStencilSurface(DefaultDepthBuffer));
			DefaultRenderTarget->Release ();
			DefaultRenderTarget = nullptr;
			if (DefaultDepthBuffer)
			{
				DefaultDepthBuffer->Release ();
				DefaultDepthBuffer = nullptr;
			}
		}

		//
		//	Release our hold on the "current" render target
		//
		if (CurrentRenderTarget != nullptr)
		{
			CurrentRenderTarget->Release ();
			CurrentRenderTarget = nullptr;
		}

		if (CurrentDepthBuffer!=nullptr)
		{
			CurrentDepthBuffer->Release();
			CurrentDepthBuffer=nullptr;
		}
	}
	else if (render_target != CurrentRenderTarget)
	{
		WWASSERT(DefaultRenderTarget==nullptr);

		//
		//	We'll need the depth buffer later...
		//
		if (DefaultDepthBuffer == nullptr)
		{
//		IDirect3DSurface9 *depth_buffer = nullptr;
			DX9CALL(GetDepthStencilSurface (&DefaultDepthBuffer));
		}

		//
		//	Get a pointer to the default render target (if necessary)
		//
		if (DefaultRenderTarget == nullptr)
		{
			DX9CALL(GetRenderTarget(0, &DefaultRenderTarget));
		}

		//
		//	Release our hold on the old "current" render target
		//
		if (CurrentRenderTarget != nullptr)
		{
			CurrentRenderTarget->Release ();
			CurrentRenderTarget = nullptr;
		}

		if (CurrentDepthBuffer!=nullptr)
		{
			CurrentDepthBuffer->Release();
			CurrentDepthBuffer=nullptr;
		}

		//
		//	Keep a copy of the current render target (for housekeeping)
		//
		CurrentRenderTarget = render_target;
		CurrentDepthBuffer = depth_buffer;
		WWASSERT (CurrentRenderTarget != nullptr);
		if (CurrentRenderTarget != nullptr)
		{
			CurrentRenderTarget->AddRef ();
			if (CurrentDepthBuffer != nullptr) {
				CurrentDepthBuffer->AddRef();
			}

			//
			//	Switch render targets
			//
			DX9CALL(SetRenderTarget(0, CurrentRenderTarget));
			DX9CALL(SetDepthStencilSurface(CurrentDepthBuffer));
		}
	}

	IsRenderToTexture=true;
}


IDirect3DSwapChain9 *
DX9Backend::Create_Additional_Swap_Chain (HWND render_window)
{
	DX9_Assert();

	//
	//	Configure the presentation parameters for a windowed render target
	//
	D3DPRESENT_PARAMETERS params				= { 0 };
	params.BackBufferFormat						= _PresentParameters.BackBufferFormat;
	params.BackBufferCount						= 1;
	params.MultiSampleType						= D3DMULTISAMPLE_NONE;
	params.SwapEffect								= D3DSWAPEFFECT_DISCARD;
	params.hDeviceWindow							= render_window;
	params.Windowed								= TRUE;
	params.EnableAutoDepthStencil				= TRUE;
	params.AutoDepthStencilFormat				= _PresentParameters.AutoDepthStencilFormat;
	params.Flags									= 0;
	params.FullScreen_RefreshRateInHz		= D3DPRESENT_RATE_DEFAULT;
	params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	//
	//	Create the swap chain
	//
	IDirect3DSwapChain9 *swap_chain = nullptr;
	DX9CALL(CreateAdditionalSwapChain(&params, &swap_chain));
	return swap_chain;
}

void DX9Backend::Flush_DX9_Resource_Manager(unsigned int bytes)
{
	DX9_Assert();
	(void)bytes;
}

unsigned int DX9Backend::Get_Free_Texture_RAM()
{
	DX9_Assert();
	DX9_RECORD_DX9_CALLS();
	return DX9Backend::_Get_D3D_Device()->GetAvailableTextureMem();
}

// Converts a linear gamma ramp to one that is controlled by:
// Gamma - controls the curvature of the middle of the curve
// Bright - controls the minimum value of the curve
// Contrast - controls the difference between the maximum and the minimum of the curve
void DX9Backend::Set_Gamma(float gamma,float bright,float contrast,bool calibrate,bool uselimit)
{
	gamma=Bound(gamma,0.6f,6.0f);
	bright=Bound(bright,-0.5f,0.5f);
	contrast=Bound(contrast,0.5f,2.0f);
	float oo_gamma=1.0f/gamma;

	DX9_Assert();
	DX9_RECORD_DX9_CALLS();

	DWORD flag=(calibrate?D3DSGR_CALIBRATE:D3DSGR_NO_CALIBRATION);

	D3DGAMMARAMP ramp;
	float			 limit;

	// IML: I'm not really sure what the intent of the 'limit' variable is. It does not produce useful results for my purposes.
	if (uselimit) {
		limit=(contrast-1)/2*contrast;
	} else {
		limit = 0.0f;
	}

	// HY - arrived at this equation after much trial and error.
	for (int i=0; i<256; i++) {
		float in,out;
		in=i/256.0f;
		float x=in-limit;
		x=Bound(x,0.0f,1.0f);
		x=powf(x,oo_gamma);
		out=contrast*x+bright;
		out=Bound(out,0.0f,1.0f);
		ramp.red[i]=(WORD) (out*65535);
		ramp.green[i]=(WORD) (out*65535);
		ramp.blue[i]=(WORD) (out*65535);
	}

	if (Get_Current_Caps()->Support_Gamma())	{
		DX9Backend::_Get_D3D_Device()->SetGammaRamp(0, flag, &ramp);
	} else {
		HWND hwnd = GetDesktopWindow();
		HDC hdc = GetDC(hwnd);
		if (hdc)
		{
			SetDeviceGammaRamp (hdc, &ramp);
			ReleaseDC (hwnd, hdc);
		}
	}
}

void DX9Backend::Set_World_Identity()
{
	if (render_state_changed&(unsigned)WORLD_IDENTITY)
		return;
	render_state.world.Make_Identity();
	render_state_changed|=(unsigned)WORLD_CHANGED|(unsigned)WORLD_IDENTITY;
}

void DX9Backend::Set_View_Identity()
{
	if (render_state_changed&(unsigned)VIEW_IDENTITY)
		return;
	render_state.view.Make_Identity();
	render_state_changed|=(unsigned)VIEW_CHANGED|(unsigned)VIEW_IDENTITY;
}

//**********************************************************************************************
//! Resets render device to default state
/*!
*/
void DX9Backend::Apply_Default_State()
{
	SNAPSHOT_SAY(("DX9Backend::Apply_Default_State()"));

	// only set states used in game
	Set_DX9_Render_State(D3DRS_ZENABLE, TRUE);
//	Set_DX9_Render_State(D3DRS_FILLMODE, D3DFILL_SOLID);
	Set_DX9_Render_State(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
	//Set_DX9_Render_State(D3DRS_LINEPATTERN, 0);
	Set_DX9_Render_State(D3DRS_ZWRITEENABLE, TRUE);
	Set_DX9_Render_State(D3DRS_ALPHATESTENABLE, FALSE);
	//Set_DX9_Render_State(D3DRS_LASTPIXEL, FALSE);
	Set_DX9_Render_State(D3DRS_SRCBLEND, D3DBLEND_ONE);
	Set_DX9_Render_State(D3DRS_DESTBLEND, D3DBLEND_ZERO);
	Set_DX9_Render_State(D3DRS_CULLMODE, D3DCULL_CW);
	Set_DX9_Render_State(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	Set_DX9_Render_State(D3DRS_ALPHAREF, 0);
	Set_DX9_Render_State(D3DRS_ALPHAFUNC, D3DCMP_LESSEQUAL);
	Set_DX9_Render_State(D3DRS_DITHERENABLE, FALSE);
	Set_DX9_Render_State(D3DRS_ALPHABLENDENABLE, FALSE);
	Set_DX9_Render_State(D3DRS_FOGENABLE, FALSE);
	Set_DX9_Render_State(D3DRS_SPECULARENABLE, FALSE);
//	Set_DX9_Render_State(D3DRS_ZVISIBLE, FALSE);
//	Set_DX9_Render_State(D3DRS_FOGCOLOR, 0);
//	Set_DX9_Render_State(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
//	Set_DX9_Render_State(D3DRS_FOGSTART, 0);

//	Set_DX9_Render_State(D3DRS_FOGEND, WWMath::Float_As_Int(1.0f));
//	Set_DX9_Render_State(D3DRS_FOGDENSITY, WWMath::Float_As_Int(1.0f));

	//Set_DX9_Render_State(D3DRS_EDGEANTIALIAS, FALSE);
	Set_DX9_Render_State(D3DRS_ZBIAS, 0);
//	Set_DX9_Render_State(D3DRS_RANGEFOGENABLE, FALSE);
	Set_DX9_Render_State(D3DRS_STENCILENABLE, FALSE);
	Set_DX9_Render_State(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
	Set_DX9_Render_State(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
	Set_DX9_Render_State(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
	Set_DX9_Render_State(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
	Set_DX9_Render_State(D3DRS_STENCILREF, 0);
	Set_DX9_Render_State(D3DRS_STENCILMASK, 0xffffffff);
	Set_DX9_Render_State(D3DRS_STENCILWRITEMASK, 0xffffffff);
	Set_DX9_Render_State(D3DRS_TEXTUREFACTOR, 0);
/*	Set_DX9_Render_State(D3DRS_WRAP0, D3DWRAP_U| D3DWRAP_V);
	Set_DX9_Render_State(D3DRS_WRAP1, D3DWRAP_U| D3DWRAP_V);
	Set_DX9_Render_State(D3DRS_WRAP2, D3DWRAP_U| D3DWRAP_V);
	Set_DX9_Render_State(D3DRS_WRAP3, D3DWRAP_U| D3DWRAP_V);
	Set_DX9_Render_State(D3DRS_WRAP4, D3DWRAP_U| D3DWRAP_V);
	Set_DX9_Render_State(D3DRS_WRAP5, D3DWRAP_U| D3DWRAP_V);
	Set_DX9_Render_State(D3DRS_WRAP6, D3DWRAP_U| D3DWRAP_V);
	Set_DX9_Render_State(D3DRS_WRAP7, D3DWRAP_U| D3DWRAP_V);*/
	Set_DX9_Render_State(D3DRS_CLIPPING, TRUE);
	Set_DX9_Render_State(D3DRS_LIGHTING, FALSE);
	//Set_DX9_Render_State(D3DRS_AMBIENT, 0);
//	Set_DX9_Render_State(D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
	Set_DX9_Render_State(D3DRS_COLORVERTEX, TRUE);
/*	Set_DX9_Render_State(D3DRS_LOCALVIEWER, TRUE);
	Set_DX9_Render_State(D3DRS_NORMALIZENORMALS, FALSE);
	Set_DX9_Render_State(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
	Set_DX9_Render_State(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR2);
	Set_DX9_Render_State(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
	Set_DX9_Render_State(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
	Set_DX9_Render_State(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);*/
	//Set_DX9_Render_State(D3DRS_CLIPPLANEENABLE, 0);
	Set_DX9_Render_State(D3DRS_SOFTWAREVERTEXPROCESSING, FALSE);
	//Set_DX9_Render_State(D3DRS_POINTSIZE, 0x3f800000);
	//Set_DX9_Render_State(D3DRS_POINTSIZE_MIN, 0);
	//Set_DX9_Render_State(D3DRS_POINTSPRITEENABLE, FALSE);
	//Set_DX9_Render_State(D3DRS_POINTSCALEENABLE, FALSE);
	//Set_DX9_Render_State(D3DRS_POINTSCALE_A, 0);
	//Set_DX9_Render_State(D3DRS_POINTSCALE_B, 0);
	//Set_DX9_Render_State(D3DRS_POINTSCALE_C, 0);
	//Set_DX9_Render_State(D3DRS_MULTISAMPLEANTIALIAS, TRUE);
	//Set_DX9_Render_State(D3DRS_MULTISAMPLEMASK, 0xffffffff);
	//Set_DX9_Render_State(D3DRS_PATCHEDGESTYLE, D3DPATCHEDGE_DISCRETE);
	//Set_DX9_Render_State(D3DRS_PATCHSEGMENTS, 0x3f800000);
	//Set_DX9_Render_State(D3DRS_DEBUGMONITORTOKEN, D3DDMT_ENABLE);
	//Set_DX9_Render_State(D3DRS_POINTSIZE_MAX, Float_At_Int(64.0f));
	//Set_DX9_Render_State(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);
	Set_DX9_Render_State(D3DRS_COLORWRITEENABLE, 0x0000000f);
	//Set_DX9_Render_State(D3DRS_TWEENFACTOR, 0);
	Set_DX9_Render_State(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	//Set_DX9_Render_State(D3DRS_POSITIONORDER, D3DORDER_CUBIC);
	//Set_DX9_Render_State(D3DRS_NORMALORDER, D3DORDER_LINEAR);

	// disable TSS stages
	int i;
	for (i=0; i<CurrentCaps->Get_Max_Textures_Per_Pass(); i++)
	{
		Set_DX9_Texture_Stage_State(i, D3DTSS_COLOROP, D3DTOP_DISABLE);
		Set_DX9_Texture_Stage_State(i, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		Set_DX9_Texture_Stage_State(i, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

		Set_DX9_Texture_Stage_State(i, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
		Set_DX9_Texture_Stage_State(i, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		Set_DX9_Texture_Stage_State(i, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

		/*Set_DX9_Texture_Stage_State(i, D3DTSS_BUMPENVMAT00, 0);
		Set_DX9_Texture_Stage_State(i, D3DTSS_BUMPENVMAT01, 0);
		Set_DX9_Texture_Stage_State(i, D3DTSS_BUMPENVMAT10, 0);
		Set_DX9_Texture_Stage_State(i, D3DTSS_BUMPENVMAT11, 0);
		Set_DX9_Texture_Stage_State(i, D3DTSS_BUMPENVLSCALE, 0);
		Set_DX9_Texture_Stage_State(i, D3DTSS_BUMPENVLOFFSET, 0);*/

		Set_DX9_Texture_Stage_State(i, D3DTSS_TEXCOORDINDEX, i);


		Set_DX9_Texture_Stage_State(i, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
		Set_DX9_Texture_Stage_State(i, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
		Set_DX9_Texture_Stage_State(i, D3DTSS_BORDERCOLOR, 0);
//		Set_DX9_Texture_Stage_State(i, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
//		Set_DX9_Texture_Stage_State(i, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
//		Set_DX9_Texture_Stage_State(i, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
//		Set_DX9_Texture_Stage_State(i, D3DTSS_MIPMAPLODBIAS, 0);
//		Set_DX9_Texture_Stage_State(i, D3DTSS_MAXMIPLEVEL, 0);
//		Set_DX9_Texture_Stage_State(i, D3DTSS_MAXANISOTROPY, 1);
		//Set_DX9_Texture_Stage_State(i, D3DTSS_ADDRESSW, D3DTADDRESS_WRAP);
		//Set_DX9_Texture_Stage_State(i, D3DTSS_COLORARG0, D3DTA_CURRENT);
		//Set_DX9_Texture_Stage_State(i, D3DTSS_ALPHAARG0, D3DTA_CURRENT);
		//Set_DX9_Texture_Stage_State(i, D3DTSS_RESULTARG, D3DTA_CURRENT);

		Set_DX9_Texture_Stage_State(i, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		Set_Texture(i,nullptr);
	}

//	DX9Backend::Set_Material(nullptr);
	VertexMaterialClass::Apply_Null();

	for (unsigned index=0;index<4;++index) {
		SNAPSHOT_SAY(("Clearing light %d to null",index));
		Set_DX9_Light(index,nullptr);
	}

	// set up simple default TSS
	Vector4 vconst[MAX_VERTEX_SHADER_CONSTANTS];
	memset(vconst,0,sizeof(Vector4)*MAX_VERTEX_SHADER_CONSTANTS);
	Set_Vertex_Shader_Constant(0u, vconst, static_cast<unsigned>(MAX_VERTEX_SHADER_CONSTANTS));

	Vector4 pconst[MAX_PIXEL_SHADER_CONSTANTS];
	memset(pconst,0,sizeof(Vector4)*MAX_PIXEL_SHADER_CONSTANTS);
	Set_Pixel_Shader_Constant(0u, pconst, static_cast<unsigned>(MAX_PIXEL_SHADER_CONSTANTS));

	Set_Vertex_Shader(DX9_FVF_XYZNDUV2);
	Set_Pixel_Shader(0);

	ShaderClass::Invalidate();
}

const char* DX9Backend::Get_DX9_Render_State_Name(D3DRENDERSTATETYPE state)
{
	switch (state) {
	case D3DRS_ZENABLE                       : return "D3DRS_ZENABLE";
	case D3DRS_FILLMODE                      : return "D3DRS_FILLMODE";
	case D3DRS_SHADEMODE                     : return "D3DRS_SHADEMODE";
	case D3DRS_LINEPATTERN                   : return "D3DRS_LINEPATTERN";
	case D3DRS_ZWRITEENABLE                  : return "D3DRS_ZWRITEENABLE";
	case D3DRS_ALPHATESTENABLE               : return "D3DRS_ALPHATESTENABLE";
	case D3DRS_LASTPIXEL                     : return "D3DRS_LASTPIXEL";
	case D3DRS_SRCBLEND                      : return "D3DRS_SRCBLEND";
	case D3DRS_DESTBLEND                     : return "D3DRS_DESTBLEND";
	case D3DRS_CULLMODE                      : return "D3DRS_CULLMODE";
	case D3DRS_ZFUNC                         : return "D3DRS_ZFUNC";
	case D3DRS_ALPHAREF                      : return "D3DRS_ALPHAREF";
	case D3DRS_ALPHAFUNC                     : return "D3DRS_ALPHAFUNC";
	case D3DRS_DITHERENABLE                  : return "D3DRS_DITHERENABLE";
	case D3DRS_ALPHABLENDENABLE              : return "D3DRS_ALPHABLENDENABLE";
	case D3DRS_FOGENABLE                     : return "D3DRS_FOGENABLE";
	case D3DRS_SPECULARENABLE                : return "D3DRS_SPECULARENABLE";
	case D3DRS_ZVISIBLE                      : return "D3DRS_ZVISIBLE";
	case D3DRS_FOGCOLOR                      : return "D3DRS_FOGCOLOR";
	case D3DRS_FOGTABLEMODE                  : return "D3DRS_FOGTABLEMODE";
	case D3DRS_FOGSTART                      : return "D3DRS_FOGSTART";
	case D3DRS_FOGEND                        : return "D3DRS_FOGEND";
	case D3DRS_FOGDENSITY                    : return "D3DRS_FOGDENSITY";
	case D3DRS_EDGEANTIALIAS                 : return "D3DRS_EDGEANTIALIAS";
	case D3DRS_ZBIAS                         : return "D3DRS_ZBIAS";
	case D3DRS_RANGEFOGENABLE                : return "D3DRS_RANGEFOGENABLE";
	case D3DRS_STENCILENABLE                 : return "D3DRS_STENCILENABLE";
	case D3DRS_STENCILFAIL                   : return "D3DRS_STENCILFAIL";
	case D3DRS_STENCILZFAIL                  : return "D3DRS_STENCILZFAIL";
	case D3DRS_STENCILPASS                   : return "D3DRS_STENCILPASS";
	case D3DRS_STENCILFUNC                   : return "D3DRS_STENCILFUNC";
	case D3DRS_STENCILREF                    : return "D3DRS_STENCILREF";
	case D3DRS_STENCILMASK                   : return "D3DRS_STENCILMASK";
	case D3DRS_STENCILWRITEMASK              : return "D3DRS_STENCILWRITEMASK";
	case D3DRS_TEXTUREFACTOR                 : return "D3DRS_TEXTUREFACTOR";
	case D3DRS_WRAP0                         : return "D3DRS_WRAP0";
	case D3DRS_WRAP1                         : return "D3DRS_WRAP1";
	case D3DRS_WRAP2                         : return "D3DRS_WRAP2";
	case D3DRS_WRAP3                         : return "D3DRS_WRAP3";
	case D3DRS_WRAP4                         : return "D3DRS_WRAP4";
	case D3DRS_WRAP5                         : return "D3DRS_WRAP5";
	case D3DRS_WRAP6                         : return "D3DRS_WRAP6";
	case D3DRS_WRAP7                         : return "D3DRS_WRAP7";
	case D3DRS_CLIPPING                      : return "D3DRS_CLIPPING";
	case D3DRS_LIGHTING                      : return "D3DRS_LIGHTING";
	case D3DRS_AMBIENT                       : return "D3DRS_AMBIENT";
	case D3DRS_FOGVERTEXMODE                 : return "D3DRS_FOGVERTEXMODE";
	case D3DRS_COLORVERTEX                   : return "D3DRS_COLORVERTEX";
	case D3DRS_LOCALVIEWER                   : return "D3DRS_LOCALVIEWER";
	case D3DRS_NORMALIZENORMALS              : return "D3DRS_NORMALIZENORMALS";
	case D3DRS_DIFFUSEMATERIALSOURCE         : return "D3DRS_DIFFUSEMATERIALSOURCE";
	case D3DRS_SPECULARMATERIALSOURCE        : return "D3DRS_SPECULARMATERIALSOURCE";
	case D3DRS_AMBIENTMATERIALSOURCE         : return "D3DRS_AMBIENTMATERIALSOURCE";
	case D3DRS_EMISSIVEMATERIALSOURCE        : return "D3DRS_EMISSIVEMATERIALSOURCE";
	case D3DRS_VERTEXBLEND                   : return "D3DRS_VERTEXBLEND";
	case D3DRS_CLIPPLANEENABLE               : return "D3DRS_CLIPPLANEENABLE";
	case D3DRS_SOFTWAREVERTEXPROCESSING      : return "D3DRS_SOFTWAREVERTEXPROCESSING";
	case D3DRS_POINTSIZE                     : return "D3DRS_POINTSIZE";
	case D3DRS_POINTSIZE_MIN                 : return "D3DRS_POINTSIZE_MIN";
	case D3DRS_POINTSPRITEENABLE             : return "D3DRS_POINTSPRITEENABLE";
	case D3DRS_POINTSCALEENABLE              : return "D3DRS_POINTSCALEENABLE";
	case D3DRS_POINTSCALE_A                  : return "D3DRS_POINTSCALE_A";
	case D3DRS_POINTSCALE_B                  : return "D3DRS_POINTSCALE_B";
	case D3DRS_POINTSCALE_C                  : return "D3DRS_POINTSCALE_C";
	case D3DRS_MULTISAMPLEANTIALIAS          : return "D3DRS_MULTISAMPLEANTIALIAS";
	case D3DRS_MULTISAMPLEMASK               : return "D3DRS_MULTISAMPLEMASK";
	case D3DRS_PATCHEDGESTYLE                : return "D3DRS_PATCHEDGESTYLE";
	case D3DRS_PATCHSEGMENTS                 : return "D3DRS_PATCHSEGMENTS";
	case D3DRS_DEBUGMONITORTOKEN             : return "D3DRS_DEBUGMONITORTOKEN";
	case D3DRS_POINTSIZE_MAX                 : return "D3DRS_POINTSIZE_MAX";
	case D3DRS_INDEXEDVERTEXBLENDENABLE      : return "D3DRS_INDEXEDVERTEXBLENDENABLE";
	case D3DRS_COLORWRITEENABLE              : return "D3DRS_COLORWRITEENABLE";
	case D3DRS_TWEENFACTOR                   : return "D3DRS_TWEENFACTOR";
	case D3DRS_BLENDOP                       : return "D3DRS_BLENDOP";
//	case D3DRS_POSITIONORDER                 : return "D3DRS_POSITIONORDER";
//	case D3DRS_NORMALORDER                   : return "D3DRS_NORMALORDER";
	default											  : return "UNKNOWN";
	}
}

const char* DX9Backend::Get_DX9_Texture_Stage_State_Name(D3DTEXTURESTAGESTATETYPE state)
{
	switch (state) {
	case D3DTSS_COLOROP                   : return "D3DTSS_COLOROP";
	case D3DTSS_COLORARG1                 : return "D3DTSS_COLORARG1";
	case D3DTSS_COLORARG2                 : return "D3DTSS_COLORARG2";
	case D3DTSS_ALPHAOP                   : return "D3DTSS_ALPHAOP";
	case D3DTSS_ALPHAARG1                 : return "D3DTSS_ALPHAARG1";
	case D3DTSS_ALPHAARG2                 : return "D3DTSS_ALPHAARG2";
	case D3DTSS_BUMPENVMAT00              : return "D3DTSS_BUMPENVMAT00";
	case D3DTSS_BUMPENVMAT01              : return "D3DTSS_BUMPENVMAT01";
	case D3DTSS_BUMPENVMAT10              : return "D3DTSS_BUMPENVMAT10";
	case D3DTSS_BUMPENVMAT11              : return "D3DTSS_BUMPENVMAT11";
	case D3DTSS_TEXCOORDINDEX             : return "D3DTSS_TEXCOORDINDEX";
	case D3DTSS_ADDRESSU                  : return "D3DTSS_ADDRESSU";
	case D3DTSS_ADDRESSV                  : return "D3DTSS_ADDRESSV";
	case D3DTSS_BORDERCOLOR               : return "D3DTSS_BORDERCOLOR";
	case D3DTSS_MAGFILTER                 : return "D3DTSS_MAGFILTER";
	case D3DTSS_MINFILTER                 : return "D3DTSS_MINFILTER";
	case D3DTSS_MIPFILTER                 : return "D3DTSS_MIPFILTER";
	case D3DTSS_MIPMAPLODBIAS             : return "D3DTSS_MIPMAPLODBIAS";
	case D3DTSS_MAXMIPLEVEL               : return "D3DTSS_MAXMIPLEVEL";
	case D3DTSS_MAXANISOTROPY             : return "D3DTSS_MAXANISOTROPY";
	case D3DTSS_BUMPENVLSCALE             : return "D3DTSS_BUMPENVLSCALE";
	case D3DTSS_BUMPENVLOFFSET            : return "D3DTSS_BUMPENVLOFFSET";
	case D3DTSS_TEXTURETRANSFORMFLAGS     : return "D3DTSS_TEXTURETRANSFORMFLAGS";
	case D3DTSS_ADDRESSW                  : return "D3DTSS_ADDRESSW";
	case D3DTSS_COLORARG0                 : return "D3DTSS_COLORARG0";
	case D3DTSS_ALPHAARG0                 : return "D3DTSS_ALPHAARG0";
	case D3DTSS_RESULTARG                 : return "D3DTSS_RESULTARG";
	default										  : return "UNKNOWN";
	}
}

void DX9Backend::Get_DX9_Render_State_Value_Name(StringClass& name, D3DRENDERSTATETYPE state, unsigned value)
{
	switch (state) {
	case D3DRS_ZENABLE:
		name=Get_DX9_ZBuffer_Type_Name(value);
		break;

	case D3DRS_FILLMODE:
		name=Get_DX9_Fill_Mode_Name(value);
		break;

	case D3DRS_SHADEMODE:
		name=Get_DX9_Shade_Mode_Name(value);
		break;

	case D3DRS_LINEPATTERN:
	case D3DRS_FOGCOLOR:
	case D3DRS_ALPHAREF:
	case D3DRS_STENCILMASK:
	case D3DRS_STENCILWRITEMASK:
	case D3DRS_TEXTUREFACTOR:
	case D3DRS_AMBIENT:
	case D3DRS_CLIPPLANEENABLE:
	case D3DRS_MULTISAMPLEMASK:
		name.Format("0x%x",value);
		break;

	case D3DRS_ZWRITEENABLE:
	case D3DRS_ALPHATESTENABLE:
	case D3DRS_LASTPIXEL:
	case D3DRS_DITHERENABLE:
	case D3DRS_ALPHABLENDENABLE:
	case D3DRS_FOGENABLE:
	case D3DRS_SPECULARENABLE:
	case D3DRS_STENCILENABLE:
	case D3DRS_RANGEFOGENABLE:
	case D3DRS_EDGEANTIALIAS:
	case D3DRS_CLIPPING:
	case D3DRS_LIGHTING:
	case D3DRS_COLORVERTEX:
	case D3DRS_LOCALVIEWER:
	case D3DRS_NORMALIZENORMALS:
	case D3DRS_SOFTWAREVERTEXPROCESSING:
	case D3DRS_POINTSPRITEENABLE:
	case D3DRS_POINTSCALEENABLE:
	case D3DRS_MULTISAMPLEANTIALIAS:
	case D3DRS_INDEXEDVERTEXBLENDENABLE:
		name=value ? "TRUE" : "FALSE";
		break;

	case D3DRS_SRCBLEND:
	case D3DRS_DESTBLEND:
		name=Get_DX9_Blend_Name(value);
		break;

	case D3DRS_CULLMODE:
		name=Get_DX9_Cull_Mode_Name(value);
		break;

	case D3DRS_ZFUNC:
	case D3DRS_ALPHAFUNC:
	case D3DRS_STENCILFUNC:
		name=Get_DX9_Cmp_Func_Name(value);
		break;

	case D3DRS_ZVISIBLE:
		name="NOTSUPPORTED";
		break;

	case D3DRS_FOGTABLEMODE:
	case D3DRS_FOGVERTEXMODE:
		name=Get_DX9_Fog_Mode_Name(value);
		break;

	case D3DRS_FOGSTART:
	case D3DRS_FOGEND:
	case D3DRS_FOGDENSITY:
	case D3DRS_POINTSIZE:
	case D3DRS_POINTSIZE_MIN:
	case D3DRS_POINTSCALE_A:
	case D3DRS_POINTSCALE_B:
	case D3DRS_POINTSCALE_C:
	case D3DRS_PATCHSEGMENTS:
	case D3DRS_POINTSIZE_MAX:
	case D3DRS_TWEENFACTOR:
		name.Format("%f",*(float*)&value);
		break;

	case D3DRS_ZBIAS:
	case D3DRS_STENCILREF:
		name.Format("%d",value);
		break;

	case D3DRS_STENCILFAIL:
	case D3DRS_STENCILZFAIL:
	case D3DRS_STENCILPASS:
		name=Get_DX9_Stencil_Op_Name(value);
		break;

	case D3DRS_WRAP0:
	case D3DRS_WRAP1:
	case D3DRS_WRAP2:
	case D3DRS_WRAP3:
	case D3DRS_WRAP4:
	case D3DRS_WRAP5:
	case D3DRS_WRAP6:
	case D3DRS_WRAP7:
		name="0";
		if (value&D3DWRAP_U) name+="|D3DWRAP_U";
		if (value&D3DWRAP_V) name+="|D3DWRAP_V";
		if (value&D3DWRAP_W) name+="|D3DWRAP_W";
		break;

	case D3DRS_DIFFUSEMATERIALSOURCE:
	case D3DRS_SPECULARMATERIALSOURCE:
	case D3DRS_AMBIENTMATERIALSOURCE:
	case D3DRS_EMISSIVEMATERIALSOURCE:
		name=Get_DX9_Material_Source_Name(value);
		break;

	case D3DRS_VERTEXBLEND:
		name=Get_DX9_Vertex_Blend_Flag_Name(value);
		break;

	case D3DRS_PATCHEDGESTYLE:
		name=Get_DX9_Patch_Edge_Style_Name(value);
		break;

	case D3DRS_DEBUGMONITORTOKEN:
		name=Get_DX9_Debug_Monitor_Token_Name(value);
		break;

	case D3DRS_COLORWRITEENABLE:
		name="0";
		if (value&D3DCOLORWRITEENABLE_RED) name+="|D3DCOLORWRITEENABLE_RED";
		if (value&D3DCOLORWRITEENABLE_GREEN) name+="|D3DCOLORWRITEENABLE_GREEN";
		if (value&D3DCOLORWRITEENABLE_BLUE) name+="|D3DCOLORWRITEENABLE_BLUE";
		if (value&D3DCOLORWRITEENABLE_ALPHA) name+="|D3DCOLORWRITEENABLE_ALPHA";
		break;
	case D3DRS_BLENDOP:
		name=Get_DX9_Blend_Op_Name(value);
		break;
	default:
		name.Format("UNKNOWN (%d)",value);
		break;
	}
}

void DX9Backend::Get_DX9_Texture_Stage_State_Value_Name(StringClass& name, D3DTEXTURESTAGESTATETYPE state, unsigned value)
{
	switch (state) {
	case D3DTSS_COLOROP:
	case D3DTSS_ALPHAOP:
		name=Get_DX9_Texture_Op_Name(value);
		break;

	case D3DTSS_COLORARG0:
	case D3DTSS_COLORARG1:
	case D3DTSS_COLORARG2:
	case D3DTSS_ALPHAARG0:
	case D3DTSS_ALPHAARG1:
	case D3DTSS_ALPHAARG2:
	case D3DTSS_RESULTARG:
		name=Get_DX9_Texture_Arg_Name(value);
		break;

	case D3DTSS_ADDRESSU:
	case D3DTSS_ADDRESSV:
	case D3DTSS_ADDRESSW:
		name=Get_DX9_Texture_Address_Name(value);
		break;

	case D3DTSS_MAGFILTER:
	case D3DTSS_MINFILTER:
	case D3DTSS_MIPFILTER:
		name=Get_DX9_Texture_Filter_Name(value);
		break;

	case D3DTSS_TEXTURETRANSFORMFLAGS:
		name=Get_DX9_Texture_Transform_Flag_Name(value);
		break;

	// Floating point values
	case D3DTSS_MIPMAPLODBIAS:
	case D3DTSS_BUMPENVMAT00:
	case D3DTSS_BUMPENVMAT01:
	case D3DTSS_BUMPENVMAT10:
	case D3DTSS_BUMPENVMAT11:
	case D3DTSS_BUMPENVLSCALE:
	case D3DTSS_BUMPENVLOFFSET:
		name.Format("%f",*(float*)&value);
		break;

	case D3DTSS_TEXCOORDINDEX:
		if ((value&0xffff0000)==D3DTSS_TCI_CAMERASPACENORMAL) {
			name.Format("D3DTSS_TCI_CAMERASPACENORMAL|%d",value&0xffff);
		}
		else if ((value&0xffff0000)==D3DTSS_TCI_CAMERASPACEPOSITION) {
			name.Format("D3DTSS_TCI_CAMERASPACEPOSITION|%d",value&0xffff);
		}
		else if ((value&0xffff0000)==D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR) {
			name.Format("D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR|%d",value&0xffff);
		}
		else {
			name.Format("%d",value);
		}
		break;

	// Integer value
	case D3DTSS_MAXMIPLEVEL:
	case D3DTSS_MAXANISOTROPY:
		name.Format("%d",value);
		break;
	// Hex values
	case D3DTSS_BORDERCOLOR:
		name.Format("0x%x",value);
		break;

	default:
		name.Format("UNKNOWN (%d)",value);
		break;
	}
}

const char* DX9Backend::Get_DX9_Texture_Op_Name(unsigned value)
{
	switch (value) {
	case D3DTOP_DISABLE                      : return "D3DTOP_DISABLE";
	case D3DTOP_SELECTARG1                   : return "D3DTOP_SELECTARG1";
	case D3DTOP_SELECTARG2                   : return "D3DTOP_SELECTARG2";
	case D3DTOP_MODULATE                     : return "D3DTOP_MODULATE";
	case D3DTOP_MODULATE2X                   : return "D3DTOP_MODULATE2X";
	case D3DTOP_MODULATE4X                   : return "D3DTOP_MODULATE4X";
	case D3DTOP_ADD                          : return "D3DTOP_ADD";
	case D3DTOP_ADDSIGNED                    : return "D3DTOP_ADDSIGNED";
	case D3DTOP_ADDSIGNED2X                  : return "D3DTOP_ADDSIGNED2X";
	case D3DTOP_SUBTRACT                     : return "D3DTOP_SUBTRACT";
	case D3DTOP_ADDSMOOTH                    : return "D3DTOP_ADDSMOOTH";
	case D3DTOP_BLENDDIFFUSEALPHA            : return "D3DTOP_BLENDDIFFUSEALPHA";
	case D3DTOP_BLENDTEXTUREALPHA            : return "D3DTOP_BLENDTEXTUREALPHA";
	case D3DTOP_BLENDFACTORALPHA             : return "D3DTOP_BLENDFACTORALPHA";
	case D3DTOP_BLENDTEXTUREALPHAPM          : return "D3DTOP_BLENDTEXTUREALPHAPM";
	case D3DTOP_BLENDCURRENTALPHA            : return "D3DTOP_BLENDCURRENTALPHA";
	case D3DTOP_PREMODULATE                  : return "D3DTOP_PREMODULATE";
	case D3DTOP_MODULATEALPHA_ADDCOLOR       : return "D3DTOP_MODULATEALPHA_ADDCOLOR";
	case D3DTOP_MODULATECOLOR_ADDALPHA       : return "D3DTOP_MODULATECOLOR_ADDALPHA";
	case D3DTOP_MODULATEINVALPHA_ADDCOLOR    : return "D3DTOP_MODULATEINVALPHA_ADDCOLOR";
	case D3DTOP_MODULATEINVCOLOR_ADDALPHA    : return "D3DTOP_MODULATEINVCOLOR_ADDALPHA";
	case D3DTOP_BUMPENVMAP                   : return "D3DTOP_BUMPENVMAP";
	case D3DTOP_BUMPENVMAPLUMINANCE          : return "D3DTOP_BUMPENVMAPLUMINANCE";
	case D3DTOP_DOTPRODUCT3                  : return "D3DTOP_DOTPRODUCT3";
	case D3DTOP_MULTIPLYADD                  : return "D3DTOP_MULTIPLYADD";
	case D3DTOP_LERP                         : return "D3DTOP_LERP";
	default										     : return "UNKNOWN";
	}
}

const char* DX9Backend::Get_DX9_Texture_Arg_Name(unsigned value)
{
	switch (value) {
	case D3DTA_CURRENT			: return "D3DTA_CURRENT";
	case D3DTA_DIFFUSE			: return "D3DTA_DIFFUSE";
	case D3DTA_SELECTMASK		: return "D3DTA_SELECTMASK";
	case D3DTA_SPECULAR			: return "D3DTA_SPECULAR";
	case D3DTA_TEMP				: return "D3DTA_TEMP";
	case D3DTA_TEXTURE			: return "D3DTA_TEXTURE";
	case D3DTA_TFACTOR			: return "D3DTA_TFACTOR";
	case D3DTA_ALPHAREPLICATE	: return "D3DTA_ALPHAREPLICATE";
	case D3DTA_COMPLEMENT		: return "D3DTA_COMPLEMENT";
	default					      : return "UNKNOWN";
	}
}

const char* DX9Backend::Get_DX9_Texture_Filter_Name(unsigned value)
{
	switch (value) {
	case D3DTEXF_NONE				: return "D3DTEXF_NONE";
	case D3DTEXF_POINT			: return "D3DTEXF_POINT";
	case D3DTEXF_LINEAR			: return "D3DTEXF_LINEAR";
	case D3DTEXF_ANISOTROPIC	: return "D3DTEXF_ANISOTROPIC";
#ifdef D3DTEXF_FLATCUBIC
	case D3DTEXF_FLATCUBIC		: return "D3DTEXF_FLATCUBIC";
#endif
#ifdef D3DTEXF_GAUSSIANCUBIC
	case D3DTEXF_GAUSSIANCUBIC	: return "D3DTEXF_GAUSSIANCUBIC";
#endif
	default					      : return "UNKNOWN";
	}
}

const char* DX9Backend::Get_DX9_Texture_Address_Name(unsigned value)
{
	switch (value) {
	case D3DTADDRESS_WRAP		: return "D3DTADDRESS_WRAP";
	case D3DTADDRESS_MIRROR		: return "D3DTADDRESS_MIRROR";
	case D3DTADDRESS_CLAMP		: return "D3DTADDRESS_CLAMP";
	case D3DTADDRESS_BORDER		: return "D3DTADDRESS_BORDER";
	case D3DTADDRESS_MIRRORONCE: return "D3DTADDRESS_MIRRORONCE";
	default					      : return "UNKNOWN";
	}
}

const char* DX9Backend::Get_DX9_Texture_Transform_Flag_Name(unsigned value)
{
	switch (value) {
	case D3DTTFF_DISABLE			: return "D3DTTFF_DISABLE";
	case D3DTTFF_COUNT1			: return "D3DTTFF_COUNT1";
	case D3DTTFF_COUNT2			: return "D3DTTFF_COUNT2";
	case D3DTTFF_COUNT3			: return "D3DTTFF_COUNT3";
	case D3DTTFF_COUNT4			: return "D3DTTFF_COUNT4";
	case D3DTTFF_PROJECTED		: return "D3DTTFF_PROJECTED";
	default					      : return "UNKNOWN";
	}
}

const char* DX9Backend::Get_DX9_ZBuffer_Type_Name(unsigned value)
{
	switch (value) {
	case D3DZB_FALSE				: return "D3DZB_FALSE";
	case D3DZB_TRUE				: return "D3DZB_TRUE";
	case D3DZB_USEW				: return "D3DZB_USEW";
	default					      : return "UNKNOWN";
	}
}

const char* DX9Backend::Get_DX9_Fill_Mode_Name(unsigned value)
{
	switch (value) {
	case D3DFILL_POINT			: return "D3DFILL_POINT";
	case D3DFILL_WIREFRAME		: return "D3DFILL_WIREFRAME";
	case D3DFILL_SOLID			: return "D3DFILL_SOLID";
	default					      : return "UNKNOWN";
	}
}

const char* DX9Backend::Get_DX9_Shade_Mode_Name(unsigned value)
{
	switch (value) {
	case D3DSHADE_FLAT			: return "D3DSHADE_FLAT";
	case D3DSHADE_GOURAUD		: return "D3DSHADE_GOURAUD";
	case D3DSHADE_PHONG			: return "D3DSHADE_PHONG";
	default							: return "UNKNOWN";
	}
}

const char* DX9Backend::Get_DX9_Blend_Name(unsigned value)
{
	switch (value) {
	case D3DBLEND_ZERO                : return "D3DBLEND_ZERO";
	case D3DBLEND_ONE                 : return "D3DBLEND_ONE";
	case D3DBLEND_SRCCOLOR            : return "D3DBLEND_SRCCOLOR";
	case D3DBLEND_INVSRCCOLOR         : return "D3DBLEND_INVSRCCOLOR";
	case D3DBLEND_SRCALPHA            : return "D3DBLEND_SRCALPHA";
	case D3DBLEND_INVSRCALPHA         : return "D3DBLEND_INVSRCALPHA";
	case D3DBLEND_DESTALPHA           : return "D3DBLEND_DESTALPHA";
	case D3DBLEND_INVDESTALPHA        : return "D3DBLEND_INVDESTALPHA";
	case D3DBLEND_DESTCOLOR           : return "D3DBLEND_DESTCOLOR";
	case D3DBLEND_INVDESTCOLOR        : return "D3DBLEND_INVDESTCOLOR";
	case D3DBLEND_SRCALPHASAT         : return "D3DBLEND_SRCALPHASAT";
	case D3DBLEND_BOTHSRCALPHA        : return "D3DBLEND_BOTHSRCALPHA";
	case D3DBLEND_BOTHINVSRCALPHA     : return "D3DBLEND_BOTHINVSRCALPHA";
	default									 : return "UNKNOWN";
	}
}

const char* DX9Backend::Get_DX9_Cull_Mode_Name(unsigned value)
{
	switch (value) {
	case D3DCULL_NONE				: return "D3DCULL_NONE";
	case D3DCULL_CW				: return "D3DCULL_CW";
	case D3DCULL_CCW				: return "D3DCULL_CCW";
	default							: return "UNKNOWN";
	}
}

const char* DX9Backend::Get_DX9_Cmp_Func_Name(unsigned value)
{
	switch (value) {
	case D3DCMP_NEVER          : return "D3DCMP_NEVER";
	case D3DCMP_LESS           : return "D3DCMP_LESS";
	case D3DCMP_EQUAL          : return "D3DCMP_EQUAL";
	case D3DCMP_LESSEQUAL      : return "D3DCMP_LESSEQUAL";
	case D3DCMP_GREATER        : return "D3DCMP_GREATER";
	case D3DCMP_NOTEQUAL       : return "D3DCMP_NOTEQUAL";
	case D3DCMP_GREATEREQUAL   : return "D3DCMP_GREATEREQUAL";
	case D3DCMP_ALWAYS         : return "D3DCMP_ALWAYS";
	default							: return "UNKNOWN";
	}
}

const char* DX9Backend::Get_DX9_Fog_Mode_Name(unsigned value)
{
	switch (value) {
	case D3DFOG_NONE				: return "D3DFOG_NONE";
	case D3DFOG_EXP				: return "D3DFOG_EXP";
	case D3DFOG_EXP2				: return "D3DFOG_EXP2";
	case D3DFOG_LINEAR			: return "D3DFOG_LINEAR";
	default							: return "UNKNOWN";
	}
}

const char* DX9Backend::Get_DX9_Stencil_Op_Name(unsigned value)
{
	switch (value) {
	case D3DSTENCILOP_KEEP		: return "D3DSTENCILOP_KEEP";
	case D3DSTENCILOP_ZERO		: return "D3DSTENCILOP_ZERO";
	case D3DSTENCILOP_REPLACE	: return "D3DSTENCILOP_REPLACE";
	case D3DSTENCILOP_INCRSAT	: return "D3DSTENCILOP_INCRSAT";
	case D3DSTENCILOP_DECRSAT	: return "D3DSTENCILOP_DECRSAT";
	case D3DSTENCILOP_INVERT	: return "D3DSTENCILOP_INVERT";
	case D3DSTENCILOP_INCR		: return "D3DSTENCILOP_INCR";
	case D3DSTENCILOP_DECR		: return "D3DSTENCILOP_DECR";
	default							: return "UNKNOWN";
	}
}

const char* DX9Backend::Get_DX9_Material_Source_Name(unsigned value)
{
	switch (value) {
	case D3DMCS_MATERIAL			: return "D3DMCS_MATERIAL";
	case D3DMCS_COLOR1			: return "D3DMCS_COLOR1";
	case D3DMCS_COLOR2			: return "D3DMCS_COLOR2";
	default							: return "UNKNOWN";
	}
}

const char* DX9Backend::Get_DX9_Vertex_Blend_Flag_Name(unsigned value)
{
	switch (value) {
	case D3DVBF_DISABLE			: return "D3DVBF_DISABLE";
	case D3DVBF_1WEIGHTS			: return "D3DVBF_1WEIGHTS";
	case D3DVBF_2WEIGHTS			: return "D3DVBF_2WEIGHTS";
	case D3DVBF_3WEIGHTS			: return "D3DVBF_3WEIGHTS";
	case D3DVBF_TWEENING			: return "D3DVBF_TWEENING";
	case D3DVBF_0WEIGHTS			: return "D3DVBF_0WEIGHTS";
	default							: return "UNKNOWN";
	}
}

const char* DX9Backend::Get_DX9_Patch_Edge_Style_Name(unsigned value)
{
	switch (value) {
	case D3DPATCHEDGE_DISCRETE	: return "D3DPATCHEDGE_DISCRETE";
   case D3DPATCHEDGE_CONTINUOUS:return "D3DPATCHEDGE_CONTINUOUS";
	default							: return "UNKNOWN";
	}
}

const char* DX9Backend::Get_DX9_Debug_Monitor_Token_Name(unsigned value)
{
	switch (value) {
	case D3DDMT_ENABLE			: return "D3DDMT_ENABLE";
	case D3DDMT_DISABLE			: return "D3DDMT_DISABLE";
	default							: return "UNKNOWN";
	}
}

const char* DX9Backend::Get_DX9_Blend_Op_Name(unsigned value)
{
	switch (value) {
	case D3DBLENDOP_ADD			: return "D3DBLENDOP_ADD";
	case D3DBLENDOP_SUBTRACT	: return "D3DBLENDOP_SUBTRACT";
	case D3DBLENDOP_REVSUBTRACT: return "D3DBLENDOP_REVSUBTRACT";
	case D3DBLENDOP_MIN			: return "D3DBLENDOP_MIN";
	case D3DBLENDOP_MAX			: return "D3DBLENDOP_MAX";
	default							: return "UNKNOWN";
	}
}


//============================================================================
// DX9Backend::getBackBufferFormat
//============================================================================

WW3DFormat	DX9Backend::getBackBufferFormat() const
{
	return WW3D_Format_From_DX9( _PresentParameters.BackBufferFormat );
}


// Interface implementation and DX9 resource adapters.

namespace
{
	typedef _com_ptr_t<_com_IIID<IFEBrowserEngine2,
		&__uuidof(IFEBrowserEngine2)>> IFEBrowserEngine2Ptr;

	IFEBrowserEngine2Ptr Browser;
	HWND BrowserWindow = nullptr;

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
			const RenderBackendVertexLayout &layout) : Buffer(buffer), Layout(layout) {}

		virtual ~DX9BackendVertexBuffer() override
		{
			if (Buffer != nullptr)
			{
				Buffer->Release();
			}
		}

		IDirect3DVertexBuffer9 *Buffer;
		RenderBackendVertexLayout Layout;
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

	class DX9BackendFont final : public RenderBackendFont
	{
	public:
		explicit DX9BackendFont(ID3DXFont * font) :
			Font(font),
			GlyphFont(nullptr),
			GlyphDC(nullptr),
			GlyphBitmap(nullptr),
			PreviousBitmap(nullptr),
			PreviousFont(nullptr),
			GlyphBitmapBits(nullptr),
			GlyphBitmapWidth(0),
			GlyphBitmapHeight(0),
			GlyphBitmapPitch(0),
			GlyphHeight(0),
			GlyphAscent(0),
			GlyphOverhang(0)
		{}

		virtual ~DX9BackendFont() override
		{
			if (GlyphDC != nullptr)
			{
				if (PreviousFont != nullptr)
					SelectObject(GlyphDC, PreviousFont);
				if (PreviousBitmap != nullptr)
					SelectObject(GlyphDC, PreviousBitmap);
			}
			if (GlyphFont != nullptr)
				DeleteObject(GlyphFont);
			if (GlyphBitmap != nullptr)
				DeleteObject(GlyphBitmap);
			if (GlyphDC != nullptr)
				DeleteDC(GlyphDC);
			if (Font != nullptr)
			{
				Font->Release();
			}
		}

		bool Initialize_Glyph_Font(int height, const char * face_name, bool bold, int width)
		{
			if (face_name == nullptr)
				return false;

			const int font_height = std::max(1, height < 0 ? -height : height);
			GlyphBitmapWidth = std::max(64, font_height * 4);
			GlyphBitmapHeight = std::max(64, font_height * 2);
			GlyphBitmapPitch = (GlyphBitmapWidth * 3 + 3) & ~3;

			GlyphFont = CreateFontA(
				-font_height,
				width,
				0,
				0,
				bold ? FW_BOLD : FW_NORMAL,
				FALSE,
				FALSE,
				FALSE,
				DEFAULT_CHARSET,
				OUT_DEFAULT_PRECIS,
				CLIP_DEFAULT_PRECIS,
				ANTIALIASED_QUALITY,
				VARIABLE_PITCH,
				face_name);
			if (GlyphFont == nullptr)
				return false;

			GlyphDC = CreateCompatibleDC(nullptr);
			if (GlyphDC == nullptr)
				return false;

			BITMAPINFO bitmap_info = {};
			bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bitmap_info.bmiHeader.biWidth = GlyphBitmapWidth;
			bitmap_info.bmiHeader.biHeight = -GlyphBitmapHeight;
			bitmap_info.bmiHeader.biPlanes = 1;
			bitmap_info.bmiHeader.biBitCount = 24;
			bitmap_info.bmiHeader.biCompression = BI_RGB;
			GlyphBitmap = CreateDIBSection(
				GlyphDC,
				&bitmap_info,
				DIB_RGB_COLORS,
				reinterpret_cast<void **>(&GlyphBitmapBits),
				nullptr,
				0);
			if (GlyphBitmap == nullptr || GlyphBitmapBits == nullptr)
				return false;

			PreviousBitmap = SelectObject(GlyphDC, GlyphBitmap);
			PreviousFont = SelectObject(GlyphDC, GlyphFont);
			SetBkColor(GlyphDC, RGB(0, 0, 0));
			SetTextColor(GlyphDC, RGB(255, 255, 255));

			TEXTMETRIC text_metric = {};
			if (!GetTextMetrics(GlyphDC, &text_metric))
				return false;

			GlyphHeight = text_metric.tmHeight;
			GlyphAscent = text_metric.tmAscent;
			GlyphOverhang = text_metric.tmOverhang;
			return GlyphHeight > 0;
		}

		ID3DXFont *Font;
		HFONT GlyphFont;
		HDC GlyphDC;
		HBITMAP GlyphBitmap;
		HGDIOBJ PreviousBitmap;
		HGDIOBJ PreviousFont;
		unsigned char *GlyphBitmapBits;
		int GlyphBitmapWidth;
		int GlyphBitmapHeight;
		int GlyphBitmapPitch;
		int GlyphHeight;
		int GlyphAscent;
		int GlyphOverhang;
		std::vector<unsigned char> GlyphPixels;
	};

	DX9BackendSurface * To_DX9_Surface(RenderBackendSurface * surface)
	{
		return static_cast<DX9BackendSurface *>(surface);
	}

	SurfaceClass *Wrap_DX9_Surface(IDirect3DSurface9 *surface)
	{
		return surface == nullptr ? nullptr : new SurfaceClass(new DX9BackendSurface(surface));
	}

	IDirect3DSurface9 *Get_DX9_Surface(RenderBackendSurface *surface)
	{
		DX9BackendSurface *backend_surface = To_DX9_Surface(surface);
		return backend_surface == nullptr ? nullptr : backend_surface->Surface;
	}

	IDirect3DSurface9 * Get_DX9_Surface(SurfaceClass * surface)
	{
		DX9BackendSurface *backend_surface = surface == nullptr ? nullptr :
			To_DX9_Surface(surface->Get_Render_Backend_Surface());
		return backend_surface == nullptr ? nullptr : backend_surface->Surface;
	}

	DX9BackendVertexBuffer * To_DX9_Vertex_Buffer(RenderBackendVertexBuffer * buffer)
	{
		return static_cast<DX9BackendVertexBuffer *>(buffer);
	}

	DX9BackendIndexBuffer * To_DX9_Index_Buffer(RenderBackendIndexBuffer * buffer)
	{
		return static_cast<DX9BackendIndexBuffer *>(buffer);
	}

	DX9BackendFont * To_DX9_Font(RenderBackendFont * font)
	{
		return static_cast<DX9BackendFont *>(font);
	}

	DWORD To_D3D_Vertex_Format(const RenderBackendVertexLayout &layout)
	{
		DWORD format = layout.transformed ? D3DFVF_XYZRHW : D3DFVF_XYZ;
		if (layout.has_blend)
		{
			format = layout.transformed ? D3DFVF_XYZRHW : D3DFVF_XYZB4;
		}
		if (layout.has_normal) format |= D3DFVF_NORMAL;
		if (layout.has_diffuse) format |= D3DFVF_DIFFUSE;
		if (layout.has_specular) format |= D3DFVF_SPECULAR;

		switch (layout.texture_count)
		{
			case 0: break;
			case 1: format |= D3DFVF_TEX1; break;
			case 2: format |= D3DFVF_TEX2; break;
			case 3: format |= D3DFVF_TEX3; break;
			case 4: format |= D3DFVF_TEX4; break;
			case 5: format |= D3DFVF_TEX5; break;
			case 6: format |= D3DFVF_TEX6; break;
			case 7: format |= D3DFVF_TEX7; break;
			case 8: format |= D3DFVF_TEX8; break;
			default:
				WWASSERT(false);
				break;
		}

		for (unsigned index = 0; index < layout.texture_count &&
			index < RENDER_BACKEND_MAX_TEXTURE_COORDINATES; ++index)
		{
			switch (layout.texture_dimensions[index])
			{
				case 1: format |= D3DFVF_TEXCOORDSIZE1(index); break;
				case 2: format |= D3DFVF_TEXCOORDSIZE2(index); break;
				case 3: format |= D3DFVF_TEXCOORDSIZE3(index); break;
				case 4: format |= D3DFVF_TEXCOORDSIZE4(index); break;
				default: WWASSERT(false); break;
			}
		}

		return format;
	}

	DWORD To_D3D_Vertex_Format(RenderBackendVertexFormat format)
	{
		return To_D3D_Vertex_Format(RenderBackend_Vertex_Layout(format));
	}

	DWORD To_D3D_Usage(unsigned usage)
	{
		DWORD d3d_usage = D3DUSAGE_WRITEONLY;
		if (usage & BUFFER_USAGE_DYNAMIC) d3d_usage |= D3DUSAGE_DYNAMIC;
		if (usage & BUFFER_USAGE_SOFTWARE_PROCESSING) d3d_usage |= D3DUSAGE_SOFTWAREPROCESSING;
		if (usage & BUFFER_USAGE_NPATCHES) d3d_usage |= D3DUSAGE_NPATCHES;
		return d3d_usage;
	}

	D3DDECLTYPE To_D3D_Vertex_Input_Type(RenderBackendVertexInputType type)
	{
		switch (type)
		{
			case RenderBackendVertexInputType::Float1: return D3DDECLTYPE_FLOAT1;
			case RenderBackendVertexInputType::Float2: return D3DDECLTYPE_FLOAT2;
			case RenderBackendVertexInputType::Float3: return D3DDECLTYPE_FLOAT3;
			case RenderBackendVertexInputType::Float4: return D3DDECLTYPE_FLOAT4;
			case RenderBackendVertexInputType::Color: return D3DDECLTYPE_D3DCOLOR;
			default:
				WWASSERT(false);
				return D3DDECLTYPE_UNUSED;
		}
	}

	D3DDECLUSAGE To_D3D_Vertex_Input_Semantic(RenderBackendVertexInputSemantic semantic)
	{
		switch (semantic)
		{
			case RenderBackendVertexInputSemantic::Position: return D3DDECLUSAGE_POSITION;
			case RenderBackendVertexInputSemantic::BlendWeight: return D3DDECLUSAGE_BLENDWEIGHT;
			case RenderBackendVertexInputSemantic::BlendIndices: return D3DDECLUSAGE_BLENDINDICES;
			case RenderBackendVertexInputSemantic::Normal: return D3DDECLUSAGE_NORMAL;
			case RenderBackendVertexInputSemantic::PointSize: return D3DDECLUSAGE_PSIZE;
			case RenderBackendVertexInputSemantic::Color: return D3DDECLUSAGE_COLOR;
			case RenderBackendVertexInputSemantic::TextureCoordinate: return D3DDECLUSAGE_TEXCOORD;
			default:
				WWASSERT(false);
				return D3DDECLUSAGE_POSITION;
		}
	}

	const char * To_Vertex_Input_Declaration_Name(RenderBackendVertexInputSemantic semantic)
	{
		switch (semantic)
		{
			case RenderBackendVertexInputSemantic::Position: return "position";
			case RenderBackendVertexInputSemantic::BlendWeight: return "blendweight";
			case RenderBackendVertexInputSemantic::BlendIndices: return "blendindices";
			case RenderBackendVertexInputSemantic::Normal: return "normal";
			case RenderBackendVertexInputSemantic::PointSize: return "psize";
			case RenderBackendVertexInputSemantic::Color: return "color";
			case RenderBackendVertexInputSemantic::TextureCoordinate: return "texcoord";
			default: return nullptr;
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
			case RenderBackendBufferLockMode::Discard:
				return D3DLOCK_NOSYSLOCK | D3DLOCK_DISCARD;
			case RenderBackendBufferLockMode::NoOverwrite:
				return D3DLOCK_NOSYSLOCK | D3DLOCK_NOOVERWRITE;
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

	DWORD To_D3D_Texture_Argument(RenderBackendTextureArgument argument,
		RenderBackendTextureArgumentModifiers modifiers)
	{
		DWORD value;
		switch (argument)
		{
			case RenderBackendTextureArgument::Current: value = D3DTA_CURRENT; break;
			case RenderBackendTextureArgument::Diffuse: value = D3DTA_DIFFUSE; break;
			case RenderBackendTextureArgument::Texture: value = D3DTA_TEXTURE; break;
			case RenderBackendTextureArgument::TextureFactor: value = D3DTA_TFACTOR; break;
			case RenderBackendTextureArgument::CurrentAlpha: value = D3DTA_CURRENT | D3DTA_ALPHAREPLICATE; break;
			case RenderBackendTextureArgument::DiffuseAlpha: value = D3DTA_DIFFUSE | D3DTA_ALPHAREPLICATE; break;
			case RenderBackendTextureArgument::TextureAlpha: value = D3DTA_TEXTURE | D3DTA_ALPHAREPLICATE; break;
			case RenderBackendTextureArgument::TextureFactorAlpha: value = D3DTA_TFACTOR | D3DTA_ALPHAREPLICATE; break;
			default:
				WWASSERT(false);
				value = D3DTA_CURRENT;
				break;
		}
		const unsigned modifier_bits = static_cast<unsigned>(modifiers);
		if ((modifier_bits & static_cast<unsigned>(RenderBackendTextureArgumentModifiers::Complement)) != 0)
			value |= D3DTA_COMPLEMENT;
		if ((modifier_bits & static_cast<unsigned>(RenderBackendTextureArgumentModifiers::AlphaReplicate)) != 0)
			value |= D3DTA_ALPHAREPLICATE;
		return value;
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

DX9Backend::DX9Backend(bool lite) : Lite(lite), CleanupHook(nullptr)
{
}

DX9Backend::~DX9Backend()
{
	DX9TextureManagerClass::Shutdown();
	Shutdown_Browser();
	if (!Lite)
	{
		Shutdown();
	}
}

DX9Backend *DX9Backend::Create(void * window, bool lite)
{
	WWDEBUG_SAY(("Init DX9 backend"));

	DX9Backend *backend = new DX9Backend(lite);
	if (!backend->Initialize(window, lite))
	{
		backend->Shutdown();
		delete backend;
		return nullptr;
	}

	return backend;
}

void DX9Backend::Initialize_Mesh_Renderer()
{
	TheDX9MeshRenderer.Init();
}

void DX9Backend::Shutdown_Mesh_Renderer()
{
	TheDX9MeshRenderer.Shutdown();
}

void DX9Backend::Invalidate_Mesh_Renderer(bool shutdown)
{
	TheDX9MeshRenderer.Invalidate(shutdown);
}

void DX9Backend::Clear_Mesh_Renderer_Delete_Lists()
{
	TheDX9MeshRenderer.Clear_Pending_Delete_Lists();
}

void DX9Backend::Set_Mesh_Renderer_Camera(CameraClass * camera)
{
	TheDX9MeshRenderer.Set_Camera(camera);
}

void DX9Backend::Flush_Mesh_Renderer()
{
	TheDX9MeshRenderer.Flush();
}

void DX9Backend::Register_Mesh_Type(MeshModelClass * mesh)
{
	TheDX9MeshRenderer.Register_Mesh_Type(mesh);
}

void DX9Backend::Unregister_Mesh_Type(MeshModelClass * mesh)
{
	TheDX9MeshRenderer.Unregister_Mesh_Type(mesh);
}

void DX9Backend::Add_Decal_Mesh(DecalMeshClass * mesh)
{
	TheDX9MeshRenderer.Add_To_Render_List(mesh);
}

void DX9Backend::Set_Mesh_Renderer_Lighting(bool enable)
{
	TheDX9MeshRenderer.Enable_Lighting(enable);
}

void DX9Backend::Set_Force_Multiply(bool enable)
{
	DX9TextureCategoryClass::SetForceMultiply(enable);
}

void DX9Backend::Add_Renderer_Debug_Mesh(MeshClass * mesh)
{
	DX9RendererDebugger::Add_Mesh(mesh);
}

bool DX9Backend::Has_Mesh_Renderers(const MeshModelClass * mesh) const
{
	return TheDX9MeshRenderer.Has_Mesh_Renderers(mesh);
}

unsigned DX9Backend::Get_Mesh_Renderer_Vertex_Offset(const MeshModelClass * mesh) const
{
	return TheDX9MeshRenderer.Get_Mesh_Renderer_Vertex_Offset(mesh);
}

unsigned DX9Backend::Get_Mesh_Renderer_Count(const MeshModelClass * mesh) const
{
	return TheDX9MeshRenderer.Get_Mesh_Renderer_Count(mesh);
}

void DX9Backend::Update_Mesh_Texture(MeshModelClass * mesh, TextureClass * texture,
	TextureClass * new_texture, unsigned pass, unsigned stage)
{
	TheDX9MeshRenderer.Update_Mesh_Texture(mesh, texture, new_texture, pass, stage);
}

void DX9Backend::Update_Mesh_Material(MeshModelClass * mesh, VertexMaterialClass * material,
	VertexMaterialClass * new_material, unsigned pass)
{
	TheDX9MeshRenderer.Update_Mesh_Material(mesh, material, new_material, pass);
}

void DX9Backend::Add_Mesh_Render_Tasks(MeshModelClass * mesh, MeshClass * instance)
{
	TheDX9MeshRenderer.Add_Mesh_Render_Tasks(mesh, instance);
}

void DX9Backend::Add_Mesh_Material_Pass(MeshModelClass * mesh, MaterialPassClass * pass,
	MeshClass * instance, bool delayed)
{
	TheDX9MeshRenderer.Add_Mesh_Material_Pass(mesh, pass, instance, delayed);
}

void DX9Backend::Add_Mesh_Skin(MeshModelClass * mesh, MeshClass * instance)
{
	TheDX9MeshRenderer.Add_Mesh_Skin(mesh, instance);
}

void DX9Backend::Render_Mesh_Pass(MeshModelClass * mesh, int base_vertex_offset)
{
	TheDX9MeshRenderer.Render_Mesh_Pass(mesh, base_vertex_offset);
}

void DX9Backend::Initialize_Sorting_Renderer()
{
	// The sorting renderer currently initializes lazily. Keep this entry point
	// so its ownership remains with the backend as the implementation moves.
}

void DX9Backend::Shutdown_Sorting_Renderer()
{
	SortingRendererClass::Deinit();
}

void DX9Backend::Set_Sorting_Min_Vertex_Buffer_Size(unsigned value)
{
	SortingRendererClass::SetMinVertexBufferSize(value);
}

void DX9Backend::Insert_Sorted_Triangles(const SphereClass & bounding_sphere,
	unsigned short start_index, unsigned short polygon_count,
	unsigned short min_vertex_index, unsigned short vertex_count)
{
	SortingRendererClass::Insert_Triangles(bounding_sphere, start_index,
		polygon_count, min_vertex_index, vertex_count);
}

void DX9Backend::Insert_Sorted_Triangles(unsigned short start_index,
	unsigned short polygon_count, unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	SortingRendererClass::Insert_Triangles(start_index, polygon_count,
		min_vertex_index, vertex_count);
}

void DX9Backend::Flush_Sorting_Renderer()
{
	SortingRendererClass::Flush();
}




bool DX9Backend::Supports_TnL() const
{
	return DX9Backend::Get_Current_Caps()->Support_TnL();
}

bool DX9Backend::Supports_DXTC() const
{
	return DX9Backend::Get_Current_Caps()->Support_DXTC();
}

bool DX9Backend::Supports_NPatches() const
{
	return DX9Backend::Get_Current_Caps()->Support_NPatches();
}

bool DX9Backend::Supports_Bump_Envmap() const
{
	return DX9Backend::Get_Current_Caps()->Support_Bump_Envmap();
}

bool DX9Backend::Supports_Bump_Envmap_Luminance() const
{
	return DX9Backend::Get_Current_Caps()->Support_Bump_Envmap_Luminance();
}

bool DX9Backend::Supports_Z_Bias() const
{
	return DX9Backend::Get_Current_Caps()->Support_ZBias();
}

bool DX9Backend::Supports_Anisotropic_Filtering() const
{
	return DX9Backend::Get_Current_Caps()->Support_Anisotropic_Filtering();
}

bool DX9Backend::Supports_Modulate_Alpha_Add_Color() const
{
	return DX9Backend::Get_Current_Caps()->Support_ModAlphaAddClr();
}

bool DX9Backend::Supports_Dot3() const
{
	return DX9Backend::Get_Current_Caps()->Support_Dot3();
}

bool DX9Backend::Supports_Point_Sprites() const
{
	return DX9Backend::Get_Current_Caps()->Support_PointSprites();
}

bool DX9Backend::Supports_Cubemaps() const
{
	return DX9Backend::Get_Current_Caps()->Support_Cubemaps();
}

bool DX9Backend::Supports_Color_Write_Mask() const
{
	return (DX9Backend::Get_Current_Caps()->Get_DX9_Caps().PrimitiveMiscCaps &
		D3DPMISCCAPS_COLORWRITEENABLE) != 0;
}

bool DX9Backend::Supports_Texture_Operation(RenderBackendTextureOperation operation) const
{
	if (operation == RenderBackendTextureOperation::Disable)
		return true;

	return (DX9Backend::Get_Current_Caps()->Get_DX9_Caps().TextureOpCaps &
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
		(DX9Backend::Get_Current_Caps()->Get_DX9_Caps().TextureFilterCaps & capability) != 0;
}

bool DX9Backend::Is_Fog_Allowed() const
{
	return DX9Backend::Get_Current_Caps()->Is_Fog_Allowed();
}

bool DX9Backend::Is_Fog_Enabled() const
{
	return DX9Backend::Get_Fog_Enable();
}

unsigned DX9Backend::Get_Fog_Color() const
{
	return static_cast<unsigned>(FogColor);
}

bool DX9Backend::Supports_Texture_Format(WW3DFormat format) const
{
	return DX9Backend::Get_Current_Caps()->Support_Texture_Format(format);
}

bool DX9Backend::Supports_Render_To_Texture_Format(WW3DFormat format) const
{
	return DX9Backend::Get_Current_Caps()->Support_Render_To_Texture_Format(format);
}

bool DX9Backend::Supports_Depth_Stencil_Format(WW3DZFormat format) const
{
	return DX9Backend::Get_Current_Caps()->Support_Depth_Stencil_Format(format);
}

WW3DFormat DX9Backend::Get_Back_Buffer_Format() const
{
	return DX9Backend::getBackBufferFormat();
}

bool DX9Backend::Is_Device_Ready() const
{
	return Get_Device_Status() == RenderBackendDeviceStatus::Ready;
}

bool DX9Backend::Is_Render_Thread() const
{
	return ThreadClass::_Get_Current_Thread_ID() == _Get_Main_Thread_ID();
}

RenderBackendDeviceStatus DX9Backend::Get_Device_Status() const
{
	IDirect3DDevice9 *device = DX9Backend::_Get_D3D_Device();
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
	return DX9Backend::Get_Current_Caps()->Get_Max_Textures_Per_Pass();
}

bool DX9Backend::Is_3DFX_Voodoo3() const
{
	const DX9Caps *caps = DX9Backend::Get_Current_Caps();
	return caps->Get_Vendor() == DX9Caps::VENDOR_3DFX &&
		caps->Get_Device() == DX9Caps::DEVICE_3DFX_VOODOO_3;
}

unsigned DX9Backend::Pack_Color(const Vector4 & color) const
{
	return DX9Backend::Convert_Color(color);
}

unsigned DX9Backend::Pack_Color(const Vector3 & color, float alpha) const
{
	return DX9Backend::Convert_Color(color, alpha);
}

unsigned DX9Backend::Pack_Color_Clamped(const Vector4 & color) const
{
	return DX9Backend::Convert_Color_Clamp(color);
}

Vector4 DX9Backend::Unpack_Color(unsigned color) const
{
	return DX9Backend::Convert_Color(color);
}

bool DX9Backend::Is_Triangle_Draw_Enabled() const
{
	return DX9Backend::_Is_Triangle_Draw_Enabled();
}

void DX9Backend::Set_Triangle_Draw_Enabled(bool enable)
{
	DX9Backend::_Enable_Triangle_Draw(enable);
}

RenderBackendDebugSettings & DX9Backend::Get_Debug_Settings()
{
	return DebugSettings;
}

void DX9Backend::Set_Cleanup_Hook(RenderBackendCleanupHook * hook)
{
	CleanupHook = hook;
}

void DX9Backend::Invalidate_Renderer_Caches()
{
	TheDX9MeshRenderer.Invalidate();
}

RenderBackendFont * DX9Backend::Create_Font(int height, const char * face_name, bool bold, int width)
{
	IDirect3DDevice9 *device = DX9Backend::_Get_D3D_Device();
	if (device == nullptr || face_name == nullptr)
	{
		return nullptr;
	}

	ID3DXFont *font = nullptr;
	const int font_height = std::max(1, height < 0 ? -height : height);
	if (FAILED(D3DXCreateFontA(device, font_height, width, bold ? FW_BOLD : FW_REGULAR, 1, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, face_name, &font)))
	{
		return nullptr;
	}

	DX9BackendFont *backend_font = new DX9BackendFont(font);
	if (!backend_font->Initialize_Glyph_Font(height, face_name, bold, width))
	{
		delete backend_font;
		return nullptr;
	}

	return backend_font;
}

void DX9Backend::Release_Font(RenderBackendFont * font)
{
	delete To_DX9_Font(font);
}

bool DX9Backend::Get_Font_Metrics(RenderBackendFont * font, RenderBackendFontMetrics & metrics) const
{
	DX9BackendFont *dx9_font = To_DX9_Font(font);
	if (dx9_font == nullptr || dx9_font->GlyphHeight <= 0)
	{
		return false;
	}

	metrics.height = dx9_font->GlyphHeight;
	metrics.ascent = dx9_font->GlyphAscent;
	metrics.overhang = dx9_font->GlyphOverhang;
	return true;
}

bool DX9Backend::Get_Font_Glyph(RenderBackendFont * font, unsigned int character,
	RenderBackendFontGlyph & glyph)
{
	DX9BackendFont *dx9_font = To_DX9_Font(font);
	if (dx9_font == nullptr || dx9_font->GlyphDC == nullptr ||
		dx9_font->GlyphBitmapBits == nullptr || dx9_font->GlyphHeight <= 0)
	{
		return false;
	}

	RECT glyph_rect = { 0, 0, dx9_font->GlyphBitmapWidth, dx9_font->GlyphBitmapHeight };
	std::memset(dx9_font->GlyphBitmapBits, 0,
		static_cast<std::size_t>(dx9_font->GlyphBitmapPitch) * dx9_font->GlyphBitmapHeight);

	const WCHAR wide_character = static_cast<WCHAR>(character);
	if (!ExtTextOutW(dx9_font->GlyphDC, 0, 0, ETO_OPAQUE, &glyph_rect,
		&wide_character, 1, nullptr))
	{
		return false;
	}

	SIZE character_size = {};
	if (!GetTextExtentPoint32W(dx9_font->GlyphDC, &wide_character, 1, &character_size))
	{
		return false;
	}

	const unsigned int glyph_width = static_cast<unsigned int>(std::max(0, std::min(
		static_cast<int>(character_size.cx), dx9_font->GlyphBitmapWidth)));
	const unsigned int glyph_height = static_cast<unsigned int>(std::min(
		dx9_font->GlyphHeight, dx9_font->GlyphBitmapHeight));
	if (glyph_width == 0 || glyph_height == 0)
	{
		return false;
	}

	dx9_font->GlyphPixels.assign(static_cast<std::size_t>(glyph_width) * glyph_height, 0);
	for (unsigned int row = 0; row < glyph_height; ++row)
	{
		const unsigned char *source = dx9_font->GlyphBitmapBits +
			static_cast<std::size_t>(row) * dx9_font->GlyphBitmapPitch;
		unsigned char *destination = dx9_font->GlyphPixels.data() +
			static_cast<std::size_t>(row) * glyph_width;
		for (unsigned int column = 0; column < glyph_width; ++column)
		{
			destination[column] = source[column * 3];
		}
	}

	glyph.width = glyph_width;
	glyph.height = glyph_height;
	glyph.pitch = glyph_width;
	glyph.pixels = dx9_font->GlyphPixels.data();
	return true;
}

void DX9Backend::Draw_Font(RenderBackendFont * font, const char * text,
	unsigned text_length, const RenderBackendRect & rect, unsigned flags,
	unsigned color)
{
	DX9BackendFont *dx9_font = To_DX9_Font(font);
	if (dx9_font == nullptr || dx9_font->Font == nullptr || text == nullptr)
	{
		return;
	}

	RECT d3d_rect = { rect.left, rect.top, rect.right, rect.bottom };
	DWORD d3d_flags = 0;
	if ((flags & RenderBackendFontDrawFlagLeft) != 0)
		d3d_flags |= DT_LEFT;
	if ((flags & RenderBackendFontDrawFlagNoClip) != 0)
		d3d_flags |= DT_NOCLIP;
	if ((flags & RenderBackendFontDrawFlagTop) != 0)
		d3d_flags |= DT_TOP;
	if ((flags & RenderBackendFontDrawFlagSingleLine) != 0)
		d3d_flags |= DT_SINGLELINE;

	dx9_font->Font->DrawTextA(nullptr, text, static_cast<INT>(text_length),
		&d3d_rect, d3d_flags, color);
}

bool DX9Backend::Initialize_Browser(const char * bad_page_url,
	const char * loading_page_url, const char * mouse_filename,
	const char * mouse_busy_filename)
{
	if (Browser != 0)
	{
		return true;
	}

	CoInitialize(nullptr);
	HRESULT result = Browser.CreateInstance(__uuidof(FEBrowserEngine2));
	if (result == REGDB_E_CLASSNOTREG)
	{
		HMODULE library = ::LoadLibraryA("BrowserEngine.DLL");
		if (library != nullptr)
		{
			FARPROC register_server = ::GetProcAddress(library, "DllRegisterServer");
			if (register_server != nullptr)
			{
				reinterpret_cast<HRESULT (STDAPICALLTYPE *)()>(register_server)();
				result = Browser.CreateInstance(__uuidof(FEBrowserEngine2));
			}
			::FreeLibrary(library);
		}
	}

	if (FAILED(result))
	{
		Browser = 0;
		CoUninitialize();
		return false;
	}

	BrowserWindow = static_cast<HWND>(WW3D::Get_Window());
	Browser->Initialize(reinterpret_cast<long *>(DX9Backend::_Get_D3D_Device()));

	if (bad_page_url != nullptr)
		Browser->put_BadPageURL(_bstr_t(bad_page_url));
	if (loading_page_url != nullptr)
		Browser->put_LoadingPageURL(_bstr_t(loading_page_url));
	if (mouse_filename != nullptr)
		Browser->put_MouseFileName(_bstr_t(mouse_filename));
	if (mouse_busy_filename != nullptr)
		Browser->put_MouseBusyFileName(_bstr_t(mouse_busy_filename));

	return true;
}

void DX9Backend::Shutdown_Browser()
{
	if (Browser != 0)
	{
		Browser->Shutdown();
		Browser = 0;
		BrowserWindow = nullptr;
		CoUninitialize();
	}
}

void DX9Backend::Update_Browser()
{
	if (Browser != 0)
		Browser->D3DUpdate();
}

void DX9Backend::Render_Browser(int backbuffer_index)
{
	if (Browser != 0)
		Browser->D3DRender(backbuffer_index);
}

void DX9Backend::Create_Browser(const char * browser_name, const char * url,
	int x, int y, int width, int height, int update_ticks,
	unsigned options, void * game_dispatch)
{
	WWDEBUG_SAY(("DX9Backend::Create_Browser - creating browser %s, url = %s, "
		"(x, y, w, h) = (%d, %d, %d, %d), update ticks = %d",
		browser_name, url, x, y, width, height, update_ticks));
	if (Browser != 0)
	{
		_bstr_t name(browser_name);
		Browser->CreateBrowser(name, _bstr_t(url),
			reinterpret_cast<long>(BrowserWindow), x, y, width, height,
			static_cast<LONG>(options), static_cast<LPDISPATCH>(game_dispatch));
		Browser->SetUpdateRate(name, update_ticks);
	}
}

void DX9Backend::Destroy_Browser(const char * browser_name)
{
	WWDEBUG_SAY(("DX9Backend::Destroy_Browser - destroying browser %s", browser_name));
	if (Browser != 0)
		Browser->DestroyBrowser(_bstr_t(browser_name));
}

bool DX9Backend::Is_Browser_Open(const char * browser_name) const
{
	if (Browser == 0)
		return false;

	long is_open = 0;
	return Browser->IsOpen(_bstr_t(browser_name), &is_open) != 0;
}

void DX9Backend::Navigate_Browser(const char * browser_name, const char * url)
{
	if (Browser != 0)
		Browser->Navigate(_bstr_t(browser_name), _bstr_t(url));
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
	DX9Backend::Set_MSAA_Mode(d3d_mode);
}

RenderBackendMultisampleMode DX9Backend::Get_Multisample_Mode() const
{
	switch (DX9Backend::Get_MSAA_Mode())
	{
		case D3DMULTISAMPLE_2_SAMPLES: return RenderBackendMultisampleMode::Samples2;
		case D3DMULTISAMPLE_4_SAMPLES: return RenderBackendMultisampleMode::Samples4;
		case D3DMULTISAMPLE_8_SAMPLES: return RenderBackendMultisampleMode::Samples8;
		case D3DMULTISAMPLE_NONE:
		default: return RenderBackendMultisampleMode::None;
	}
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
	DX9Backend::Set_Viewport(&vp);
}

bool DX9Backend::Get_Viewport(RenderBackendViewport & viewport) const
{
	if (D3DDevice == nullptr)
		return false;
	D3DVIEWPORT9 native_viewport{};
	if (FAILED(D3DDevice->GetViewport(&native_viewport)))
		return false;
	viewport = {native_viewport.X, native_viewport.Y, native_viewport.Width,
		native_viewport.Height, native_viewport.MinZ, native_viewport.MaxZ};
	return true;
}

bool DX9Backend::Create_Pixel_Shader(const void * bytecode, uintptr_t * shader)
{
	if (D3DDevice == nullptr || bytecode == nullptr || shader == nullptr)
		return false;
	IDirect3DPixelShader9 * pixel_shader = nullptr;
	void *aligned_bytecode = nullptr;
	const DWORD *shader_bytecode = Copy_DX9_Shader_Bytecode_Aligned(bytecode, &aligned_bytecode);
	const HRESULT result = D3DDevice->CreatePixelShader(shader_bytecode, &pixel_shader);
	if (FAILED(result))
	{
		SDL_Log("DX9Backend: CreatePixelShader failed (hr=0x%08lX)",
			static_cast<unsigned long>(result));
		if (aligned_bytecode != nullptr)
		{
			_aligned_free(aligned_bytecode);
			aligned_bytecode = nullptr;
		}
		return false;
	}
	if (aligned_bytecode != nullptr)
	{
		_aligned_free(aligned_bytecode);
	}
	*shader = reinterpret_cast<uintptr_t>(pixel_shader);
	return true;
}

bool DX9Backend::Create_Pixel_Shader_From_Source(const char * source, uintptr_t * shader)
{
	if (source == nullptr || shader == nullptr)
	{
		return false;
	}

	ID3DXBuffer *compiled_shader = nullptr;
	if (FAILED(D3DXAssembleShader(source, static_cast<UINT>(std::strlen(source)),
		nullptr, nullptr, 0, &compiled_shader, nullptr)))
	{
		return false;
	}

	const bool created = Create_Pixel_Shader(compiled_shader->GetBufferPointer(), shader);
	compiled_shader->Release();
	return created;
}

void DX9Backend::Release_Vertex_Shader_Input_Layout()
{
	if (Vertex_Declaration != nullptr)
	{
		Vertex_Declaration->Release();
		Vertex_Declaration = nullptr;
	}
}

bool DX9Backend::Set_Vertex_Shader_Input_Layout(
	const RenderBackendVertexShaderInputLayout & layout)
{
	if (D3DDevice == nullptr || layout.element_count == 0 ||
		layout.element_count > RENDER_BACKEND_MAX_VERTEX_INPUT_ELEMENTS)
	{
		return false;
	}

	D3DVERTEXELEMENT9 declaration[
		RENDER_BACKEND_MAX_VERTEX_INPUT_ELEMENTS + 1] = {};
	for (unsigned index = 0; index < layout.element_count; ++index)
	{
		const RenderBackendVertexInputElement & source = layout.elements[index];
		if (source.stream > 0xffffU || source.offset > 0xffffU ||
			source.semantic_index > 0xffU || source.shader_register > 0xffU)
		{
			return false;
		}

		D3DVERTEXELEMENT9 & destination = declaration[index];
		destination.Stream = static_cast<WORD>(source.stream);
		destination.Offset = static_cast<WORD>(source.offset);
		destination.Type = static_cast<BYTE>(To_D3D_Vertex_Input_Type(source.type));
		destination.Method = D3DDECLMETHOD_DEFAULT;
		destination.Usage = static_cast<BYTE>(
			To_D3D_Vertex_Input_Semantic(source.semantic));
		destination.UsageIndex = static_cast<BYTE>(source.semantic_index);
	}
	declaration[layout.element_count] = D3DDECL_END();

	IDirect3DVertexDeclaration9 * new_declaration = nullptr;
	HRESULT result = D3DDevice->CreateVertexDeclaration(
		declaration, &new_declaration);
	if (FAILED(result) || new_declaration == nullptr)
	{
		SDL_Log("DX9Backend: failed to create vertex declaration (hr=0x%08lX)",
			static_cast<unsigned long>(result));
		return false;
	}

	Release_Vertex_Shader_Input_Layout();
	Vertex_Declaration = new_declaration;
	result = D3DDevice->SetVertexDeclaration(Vertex_Declaration);
	if (FAILED(result))
	{
		SDL_Log("DX9Backend: failed to bind vertex declaration (hr=0x%08lX)",
			static_cast<unsigned long>(result));
		Release_Vertex_Shader_Input_Layout();
		return false;
	}

	return true;
}

bool DX9Backend::Create_Vertex_Shader(const void * bytecode, uintptr_t * shader,
	const RenderBackendVertexShaderInputLayout * input_layout)
{
	if (D3DDevice == nullptr || bytecode == nullptr || shader == nullptr)
		return false;
	*shader = 0;
	IDirect3DVertexShader9 * vertex_shader = nullptr;
	void *aligned_bytecode = nullptr;
	const DWORD *shader_bytecode = Copy_DX9_Shader_Bytecode_Aligned(bytecode, &aligned_bytecode);
	HRESULT result = D3DDevice->CreateVertexShader(shader_bytecode, &vertex_shader);
	if (FAILED(result))
	{
		ID3DXBuffer *disassembly = nullptr;
		ID3DXBuffer *assembled_shader = nullptr;
		ID3DXBuffer *assembly_errors = nullptr;
		if (SUCCEEDED(D3DXDisassembleShader(shader_bytecode, FALSE, nullptr, &disassembly)))
		{
			const char *assembly_source = static_cast<const char *>(disassembly->GetBufferPointer());
			const char *instruction_start = std::strchr(assembly_source, '\n');
			std::string normalized_source = assembly_source;
			if (instruction_start != nullptr && std::strncmp(assembly_source, "    vs_1_1", 10) == 0)
			{
				normalized_source = "vs_1_1\n";
				if (input_layout != nullptr)
				{
					for (unsigned index = 0; index < input_layout->element_count; ++index)
					{
						const RenderBackendVertexInputElement & element =
							input_layout->elements[index];
						const char * declaration_name =
							To_Vertex_Input_Declaration_Name(element.semantic);
						if (declaration_name == nullptr)
						{
							continue;
						}

						normalized_source += "dcl_";
						normalized_source += declaration_name;
						if (element.semantic ==
							RenderBackendVertexInputSemantic::TextureCoordinate ||
							element.semantic_index != 0)
						{
							normalized_source += std::to_string(element.semantic_index);
						}
						normalized_source += " v";
						normalized_source += std::to_string(element.shader_register);
						normalized_source += "\n";
					}
				}
				normalized_source.append(instruction_start + 1);
			}
			const HRESULT assembly_result = D3DXAssembleShader(normalized_source.c_str(),
				static_cast<UINT>(normalized_source.length()), nullptr, nullptr, 0,
				&assembled_shader, &assembly_errors);
			if (SUCCEEDED(assembly_result))
			{
				result = D3DDevice->CreateVertexShader(
					static_cast<const DWORD *>(assembled_shader->GetBufferPointer()), &vertex_shader);
			}
			else if (assembly_errors != nullptr)
			{
				SDL_Log("DX9Backend: vertex shader reassembly failed:\n%s",
					static_cast<const char *>(assembly_errors->GetBufferPointer()));
			}
		}
		if (assembly_errors != nullptr)
		{
			assembly_errors->Release();
		}
		if (disassembly != nullptr)
		{
			disassembly->Release();
		}
		if (assembled_shader != nullptr)
		{
			assembled_shader->Release();
		}
		if (aligned_bytecode != nullptr)
		{
			_aligned_free(aligned_bytecode);
			aligned_bytecode = nullptr;
		}
		if (FAILED(result))
		{
			return false;
		}
	}
	if (aligned_bytecode != nullptr)
	{
		_aligned_free(aligned_bytecode);
	}
	*shader = reinterpret_cast<uintptr_t>(vertex_shader);
	return true;
}

bool DX9Backend::Get_Adapter_Info(RenderBackendAdapterInfo & info) const
{
	if (D3DInterface == nullptr)
		return false;
	D3DADAPTER_IDENTIFIER9 identifier{};
	if (FAILED(D3DInterface->GetAdapterIdentifier(0, D3DENUM_NO_WHQL_LEVEL, &identifier)))
		return false;
	info.vendor_id = identifier.VendorId;
	info.device_id = identifier.DeviceId;
	info.driver_version_high = identifier.DriverVersion.HighPart;
	info.driver_version_low = identifier.DriverVersion.LowPart;
	return true;
}

void DX9Backend::Show_Cursor(bool show)
{
	if (D3DDevice != nullptr)
		D3DDevice->ShowCursor(show ? TRUE : FALSE);
}

bool DX9Backend::Set_Cursor_Properties(int hotspot_x, int hotspot_y, SurfaceClass * surface)
{
	return D3DDevice != nullptr && surface != nullptr &&
		SUCCEEDED(D3DDevice->SetCursorProperties(hotspot_x, hotspot_y, Get_DX9_Surface(surface)));
}

void DX9Backend::Set_Cursor_Position(int x, int y)
{
	if (D3DDevice != nullptr)
		D3DDevice->SetCursorPosition(x, y, D3DCURSOR_IMMEDIATE_UPDATE);
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
	DX9Backend::Set_DX9_Material(&d3d_material);
}

void DX9Backend::Set_Fill_Mode(RenderBackendFillMode mode)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_FILLMODE, To_D3D_Fill_Mode(mode));
}

RenderBackendFillMode DX9Backend::Get_Fill_Mode() const
{
	DWORD value = D3DFILL_SOLID;
	IDirect3DDevice9 *device = DX9Backend::_Get_D3D_Device();
	if (device != nullptr)
		device->GetRenderState(D3DRS_FILLMODE, &value);

	switch (value)
	{
		case D3DFILL_POINT: return RenderBackendFillMode::Point;
		case D3DFILL_WIREFRAME: return RenderBackendFillMode::Wireframe;
		case D3DFILL_SOLID:
		default: return RenderBackendFillMode::Solid;
	}
}

void DX9Backend::Set_Color_Write_Mask(RenderBackendColorWriteMask mask)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_COLORWRITEENABLE,
		To_D3D_Color_Write_Mask(mask));
}

RenderBackendColorWriteMask DX9Backend::Get_Color_Write_Mask() const
{
	DWORD mask = 0;
	DX9Backend::_Get_D3D_Device()->GetRenderState(D3DRS_COLORWRITEENABLE, &mask);
	return From_D3D_Color_Write_Mask(mask);
}

void DX9Backend::Set_Alpha_Blend_Enabled(bool enable)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_ALPHABLENDENABLE, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Blend_Operation(RenderBackendBlendOperation operation)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_BLENDOP, To_D3D_Blend_Operation(operation));
}

void DX9Backend::Set_Blend_Factors(RenderBackendBlendFactor source,
	RenderBackendBlendFactor destination)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_SRCBLEND, To_D3D_Blend_Factor(source));
	DX9Backend::Set_DX9_Render_State(D3DRS_DESTBLEND, To_D3D_Blend_Factor(destination));
}

void DX9Backend::Set_Source_Blend_Factor(RenderBackendBlendFactor factor)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_SRCBLEND, To_D3D_Blend_Factor(factor));
}

void DX9Backend::Set_Destination_Blend_Factor(RenderBackendBlendFactor factor)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_DESTBLEND, To_D3D_Blend_Factor(factor));
}

void DX9Backend::Set_Alpha_Test_Enabled(bool enable)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_ALPHATESTENABLE, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Alpha_Test_Function(RenderBackendCompareFunction function)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_ALPHAFUNC, To_D3D_Compare_Function(function));
}

void DX9Backend::Set_Alpha_Test_Reference(unsigned reference)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_ALPHAREF, reference);
}

void DX9Backend::Set_Fog_Enabled(bool enable)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_FOGENABLE, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Fog_Color(unsigned color)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_FOGCOLOR, color);
}

void DX9Backend::Set_Depth_Bias(unsigned bias)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_ZBIAS, bias);
}

void DX9Backend::Set_Texture_Factor(unsigned color)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_TEXTUREFACTOR, color);
}

void DX9Backend::Set_Depth_Test_Enabled(bool enable)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_ZENABLE, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Depth_Write_Enabled(bool enable)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_ZWRITEENABLE, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Depth_Function(RenderBackendCompareFunction function)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_ZFUNC, To_D3D_Compare_Function(function));
}

void DX9Backend::Set_Cull_Mode(RenderBackendCullMode mode)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_CULLMODE, To_D3D_Cull_Mode(mode));
}

RenderBackendCullMode DX9Backend::Get_Cull_Mode() const
{
	DWORD value = D3DCULL_CCW;
	IDirect3DDevice9 *device = DX9Backend::_Get_D3D_Device();
	if (device != nullptr)
		device->GetRenderState(D3DRS_CULLMODE, &value);

	switch (value)
	{
		case D3DCULL_NONE: return RenderBackendCullMode::None;
		case D3DCULL_CW: return RenderBackendCullMode::Clockwise;
		case D3DCULL_CCW:
		default: return RenderBackendCullMode::CounterClockwise;
	}
}

void DX9Backend::Set_Point_Sprite_Enabled(bool enable)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_POINTSPRITEENABLE, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Point_Scale_Enabled(bool enable)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_POINTSCALEENABLE, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Point_Size(float size)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_POINTSIZE, Float_To_Dword(size));
}

void DX9Backend::Set_Point_Size_Min(float size)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_POINTSIZE_MIN, Float_To_Dword(size));
}

void DX9Backend::Set_Point_Size_Max(float size)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_POINTSIZE_MAX, Float_To_Dword(size));
}

void DX9Backend::Set_Point_Scale(float scale_a, float scale_b, float scale_c)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_POINTSCALE_A, Float_To_Dword(scale_a));
	DX9Backend::Set_DX9_Render_State(D3DRS_POINTSCALE_B, Float_To_Dword(scale_b));
	DX9Backend::Set_DX9_Render_State(D3DRS_POINTSCALE_C, Float_To_Dword(scale_c));
}

void DX9Backend::Set_Shade_Mode(RenderBackendShadeMode mode)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_SHADEMODE, To_D3D_Shade_Mode(mode));
}

void DX9Backend::Set_Lighting_Enabled(bool enable)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_LIGHTING, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Normalize_Normals(bool enable)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_NORMALIZENORMALS, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Specular_Enabled(bool enable)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_SPECULARENABLE, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Material_Color_Sources(RenderBackendMaterialSource ambient,
	RenderBackendMaterialSource diffuse,
	RenderBackendMaterialSource emissive)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_AMBIENTMATERIALSOURCE,
		To_D3D_Material_Source(ambient));
	DX9Backend::Set_DX9_Render_State(D3DRS_DIFFUSEMATERIALSOURCE,
		To_D3D_Material_Source(diffuse));
	DX9Backend::Set_DX9_Render_State(D3DRS_EMISSIVEMATERIALSOURCE,
		To_D3D_Material_Source(emissive));
}

void DX9Backend::Set_NPatch_Segments(float segments)
{
	// The DX9 compatibility wrapper deliberately treats its legacy
	// D3DRS_PATCHSEGMENTS token as unsupported. Keep that behavior behind the
	// backend boundary until the DX9 tessellation path is implemented.
	DX9Backend::Set_DX9_Render_State(D3DRS_PATCHSEGMENTS, Float_To_Dword(segments));
}

void DX9Backend::Set_Stencil_Enabled(bool enable)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_STENCILENABLE, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Stencil_Function(RenderBackendCompareFunction function)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_STENCILFUNC, To_D3D_Compare_Function(function));
}

void DX9Backend::Set_Stencil_Reference(unsigned reference)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_STENCILREF, reference);
}

void DX9Backend::Set_Stencil_Read_Mask(unsigned mask)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_STENCILMASK, mask);
}

void DX9Backend::Set_Stencil_Write_Mask(unsigned mask)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_STENCILWRITEMASK, mask);
}

void DX9Backend::Set_Stencil_Z_Fail_Operation(RenderBackendStencilOperation operation)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_STENCILZFAIL, To_D3D_Stencil_Operation(operation));
}

void DX9Backend::Set_Stencil_Fail_Operation(RenderBackendStencilOperation operation)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_STENCILFAIL, To_D3D_Stencil_Operation(operation));
}

void DX9Backend::Set_Stencil_Pass_Operation(RenderBackendStencilOperation operation)
{
	DX9Backend::Set_DX9_Render_State(D3DRS_STENCILPASS, To_D3D_Stencil_Operation(operation));
}

void DX9Backend::Set_Texture_Operation(unsigned stage,
	RenderBackendTextureComponent component,
	RenderBackendTextureOperation operation)
{
	const D3DTEXTURESTAGESTATETYPE state = component == RenderBackendTextureComponent::Color
		? D3DTSS_COLOROP
		: D3DTSS_ALPHAOP;
	DX9Backend::Set_DX9_Texture_Stage_State(stage, state, To_D3D_Texture_Operation(operation));
}

void DX9Backend::Set_Texture_Argument(unsigned stage,
	RenderBackendTextureComponent component,
	unsigned argument_index,
	RenderBackendTextureArgument argument,
	RenderBackendTextureArgumentModifiers modifiers)
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
	DX9Backend::Set_DX9_Texture_Stage_State(stage, state, To_D3D_Texture_Argument(argument, modifiers));
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
	DX9Backend::Set_DX9_Texture_Stage_State(stage, D3DTSS_TEXCOORDINDEX, value);
}

void DX9Backend::Set_Texture_Transform_Flags(unsigned stage,
	RenderBackendTextureTransformFlags flags)
{
	DX9Backend::Set_DX9_Texture_Stage_State(stage, D3DTSS_TEXTURETRANSFORMFLAGS,
		To_D3D_Texture_Transform_Flags(flags));
}

void DX9Backend::Set_Texture_Address_Mode(unsigned stage,
	bool u_coordinate,
	RenderBackendTextureAddressMode mode)
{
	DX9Backend::Set_DX9_Texture_Stage_State(stage,
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
	DX9Backend::Set_DX9_Texture_Stage_State(stage, state, To_D3D_Texture_Filter(filter));
}

void DX9Backend::Set_Texture_Max_Anisotropy(unsigned stage, unsigned level)
{
	DX9Backend::Set_DX9_Texture_Stage_State(stage, D3DTSS_MAXANISOTROPY, level);
}

void DX9Backend::Set_Texture_Bump_Environment_Matrix(unsigned stage,
	float m00, float m01, float m10, float m11, float scale, float offset)
{
	DX9Backend::Set_DX9_Texture_Stage_State(stage, D3DTSS_BUMPENVMAT00, Float_To_Dword(m00));
	DX9Backend::Set_DX9_Texture_Stage_State(stage, D3DTSS_BUMPENVMAT01, Float_To_Dword(m01));
	DX9Backend::Set_DX9_Texture_Stage_State(stage, D3DTSS_BUMPENVMAT10, Float_To_Dword(m10));
	DX9Backend::Set_DX9_Texture_Stage_State(stage, D3DTSS_BUMPENVMAT11, Float_To_Dword(m11));
	DX9Backend::Set_DX9_Texture_Stage_State(stage, D3DTSS_BUMPENVLSCALE, Float_To_Dword(scale));
	DX9Backend::Set_DX9_Texture_Stage_State(stage, D3DTSS_BUMPENVLOFFSET, Float_To_Dword(offset));
}



void DX9Backend::Set_Render_Target(TextureClass * render_target, ZTextureClass * depth_target)
{
	if (render_target != nullptr)
	{
		DX9Backend::Set_Render_Target_With_Z(render_target, depth_target);
	}
	else
	{
		DX9Backend::Set_Render_Target(static_cast<IDirect3DSurface9 *>(nullptr));
	}
}

RenderBackendSurface *DX9Backend::Create_System_Memory_Surface(unsigned width,
	unsigned height, WW3DFormat format)
{
	IDirect3DSurface9 *surface = DX9Backend::_Create_DX9_Surface(width, height, format);
	return surface != nullptr ? new DX9BackendSurface(surface) : nullptr;
}

RenderBackendSurface *DX9Backend::Create_Surface_From_File(const char * filename)
{
	IDirect3DSurface9 *surface = DX9Backend::_Create_DX9_Surface(filename);
	return surface != nullptr ? new DX9BackendSurface(surface) : nullptr;
}

bool DX9Backend::Get_Surface_Description(RenderBackendSurface * surface,
	RenderBackendSurfaceDescription & description) const
{
	DX9BackendSurface *dx9_surface = To_DX9_Surface(surface);
	if (dx9_surface == nullptr || dx9_surface->Surface == nullptr)
	{
		return false;
	}

	D3DSURFACE_DESC native_description{};
	if (FAILED(dx9_surface->Surface->GetDesc(&native_description)))
	{
		return false;
	}

	description.format = WW3D_Format_From_DX9(native_description.Format);
	description.width = native_description.Width;
	description.height = native_description.Height;
	return true;
}

bool DX9Backend::Lock_Surface(RenderBackendSurface * surface,
	RenderBackendLockedSurface & locked_surface,
	const RenderBackendRect * rect,
	RenderBackendSurfaceLockMode mode)
{
	DX9BackendSurface *dx9_surface = To_DX9_Surface(surface);
	if (dx9_surface == nullptr || dx9_surface->Surface == nullptr)
	{
		return false;
	}

	D3DLOCKED_RECT d3d_locked_surface = {};
	RECT d3d_rect{};
	const RECT *lock_rect = nullptr;
	if (rect != nullptr)
	{
		d3d_rect.left = rect->left;
		d3d_rect.top = rect->top;
		d3d_rect.right = rect->right;
		d3d_rect.bottom = rect->bottom;
		lock_rect = &d3d_rect;
	}
	// Writable locks must leave managed textures dirty so Direct3D copies the
	// updated contents to the device.  D3DLOCK_NO_DIRTY_UPDATE is appropriate
	// only when the caller promises that the resource was not modified; using it
	// for the default read/write path prevents video frames and other CPU-written
	// texture data from reaching the GPU.
	const DWORD lock_flags = mode == RenderBackendSurfaceLockMode::ReadOnly ?
		D3DLOCK_READONLY : 0;
	const HRESULT result = dx9_surface->Surface->LockRect(
		&d3d_locked_surface, lock_rect, lock_flags);
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

SurfaceClass *DX9Backend::Create_Surface(unsigned width, unsigned height, WW3DFormat format)
{
	RenderBackendSurface *surface = Create_System_Memory_Surface(width, height, format);
	return surface != nullptr ? new SurfaceClass(surface) : nullptr;
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
	DX9Backend::_Copy_DX9_Rects(
		dx9_source->Surface,
		&d3d_source_rect,
		1,
		Get_DX9_Surface(destination),
		&d3d_destination_point);
}

bool DX9Backend::Copy_Surface_Rect(SurfaceClass * source,
	const RenderBackendRect & source_rect,
	RenderBackendSurface * destination,
	const RenderBackendPoint & destination_point)
{
	DX9BackendSurface *dx9_destination = To_DX9_Surface(destination);
	IDirect3DDevice9 *device = DX9Backend::_Get_D3D_Device();
	if (source == nullptr || Get_DX9_Surface(source) == nullptr ||
		dx9_destination == nullptr || dx9_destination->Surface == nullptr ||
		device == nullptr)
	{
		return false;
	}

	IDirect3DSurface9 *source_surface = Get_DX9_Surface(source);
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

bool DX9Backend::Copy_Surface_Stretch(SurfaceClass * source,
	const RenderBackendRect & source_rect, SurfaceClass * destination,
	const RenderBackendRect & destination_rect)
{
	if (source == nullptr || destination == nullptr)
	{
		return false;
	}

	DX9BackendSurface *dx9_source = To_DX9_Surface(source->Get_Render_Backend_Surface());
	DX9BackendSurface *dx9_destination = To_DX9_Surface(destination->Get_Render_Backend_Surface());
	if (dx9_source == nullptr || dx9_source->Surface == nullptr ||
		dx9_destination == nullptr || dx9_destination->Surface == nullptr)
	{
		return false;
	}

	const RECT d3d_source_rect{source_rect.left, source_rect.top,
		source_rect.right, source_rect.bottom};
	const RECT d3d_destination_rect{destination_rect.left, destination_rect.top,
		destination_rect.right, destination_rect.bottom};
	return SUCCEEDED(D3DXLoadSurfaceFromSurface(dx9_destination->Surface,
		nullptr, &d3d_destination_rect, dx9_source->Surface, nullptr,
		&d3d_source_rect, D3DX_FILTER_TRIANGLE, 0));
}

bool DX9Backend::Copy_Surface(SurfaceClass * source, SurfaceClass * destination)
{
	if (source == nullptr || destination == nullptr)
		return false;
	_Copy_DX9_Rects(Get_DX9_Surface(source), nullptr, 0,
		Get_DX9_Surface(destination), nullptr);
	return true;
}

bool DX9Backend::Copy_Surface_Rect(SurfaceClass * source, const RenderBackendRect & source_rect,
	SurfaceClass * destination, const RenderBackendPoint & destination_point)
{
	if (source == nullptr || destination == nullptr)
		return false;
	const RECT rect{source_rect.left, source_rect.top, source_rect.right, source_rect.bottom};
	const POINT point{destination_point.x, destination_point.y};
	_Copy_DX9_Rects(Get_DX9_Surface(source), &rect, 1,
		Get_DX9_Surface(destination), &point);
	return true;
}

int DX9Backend::Read_Back_Buffer_Rect(void * buffer, int buffer_size, int x, int y, int width, int height)
{
	if (buffer == nullptr || buffer_size <= 0 || D3DDevice == nullptr)
		return 0;
	IDirect3DSurface9 *source = nullptr;
	IDirect3DSurface9 *staging = nullptr;
	if (FAILED(D3DDevice->GetRenderTarget(0, &source)) || source == nullptr)
		return 0;
	D3DSURFACE_DESC desc;
	const HRESULT desc_hr = source->GetDesc(&desc);
	const HRESULT create_hr = SUCCEEDED(desc_hr) ? D3DDevice->CreateOffscreenPlainSurface(width, height, desc.Format, D3DPOOL_SCRATCH, &staging, nullptr) : E_FAIL;
	if (FAILED(create_hr)) { source->Release(); return 0; }
	RECT rect{x, y, x + width, y + height};
	POINT point{0, 0};
	int result = 0;
	if (SUCCEEDED(D3DDevice->UpdateSurface(source, &rect, staging, &point)))
	{
		D3DLOCKED_RECT locked;
		if (SUCCEEDED(staging->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
		{
			result = min(buffer_size, static_cast<int>(locked.Pitch * height));
			memcpy(buffer, locked.pBits, result);
			staging->UnlockRect();
		}
	}
	staging->Release();
	source->Release();
	return result;
}

uintptr_t DX9Backend::Create_Transient_Render_Texture(unsigned width, unsigned height, WW3DFormat format)
{
	if (D3DDevice == nullptr)
		return 0;
	IDirect3DTexture9 *texture = nullptr;
	if (FAILED(D3DDevice->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET,
		DX9_Format_From_WW3D(format), D3DPOOL_DEFAULT, &texture, nullptr)))
		return 0;
	return reinterpret_cast<uintptr_t>(texture);
}

bool DX9Backend::Copy_Back_Buffer_To_Texture(uintptr_t texture)
{
	if (D3DDevice == nullptr || texture == 0)
		return false;
	IDirect3DSurface9 *source = nullptr;
	IDirect3DSurface9 *destination = nullptr;
	IDirect3DTexture9 *target = reinterpret_cast<IDirect3DTexture9 *>(texture);
	if (FAILED(D3DDevice->GetRenderTarget(0, &source)) || FAILED(target->GetSurfaceLevel(0, &destination)))
	{
		if (source) source->Release();
		return false;
	}
	_Copy_DX9_Rects(source, nullptr, 0, destination, nullptr);
	destination->Release();
	source->Release();
	return true;
}

bool DX9Backend::Copy_Texture_To_Surface(uintptr_t texture, SurfaceClass * destination)
{
	if (texture == 0 || destination == nullptr)
		return false;
	IDirect3DSurface9 *source = nullptr;
	IDirect3DTexture9 *texture_object = reinterpret_cast<IDirect3DTexture9 *>(texture);
	if (FAILED(texture_object->GetSurfaceLevel(0, &source)))
		return false;
	_Copy_DX9_Rects(source, nullptr, 0, Get_DX9_Surface(destination), nullptr);
	source->Release();
	return true;
}

bool DX9Backend::Copy_Render_Target_To_Surface(TextureClass * source, SurfaceClass * destination)
{
	if (source == nullptr || destination == nullptr)
		return false;

	SurfaceClass *source_surface = source->Get_Surface_Level(0);
	if (source_surface == nullptr)
		return false;

	SurfaceClass::SurfaceDescription source_description;
	SurfaceClass::SurfaceDescription destination_description;
	source_surface->Get_Description(source_description);
	destination->Get_Description(destination_description);
	const unsigned copy_width = std::min(source_description.Width, destination_description.Width);
	const unsigned copy_height = std::min(source_description.Height, destination_description.Height);
	if (copy_width == 0 || copy_height == 0)
	{
		REF_PTR_RELEASE(source_surface);
		return false;
	}

	const RenderBackendRect source_rect{0, 0,
		static_cast<int>(copy_width), static_cast<int>(copy_height)};
	const RenderBackendPoint destination_point{0, 0};
	const bool result = Copy_Surface_Rect(source_surface, source_rect,
		destination->Get_Render_Backend_Surface(), destination_point);
	REF_PTR_RELEASE(source_surface);
	return result;
}

void DX9Backend::Release_Transient_Render_Texture(uintptr_t texture)
{
	if (texture != 0)
		reinterpret_cast<IDirect3DTexture9 *>(texture)->Release();
}

uintptr_t DX9Backend::Create_Texture_Handle(unsigned width, unsigned height, WW3DFormat format,
	unsigned mip_levels, bool dynamic, bool render_target)
{
	const D3DPOOL pool = dynamic || render_target ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
	return reinterpret_cast<uintptr_t>(_Create_DX9_Texture(width, height, format,
		static_cast<MipCountType>(mip_levels), pool, render_target));
}

uintptr_t DX9Backend::Create_ZTexture_Handle(unsigned width, unsigned height, WW3DZFormat format,
	unsigned mip_levels)
{
	return reinterpret_cast<uintptr_t>(_Create_DX9_ZTexture(width, height, format,
		static_cast<MipCountType>(mip_levels), D3DPOOL_DEFAULT));
}

uintptr_t DX9Backend::Create_Surface_Handle(unsigned width, unsigned height, WW3DFormat format)
{
	return reinterpret_cast<uintptr_t>(_Create_DX9_Surface(width, height, format));
}

uintptr_t DX9Backend::Create_Surface_Handle(const char *filename)
{
	return reinterpret_cast<uintptr_t>(_Create_DX9_Surface(filename));
}

static D3DPOOL To_DX9_Pool(RenderBackendTexturePool pool)
{
	switch (pool)
	{
	case RenderBackendTexturePool::Default: return D3DPOOL_DEFAULT;
	case RenderBackendTexturePool::Managed: return D3DPOOL_MANAGED;
	case RenderBackendTexturePool::SystemMemory: return D3DPOOL_SYSTEMMEM;
	default: return D3DPOOL_MANAGED;
	}
}

uintptr_t DX9Backend::Create_Texture_Handle_Pooled(unsigned width, unsigned height, WW3DFormat format,
	unsigned mip_levels, RenderBackendTexturePool pool, bool render_target)
{
	return reinterpret_cast<uintptr_t>(_Create_DX9_Texture(width, height, format,
		static_cast<MipCountType>(mip_levels), To_DX9_Pool(pool), render_target));
}

uintptr_t DX9Backend::Create_ZTexture_Handle_Pooled(unsigned width, unsigned height, WW3DZFormat format,
	unsigned mip_levels, RenderBackendTexturePool pool)
{
	return reinterpret_cast<uintptr_t>(_Create_DX9_ZTexture(width, height, format,
		static_cast<MipCountType>(mip_levels), To_DX9_Pool(pool)));
}

uintptr_t DX9Backend::Create_Cube_Texture_Handle(unsigned width, unsigned height, WW3DFormat format,
	unsigned mip_levels, RenderBackendTexturePool pool, bool render_target)
{
	return reinterpret_cast<uintptr_t>(_Create_DX9_Cube_Texture(width, height, format,
		static_cast<MipCountType>(mip_levels), To_DX9_Pool(pool), render_target));
}

uintptr_t DX9Backend::Create_Volume_Texture_Handle(unsigned width, unsigned height, unsigned depth,
	WW3DFormat format, unsigned mip_levels, RenderBackendTexturePool pool)
{
	return reinterpret_cast<uintptr_t>(_Create_DX9_Volume_Texture(width, height, depth, format,
		static_cast<MipCountType>(mip_levels), To_DX9_Pool(pool)));
}

RenderBackendTextureHandle DX9Backend::Create_Texture_From_Surface(RenderBackendSurface * surface,
	unsigned mip_levels)
{
	DX9BackendSurface *backend_surface = To_DX9_Surface(surface);
	if (backend_surface == nullptr)
	{
		return 0;
	}
	return reinterpret_cast<uintptr_t>(_Create_DX9_Texture(
		backend_surface->Surface, static_cast<MipCountType>(mip_levels)));
}

uintptr_t DX9Backend::Create_Texture_From_File_Handle(const char *filename, unsigned mip_levels)
{
	return reinterpret_cast<uintptr_t>(_Create_DX9_Texture(filename, static_cast<MipCountType>(mip_levels)));
}

SurfaceClass *DX9Backend::Get_Texture_Surface_Level(uintptr_t texture, unsigned level)
{
	if (texture == 0)
		return nullptr;

	IDirect3DSurface9 *surface = nullptr;
	IDirect3DTexture9 *texture_object = reinterpret_cast<IDirect3DTexture9 *>(texture);
	const HRESULT result = texture_object->GetSurfaceLevel(level, &surface);
	DX9_ErrorCode(result);
	if (FAILED(result) || surface == nullptr)
		return nullptr;

	return new SurfaceClass(new DX9BackendSurface(surface));
}

RenderBackendTextureHandle DX9Backend::Add_Texture_Reference(RenderBackendTextureHandle texture)
{
	if (texture != 0)
	{
		reinterpret_cast<IDirect3DBaseTexture9 *>(texture)->AddRef();
	}
	return texture;
}

void DX9Backend::Release_Texture_Handle(RenderBackendTextureHandle texture)
{
	if (texture != 0)
	{
		reinterpret_cast<IDirect3DBaseTexture9 *>(texture)->Release();
	}
}

unsigned DX9Backend::Get_Texture_Level_Count(RenderBackendTextureHandle texture) const
{
	if (texture == 0)
	{
		return 0;
	}
	return reinterpret_cast<IDirect3DBaseTexture9 *>(texture)->GetLevelCount();
}

bool DX9Backend::Get_Texture_Description(RenderBackendTextureHandle texture, unsigned level,
	RenderBackendTextureDescription & description) const
{
	description = RenderBackendTextureDescription();
	if (texture == 0)
	{
		return false;
	}

	IDirect3DBaseTexture9 *base_texture = reinterpret_cast<IDirect3DBaseTexture9 *>(texture);
	description.mip_levels = base_texture->GetLevelCount();
	if (level >= description.mip_levels)
	{
		return false;
	}

	if (base_texture->GetType() == D3DRTYPE_VOLUMETEXTURE)
	{
		D3DVOLUME_DESC volume_description = {};
		if (FAILED(reinterpret_cast<IDirect3DVolumeTexture9 *>(base_texture)->GetLevelDesc(
			level, &volume_description)))
		{
			return false;
		}
		description.kind = RenderBackendTextureKind::Volume;
		description.format = WW3D_Format_From_DX9(volume_description.Format);
		description.depth_format = WW3DZ_Format_From_DX9(volume_description.Format);
		description.width = volume_description.Width;
		description.height = volume_description.Height;
		description.depth = volume_description.Depth;
		return true;
	}

	D3DSURFACE_DESC surface_description = {};
	HRESULT result = E_FAIL;
	if (base_texture->GetType() == D3DRTYPE_CUBETEXTURE)
	{
		description.kind = RenderBackendTextureKind::Cube;
		result = reinterpret_cast<IDirect3DCubeTexture9 *>(base_texture)->GetLevelDesc(
			level, &surface_description);
	}
	else if (base_texture->GetType() == D3DRTYPE_TEXTURE)
	{
		result = reinterpret_cast<IDirect3DTexture9 *>(base_texture)->GetLevelDesc(
			level, &surface_description);
	}

	if (FAILED(result))
	{
		return false;
	}

	description.format = WW3D_Format_From_DX9(surface_description.Format);
	description.depth_format = WW3DZ_Format_From_DX9(surface_description.Format);
	if (description.depth_format != WW3D_ZFORMAT_UNKNOWN)
	{
		description.kind = RenderBackendTextureKind::DepthStencil;
	}
	description.width = surface_description.Width;
	description.height = surface_description.Height;
	description.depth = 1;
	return true;
}

bool DX9Backend::Lock_Texture(RenderBackendTextureHandle texture, unsigned level,
	RenderBackendTextureLock & locked_texture, bool read_only)
{
	locked_texture = RenderBackendTextureLock();
	if (texture == 0)
	{
		return false;
	}

	D3DLOCKED_RECT locked_rect = {};
	const DWORD flags = read_only ? D3DLOCK_READONLY : 0;
	const HRESULT result = reinterpret_cast<IDirect3DTexture9 *>(texture)->LockRect(
		level, &locked_rect, nullptr, flags);
	if (FAILED(result))
	{
		DX9_ErrorCode(result);
		return false;
	}
	locked_texture.bits = locked_rect.pBits;
	locked_texture.row_pitch = static_cast<unsigned>(locked_rect.Pitch);
	locked_texture.slice_pitch = locked_texture.row_pitch;
	return true;
}

void DX9Backend::Unlock_Texture(RenderBackendTextureHandle texture, unsigned level)
{
	if (texture != 0)
	{
		DX9_ErrorCode(reinterpret_cast<IDirect3DTexture9 *>(texture)->UnlockRect(level));
	}
}

static D3DCUBEMAP_FACES To_DX9_Cube_Face(RenderBackendCubeFace face)
{
	switch (face)
	{
	case RenderBackendCubeFace::PositiveX: return D3DCUBEMAP_FACE_POSITIVE_X;
	case RenderBackendCubeFace::NegativeX: return D3DCUBEMAP_FACE_NEGATIVE_X;
	case RenderBackendCubeFace::PositiveY: return D3DCUBEMAP_FACE_POSITIVE_Y;
	case RenderBackendCubeFace::NegativeY: return D3DCUBEMAP_FACE_NEGATIVE_Y;
	case RenderBackendCubeFace::PositiveZ: return D3DCUBEMAP_FACE_POSITIVE_Z;
	case RenderBackendCubeFace::NegativeZ: return D3DCUBEMAP_FACE_NEGATIVE_Z;
	default: return D3DCUBEMAP_FACE_POSITIVE_X;
	}
}

bool DX9Backend::Lock_Cube_Texture(RenderBackendTextureHandle texture, RenderBackendCubeFace face,
	unsigned level, RenderBackendTextureLock & locked_texture, bool read_only)
{
	locked_texture = RenderBackendTextureLock();
	if (texture == 0)
	{
		return false;
	}

	D3DLOCKED_RECT locked_rect = {};
	const DWORD flags = read_only ? D3DLOCK_READONLY : 0;
	const HRESULT result = reinterpret_cast<IDirect3DCubeTexture9 *>(texture)->LockRect(
		To_DX9_Cube_Face(face), level, &locked_rect, nullptr, flags);
	if (FAILED(result))
	{
		DX9_ErrorCode(result);
		return false;
	}
	locked_texture.bits = locked_rect.pBits;
	locked_texture.row_pitch = static_cast<unsigned>(locked_rect.Pitch);
	locked_texture.slice_pitch = locked_texture.row_pitch;
	return true;
}

void DX9Backend::Unlock_Cube_Texture(RenderBackendTextureHandle texture, RenderBackendCubeFace face,
	unsigned level)
{
	if (texture != 0)
	{
		DX9_ErrorCode(reinterpret_cast<IDirect3DCubeTexture9 *>(texture)->UnlockRect(
			To_DX9_Cube_Face(face), level));
	}
}

bool DX9Backend::Lock_Volume_Texture(RenderBackendTextureHandle texture, unsigned level,
	RenderBackendTextureLock & locked_texture, bool read_only)
{
	locked_texture = RenderBackendTextureLock();
	if (texture == 0)
	{
		return false;
	}

	D3DLOCKED_BOX locked_box = {};
	const DWORD flags = read_only ? D3DLOCK_READONLY : 0;
	const HRESULT result = reinterpret_cast<IDirect3DVolumeTexture9 *>(texture)->LockBox(
		level, &locked_box, nullptr, flags);
	if (FAILED(result))
	{
		DX9_ErrorCode(result);
		return false;
	}
	locked_texture.bits = locked_box.pBits;
	locked_texture.row_pitch = static_cast<unsigned>(locked_box.RowPitch);
	locked_texture.slice_pitch = static_cast<unsigned>(locked_box.SlicePitch);
	return true;
}

void DX9Backend::Unlock_Volume_Texture(RenderBackendTextureHandle texture, unsigned level)
{
	if (texture != 0)
	{
		DX9_ErrorCode(reinterpret_cast<IDirect3DVolumeTexture9 *>(texture)->UnlockBox(level));
	}
}

bool DX9Backend::Update_Texture(RenderBackendTextureHandle source,
	RenderBackendTextureHandle destination)
{
	if (D3DDevice == nullptr || source == 0 || destination == 0)
	{
		return false;
	}
	const HRESULT result = D3DDevice->UpdateTexture(
		reinterpret_cast<IDirect3DBaseTexture9 *>(source),
		reinterpret_cast<IDirect3DBaseTexture9 *>(destination));
	DX9_ErrorCode(result);
	return SUCCEEDED(result);
}

bool DX9Backend::Generate_Texture_Mipmaps(RenderBackendTextureHandle texture)
{
	if (texture == 0)
	{
		return false;
	}

	const HRESULT result = D3DXFilterTexture(
		reinterpret_cast<IDirect3DBaseTexture9 *>(texture), nullptr, 0, D3DX_FILTER_BOX);
	DX9_ErrorCode(result);
	return SUCCEEDED(result);
}

void DX9Backend::Set_Texture_LOD(RenderBackendTextureHandle texture, unsigned lod)
{
	if (texture != 0)
	{
		reinterpret_cast<IDirect3DBaseTexture9 *>(texture)->SetLOD(lod);
	}
}

unsigned DX9Backend::Get_Texture_Priority(RenderBackendTextureHandle texture) const
{
	return texture == 0 ? 0 : reinterpret_cast<IDirect3DBaseTexture9 *>(texture)->GetPriority();
}

unsigned DX9Backend::Set_Texture_Priority(RenderBackendTextureHandle texture, unsigned priority)
{
	if (texture == 0)
	{
		return 0;
	}
	return reinterpret_cast<IDirect3DBaseTexture9 *>(texture)->SetPriority(priority);
}

bool DX9Backend::Is_Missing_Texture_Handle(RenderBackendTextureHandle texture) const
{
	if (texture == 0)
	{
		return false;
	}
	RenderBackendTextureHandle missing = MissingTexture::_Get_Missing_Texture();
	const bool is_missing = texture == missing;
	if (missing != 0)
	{
		reinterpret_cast<IDirect3DBaseTexture9 *>(missing)->Release();
	}
	return is_missing;
}

RenderBackendTextureHandle DX9Backend::Create_Missing_Texture()
{
	IDirect3DTexture9 *texture = _Create_DX9_Texture(128, 128, WW3D_FORMAT_A8R8G8B8,
		MIP_LEVELS_ALL, D3DPOOL_MANAGED, false);
	if (texture == nullptr)
	{
		return 0;
	}

	D3DLOCKED_RECT locked_rect = {};
	const HRESULT lock_result = texture->LockRect(0, &locked_rect, nullptr, 0);
	if (FAILED(lock_result))
	{
		DX9_ErrorCode(lock_result);
		texture->Release();
		return 0;
	}
	for (unsigned y = 0; y < 128; ++y)
	{
		unsigned *row = reinterpret_cast<unsigned *>(
			static_cast<unsigned char *>(locked_rect.pBits) + y * locked_rect.Pitch);
		for (unsigned x = 0; x < 128; ++x)
		{
			row[x] = 0x7FFF00FF;
		}
	}
	DX9_ErrorCode(texture->UnlockRect(0));

	for (unsigned level = 1; level < texture->GetLevelCount(); ++level)
	{
		IDirect3DSurface9 *source = nullptr;
		IDirect3DSurface9 *destination = nullptr;
		if (FAILED(texture->GetSurfaceLevel(level - 1, &source)) ||
			FAILED(texture->GetSurfaceLevel(level, &destination)))
		{
			if (source != nullptr) source->Release();
			if (destination != nullptr) destination->Release();
			texture->Release();
			return 0;
		}
		const HRESULT mip_result = D3DXLoadSurfaceFromSurface(destination, nullptr, nullptr,
			source, nullptr, nullptr, D3DX_FILTER_BOX, 0);
		DX9_ErrorCode(mip_result);
		source->Release();
		destination->Release();
		if (FAILED(mip_result))
		{
			texture->Release();
			return 0;
		}
	}

	return reinterpret_cast<RenderBackendTextureHandle>(texture);
}

RenderBackendSurface *DX9Backend::Create_Missing_Surface()
{
	RenderBackendTextureHandle missing = MissingTexture::_Get_Missing_Texture();
	if (missing == 0 || D3DDevice == nullptr)
	{
		if (missing != 0) Release_Texture_Handle(missing);
		return nullptr;
	}

	IDirect3DSurface9 *source = nullptr;
	IDirect3DSurface9 *surface = nullptr;
	IDirect3DTexture9 *texture = reinterpret_cast<IDirect3DTexture9 *>(missing);
	HRESULT result = texture->GetSurfaceLevel(0, &source);
	D3DSURFACE_DESC description = {};
	if (SUCCEEDED(result)) result = source->GetDesc(&description);
	if (SUCCEEDED(result))
	{
		result = D3DDevice->CreateOffscreenPlainSurface(description.Width, description.Height,
			description.Format, D3DPOOL_SCRATCH, &surface, nullptr);
	}
	if (SUCCEEDED(result)) result = D3DDevice->UpdateSurface(source, nullptr, surface, nullptr);
	if (source != nullptr) source->Release();
	Release_Texture_Handle(missing);
	if (FAILED(result) || surface == nullptr)
	{
		if (surface != nullptr) surface->Release();
		DX9_ErrorCode(result);
		return nullptr;
	}
	return new DX9BackendSurface(surface);
}

void DX9Backend::Register_Texture(TextureBaseClass * texture, RenderBackendTextureKind kind,
	unsigned width, unsigned height, unsigned depth, WW3DFormat format,
	WW3DZFormat depth_format, unsigned mip_levels, bool render_target)
{
	DX9TextureManagerClass::Register(texture, kind, width, height, depth, format,
		depth_format, static_cast<MipCountType>(mip_levels), render_target);
}

void DX9Backend::Unregister_Texture(TextureBaseClass * texture)
{
	DX9TextureManagerClass::Remove(texture);
}

bool DX9Backend::Get_Texture_Limits(RenderBackendTextureLimits & limits) const
{
	const D3DCAPS9 &caps = Get_Current_Caps()->Get_DX9_Caps();
	limits.max_width = caps.MaxTextureWidth;
	limits.max_height = caps.MaxTextureHeight;
	limits.max_volume_extent = caps.MaxVolumeExtent;
	limits.max_aspect_ratio = caps.MaxTextureAspectRatio;
	return true;
}

RenderBackendVertexBuffer *DX9Backend::Create_Vertex_Buffer(unsigned size_bytes,
	const RenderBackendVertexLayout &layout, unsigned usage)
{
	IDirect3DDevice9 *device = DX9Backend::_Get_D3D_Device();
	if (device == nullptr || size_bytes == 0)
	{
		return nullptr;
	}

	IDirect3DVertexBuffer9 *buffer = nullptr;
	const bool dynamic = (usage & BUFFER_USAGE_DYNAMIC) != 0;
	DWORD d3d_usage = To_D3D_Usage(usage);
	// The legacy buffer path explicitly marked buffers for software vertex
	// processing when the device did not expose hardware T&L. Keep that
	// contract in the concrete backend; terrain uses fixed-function buffers
	// and otherwise becomes invalid on software-VP D3D9 devices.
	if (!Supports_TnL())
	{
		d3d_usage |= D3DUSAGE_SOFTWAREPROCESSING;
	}
	const D3DPOOL pool = dynamic ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
	if (FAILED(device->CreateVertexBuffer(size_bytes, d3d_usage,
		To_D3D_Vertex_Format(layout), pool, &buffer, nullptr)))
	{
		return nullptr;
	}

	return new DX9BackendVertexBuffer(buffer, layout);
}

RenderBackendIndexBuffer *DX9Backend::Create_Index_Buffer(unsigned size_bytes,
	unsigned usage)
{
	IDirect3DDevice9 *device = DX9Backend::_Get_D3D_Device();
	if (device == nullptr || size_bytes == 0)
	{
		return nullptr;
	}

	IDirect3DIndexBuffer9 *buffer = nullptr;
	const bool dynamic = (usage & BUFFER_USAGE_DYNAMIC) != 0;
	DWORD d3d_usage = To_D3D_Usage(usage);
	if (!Supports_TnL())
	{
		d3d_usage |= D3DUSAGE_SOFTWAREPROCESSING;
	}
	const D3DPOOL pool = dynamic ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
	if (FAILED(device->CreateIndexBuffer(size_bytes, d3d_usage, D3DFMT_INDEX16,
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
	IDirect3DDevice9 *device = DX9Backend::_Get_D3D_Device();
	if (device == nullptr)
	{
		return;
	}

	DX9BackendVertexBuffer *dx9_buffer = To_DX9_Vertex_Buffer(buffer);
	device->SetStreamSource(stream,
		dx9_buffer != nullptr ? dx9_buffer->Buffer : nullptr,
		dx9_buffer != nullptr ? offset_bytes : 0,
		dx9_buffer != nullptr ? stride_bytes : 0);
	if (dx9_buffer != nullptr &&
		(Vertex_Shader == 0 || Vertex_Shader < 0x10000 ||
			(Vertex_Shader & D3DFVF_RESERVED0) != 0))
	{
		// Rigid and dynamic legacy buffers use the fixed-function vertex path.
		// Do not do this when a caller has selected a programmable vertex shader:
		// its register inputs are part of the shader contract, not the buffer's
		// native FVF.
		const DWORD vertex_format = To_D3D_Vertex_Format(dx9_buffer->Layout);
		Vertex_Shader = vertex_format;
		Release_Vertex_Shader_Input_Layout();
		device->SetVertexShader(nullptr);
		device->SetFVF(vertex_format);
	}
}

void DX9Backend::Set_Index_Buffer(RenderBackendIndexBuffer * buffer)
{
	IDirect3DDevice9 *device = DX9Backend::_Get_D3D_Device();
	if (device == nullptr)
	{
		return;
	}

	DX9BackendIndexBuffer *dx9_buffer = To_DX9_Index_Buffer(buffer);
	device->SetIndices(dx9_buffer != nullptr ? dx9_buffer->Buffer : nullptr);
}

void DX9Backend::Set_Vertex_Format(RenderBackendVertexFormat format)
{
	IDirect3DDevice9 *device = DX9Backend::_Get_D3D_Device();
	if (device != nullptr)
	{
		const DWORD vertex_format = To_D3D_Vertex_Format(format);
		// Set_Vertex_Format selects the fixed-function vertex declaration. Keep
		// the backend cache in the same state so a previously bound custom
		// vertex shader cannot be restored by a later state application.
		Vertex_Shader = vertex_format;
		Release_Vertex_Shader_Input_Layout();
		device->SetVertexShader(nullptr);
		device->SetFVF(vertex_format);
	}
}

bool DX9Backend::Process_Vertices(VertexBufferClass * destination, unsigned vertex_count)
{
	IDirect3DDevice9 *device = DX9Backend::_Get_D3D_Device();
	RenderBackendVertexBuffer *destination_buffer = destination != nullptr ?
		destination->Get_Backend_Buffer() : nullptr;
	DX9BackendVertexBuffer *dx9_destination = To_DX9_Vertex_Buffer(destination_buffer);
	return device != nullptr && dx9_destination != nullptr &&
		SUCCEEDED(device->ProcessVertices(0, 0, vertex_count,
			dx9_destination->Buffer, nullptr, 0));
}

void DX9Backend::Draw_Indexed_Primitives(RenderBackendPrimitiveType primitive_type,
	unsigned base_vertex_index, unsigned min_vertex_index,
	unsigned vertex_count, unsigned start_index, unsigned primitive_count)
{
	IDirect3DDevice9 *device = DX9Backend::_Get_D3D_Device();
	if (device != nullptr)
	{
		Apply_Render_State_Changes();
		if (!DX9Backend::_Is_Triangle_Draw_Enabled())
		{
			return;
		}
		device->DrawIndexedPrimitive(To_D3D_Primitive_Type(primitive_type),
			base_vertex_index, min_vertex_index, vertex_count, start_index,
			primitive_count);
	}
}

void DX9Backend::Draw_Primitive_Up(RenderBackendPrimitiveType primitive_type,
	unsigned primitive_count, const void * vertices, unsigned stride_bytes,
	RenderBackendVertexFormat format)
{
	IDirect3DDevice9 *device = DX9Backend::_Get_D3D_Device();
	if (device != nullptr && vertices != nullptr)
	{
		Set_Vertex_Format(format);
		device->DrawPrimitiveUP(To_D3D_Primitive_Type(primitive_type), primitive_count,
			vertices, stride_bytes);
	}
}

void DX9Backend::Draw_Primitive(RenderBackendPrimitiveType primitive_type,
	unsigned start_vertex, unsigned primitive_count)
{
	if (D3DDevice != nullptr)
		D3DDevice->DrawPrimitive(To_D3D_Primitive_Type(primitive_type), start_vertex, primitive_count);
}







void DX9Backend::Set_Transform(RenderBackendTransform transform, const Matrix4x4 & matrix)
{
	DX9Backend::Set_Transform(To_D3D_Transform(transform), matrix);
}

void DX9Backend::Set_Transform(RenderBackendTransform transform, const Matrix3D & matrix)
{
	DX9Backend::Set_Transform(To_D3D_Transform(transform), matrix);
}

void DX9Backend::Get_Transform(RenderBackendTransform transform, Matrix4x4 & matrix)
{
	DX9Backend::Get_Transform(To_D3D_Transform(transform), matrix);
}

void DX9Backend::Set_Transform(RenderBackendTransform transform, const float * matrix_elements)
{
	if (matrix_elements == nullptr) return;
	D3DMATRIX matrix;
	std::memcpy(&matrix, matrix_elements, sizeof(matrix));
	_Set_DX9_Transform(To_D3D_Transform(transform), matrix);
}

void DX9Backend::Get_Transform(RenderBackendTransform transform, float * matrix_elements)
{
	if (matrix_elements == nullptr) return;
	D3DMATRIX matrix;
	_Get_DX9_Transform(To_D3D_Transform(transform), matrix);
	std::memcpy(matrix_elements, &matrix, sizeof(matrix));
}

void DX9Backend::Set_Render_State(unsigned state, unsigned value)
{
	Set_DX9_Render_State(static_cast<D3DRENDERSTATETYPE>(state), value);
}

unsigned DX9Backend::Get_Render_State(unsigned state) const
{
	return Get_DX9_Render_State(static_cast<D3DRENDERSTATETYPE>(state));
}

void DX9Backend::Set_Texture_Stage_State(unsigned stage, unsigned state, unsigned value)
{
	Set_DX9_Texture_Stage_State(stage, static_cast<D3DTEXTURESTAGESTATETYPE>(state), value);
}












void DX9Backend::Set_Texture_Resource(unsigned stage, const TextureBaseClass * texture)
{
	if (texture != nullptr)
	{
		// Custom shader paths bind the resource directly and therefore bypass TextureClass::Apply.
		// Give the texture abstraction a chance to recreate an evicted resource before querying
		// the opaque handle. No caller needs to know which backend owns that resource.
		const_cast<TextureBaseClass *>(texture)->Ensure_Render_Backend_Texture();
	}

	IDirect3DBaseTexture9 *resource = texture != nullptr ? reinterpret_cast<IDirect3DBaseTexture9 *>(
		texture->Peek_Render_Backend_Texture()) : nullptr;
	DX9Backend::Set_DX9_Texture(stage, resource);
}

void DX9Backend::Set_Texture_Handle(unsigned stage, uintptr_t texture)
{
	DX9Backend::Set_DX9_Texture(stage,
		reinterpret_cast<IDirect3DBaseTexture9 *>(texture));
}

SurfaceClass * DX9Backend::Get_Back_Buffer_Surface()
{
	return _Get_DX9_Back_Buffer();
}

int DX9Backend::Get_Pixel_Shader_Major_Version() const
{
	return CurrentCaps != nullptr ? CurrentCaps->Get_Pixel_Shader_Major_Version() : 0;
}

int DX9Backend::Get_Pixel_Shader_Minor_Version() const
{
	return CurrentCaps != nullptr ? CurrentCaps->Get_Pixel_Shader_Minor_Version() : 0;
}



void DX9Backend::Disable_Light(unsigned index)
{
	DX9Backend::Set_Light(index, nullptr);
}

void DX9Backend::Set_Light_From_State(unsigned index, const RenderBackendLight * light)
{
	if (light) {
		render_state.Lights[index] = *light;
		render_state.LightEnable[index] = true;
	}
	else {
		render_state.LightEnable[index] = false;
	}
	render_state_changed |= (LIGHT0_CHANGED << index);
}

void DX9Backend::Capture_Render_State(RenderStateStruct & state)
{
	DX9Backend::Get_Render_State(state);
}

void DX9Backend::Restore_Render_State()
{
	DX9Backend::Release_Render_State();
}

void DX9Backend::Apply_Render_State(const RenderStateStruct & state)
{
	DX9Backend::Set_Render_State(state);
}

void DX9Backend::Begin_Backend_Statistics()
{
	DX9Backend::Begin_Statistics();
}

void DX9Backend::End_Backend_Statistics()
{
	DX9Backend::End_Statistics();
}
