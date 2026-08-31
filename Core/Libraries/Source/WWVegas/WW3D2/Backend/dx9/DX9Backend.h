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
 *                     $Archive:: /Commando/Code/ww3d2/Backend/DX9Backend.h                           $*
 *                                                                                             *
 *              Original Author:: Jani Penttinen                                               *
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 08/05/02 2:40p                                              $*
 *                                                                                             *
 *                    $Revision:: 92                                                          $*
 *                                                                                             *
 * 06/26/02 KM Matrix name change to avoid MAX conflicts                                       *
 * 06/27/02 KM Render to shadow buffer texture support														*
 * 08/05/02 KM Texture class redesign
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "Backend/IRenderBackend.h"
#include "WW3D2/RenderState.h"

#include "WWLib/always.h"
#include <stdint.h>
#include "WW3D2/DList.h"
#include "d3d9.h"
#include "WWMath/matrix4.h"
#include "WW3D2/Statistics.h"
#include "WWLib/wwstring.h"
#include "WW3D2/LightEnvironment.h"
#include "WW3D2/Shader.h"
#include "WWMath/vector4.h"
#include "WWLib/cpudetect.h"
#include "WW3D2/Backend/dx9/Caps.h"
#include "WW3D2/Backend/dx9/VertexFormatMapper.h"

#include "WW3D2/Texture.h"
#include "WW3D2/VertexBuffer.h"
#include "WW3D2/IndexBuffer.h"
#include "WW3D2/VertMaterial.h"

#ifndef D3DRS_ZBIAS
#define D3DRS_ZBIAS D3DRS_DEPTHBIAS
#endif
#ifndef D3DRS_LINEPATTERN
#define D3DRS_LINEPATTERN ((D3DRENDERSTATETYPE)0x7fff0001)
#endif
#ifndef D3DRS_ZVISIBLE
#define D3DRS_ZVISIBLE ((D3DRENDERSTATETYPE)0x7fff0002)
#endif
#ifndef D3DRS_EDGEANTIALIAS
#define D3DRS_EDGEANTIALIAS ((D3DRENDERSTATETYPE)0x7fff0003)
#endif
#ifndef D3DRS_SOFTWAREVERTEXPROCESSING
#define D3DRS_SOFTWAREVERTEXPROCESSING ((D3DRENDERSTATETYPE)0x7fff0004)
#endif
#ifndef D3DRS_PATCHSEGMENTS
#define D3DRS_PATCHSEGMENTS ((D3DRENDERSTATETYPE)0x7fff0005)
#endif

#ifndef D3DTSS_ADDRESSU
#define D3DTSS_ADDRESSU ((D3DTEXTURESTAGESTATETYPE)0x7ffe0001)
#endif
#ifndef D3DTSS_ADDRESSV
#define D3DTSS_ADDRESSV ((D3DTEXTURESTAGESTATETYPE)0x7ffe0002)
#endif
#ifndef D3DTSS_BORDERCOLOR
#define D3DTSS_BORDERCOLOR ((D3DTEXTURESTAGESTATETYPE)0x7ffe0003)
#endif
#ifndef D3DTSS_MAGFILTER
#define D3DTSS_MAGFILTER ((D3DTEXTURESTAGESTATETYPE)0x7ffe0004)
#endif
#ifndef D3DTSS_MINFILTER
#define D3DTSS_MINFILTER ((D3DTEXTURESTAGESTATETYPE)0x7ffe0005)
#endif
#ifndef D3DTSS_MIPFILTER
#define D3DTSS_MIPFILTER ((D3DTEXTURESTAGESTATETYPE)0x7ffe0006)
#endif
#ifndef D3DTSS_MIPMAPLODBIAS
#define D3DTSS_MIPMAPLODBIAS ((D3DTEXTURESTAGESTATETYPE)0x7ffe0007)
#endif
#ifndef D3DTSS_MAXMIPLEVEL
#define D3DTSS_MAXMIPLEVEL ((D3DTEXTURESTAGESTATETYPE)0x7ffe0008)
#endif
#ifndef D3DTSS_MAXANISOTROPY
#define D3DTSS_MAXANISOTROPY ((D3DTEXTURESTAGESTATETYPE)0x7ffe0009)
#endif
#ifndef D3DTSS_ADDRESSW
#define D3DTSS_ADDRESSW ((D3DTEXTURESTAGESTATETYPE)0x7ffe000A)
#endif

#ifndef D3DSWAPEFFECT_COPY_VSYNC
#define D3DSWAPEFFECT_COPY_VSYNC D3DSWAPEFFECT_COPY
#endif

#ifndef D3DENUM_NO_WHQL_LEVEL
#define D3DENUM_NO_WHQL_LEVEL 0
#endif

// D3D9 removed D3D9 fixed-function declaration tokens. Keep no-op token
// definitions so legacy declaration arrays still compile where unused.
#ifndef D3DVSD_STREAM
#define D3DVSD_STREAM(_stream) 0U
#endif
#ifndef D3DVSD_REG
#define D3DVSD_REG(_reg, _type) 0U
#endif
#ifndef D3DVSD_END
#define D3DVSD_END() 0xFFFFFFFFU
#endif
#ifndef D3DVSDT_FLOAT1
#define D3DVSDT_FLOAT1 0U
#endif
#ifndef D3DVSDT_FLOAT2
#define D3DVSDT_FLOAT2 0U
#endif
#ifndef D3DVSDT_FLOAT3
#define D3DVSDT_FLOAT3 0U
#endif
#ifndef D3DVSDT_FLOAT4
#define D3DVSDT_FLOAT4 0U
#endif
#ifndef D3DVSDT_D3DCOLOR
#define D3DVSDT_D3DCOLOR 0U
#endif

/*
** Registry value names
*/
#define	VALUE_NAME_RENDER_DEVICE_NAME					"RenderDeviceName"
#define	VALUE_NAME_RENDER_DEVICE_WIDTH				"RenderDeviceWidth"
#define	VALUE_NAME_RENDER_DEVICE_HEIGHT				"RenderDeviceHeight"
#define	VALUE_NAME_RENDER_DEVICE_DEPTH				"RenderDeviceDepth"
#define	VALUE_NAME_RENDER_DEVICE_WINDOWED			"RenderDeviceWindowed"
#define	VALUE_NAME_RENDER_DEVICE_TEXTURE_DEPTH		"RenderDeviceTextureDepth"

class VertexMaterialClass;
class CameraClass;
class LightEnvironmentClass;
class RenderDeviceDescClass;
class VertexBufferClass;
class DynamicVBAccessClass;
class IndexBufferClass;
class DynamicIBAccessClass;
class TextureClass;
class LightClass;
class SurfaceClass;
class DX9Backend;

struct RenderFrameStatistics
{
	RenderFrameStatistics() :
		matrix_changes(0),
		material_changes(0),
		vertex_buffer_changes(0),
		index_buffer_changes(0),
		light_changes(0),
		texture_changes(0),
		render_state_changes(0),
		texture_stage_state_changes(0),
		dx9_calls(0),
		draw_calls(0)
	{
	}

	unsigned matrix_changes;
	unsigned material_changes;
	unsigned vertex_buffer_changes;
	unsigned index_buffer_changes;
	unsigned light_changes;
	unsigned texture_changes;
	unsigned render_state_changes;
	unsigned texture_stage_state_changes;
	unsigned dx9_calls;
	unsigned draw_calls;
};

#define DX9_RECORD_MATRIX_CHANGE()				FrameStatistics.matrix_changes++
#define DX9_RECORD_MATERIAL_CHANGE()			FrameStatistics.material_changes++
#define DX9_RECORD_VERTEX_BUFFER_CHANGE()		FrameStatistics.vertex_buffer_changes++
#define DX9_RECORD_INDEX_BUFFER_CHANGE()		FrameStatistics.index_buffer_changes++
#define DX9_RECORD_LIGHT_CHANGE()				FrameStatistics.light_changes++
#define DX9_RECORD_TEXTURE_CHANGE()				FrameStatistics.texture_changes++
#define DX9_RECORD_RENDER_STATE_CHANGE()		FrameStatistics.render_state_changes++
#define DX9_RECORD_TEXTURE_STAGE_STATE_CHANGE() FrameStatistics.texture_stage_state_changes++
#define DX9_RECORD_DX9_CALLS()					FrameStatistics.dx9_calls++
#define DX9_RECORD_DRAW_CALLS()					FrameStatistics.draw_calls++

extern bool _DX9SingleThreaded;

void DX9_Assert();
void Log_DX9_ErrorCode(unsigned res, const char* file, int line);

WWINLINE void DX9_ErrorCode_Impl(unsigned res, const char* file, int line)
{
	if (res==D3D_OK) return;
	Log_DX9_ErrorCode(res, file, line);
}
#define DX9_ErrorCode(res) DX9_ErrorCode_Impl((res), __FILE__, __LINE__)

#ifdef WWDEBUG
#define DX9CALL_HRES(x,res) DX9_Assert(); res = DX9Backend::_Get_D3D_Device()->x; DX9_ErrorCode(res); DX9Backend::Increment_DX9_CallCount();
#define DX9CALL(x) DX9_Assert(); DX9_ErrorCode(DX9Backend::_Get_D3D_Device()->x); DX9Backend::Increment_DX9_CallCount();
#define DX9CALL_D3D(x) DX9_Assert(); DX9_ErrorCode(DX9Backend::_Get_D3D9()->x); DX9Backend::Increment_DX9_CallCount();
#define DX9_THREAD_ASSERT() if (_DX9SingleThreaded) { WWASSERT_PRINT(DX9Backend::_Get_Main_Thread_ID()==ThreadClass::_Get_Current_Thread_ID(),"DX9Backend::DX9 calls must be called from the main thread!"); }
#else
#define DX9CALL_HRES(x,res) res = DX9Backend::_Get_D3D_Device()->x; DX9Backend::Increment_DX9_CallCount();
#define DX9CALL(x) DX9Backend::_Get_D3D_Device()->x; DX9Backend::Increment_DX9_CallCount();
#define DX9CALL_D3D(x) DX9Backend::_Get_D3D9()->x; DX9Backend::Increment_DX9_CallCount();
#define DX9_THREAD_ASSERT() ;
#endif

/**
** DX9Backend
**
** DX9 interface wrapper class.  This encapsulates the DX9 interface; adding redundant state
** detection, stat tracking, etc etc.  In general, we will wrap all DX9 calls with at least
** an WWINLINE function so that we can add stat tracking, etc if needed.  Direct access to the
** D3D device will require "friend" status and should be granted only in extreme circumstances :-)
*/
class DX9Backend : public IRenderBackend
{
	enum ChangedStates {
		WORLD_CHANGED	=	1<<0,
		VIEW_CHANGED	=	1<<1,
		LIGHT0_CHANGED	=	1<<2,
		LIGHT1_CHANGED	=	1<<3,
		LIGHT2_CHANGED	=	1<<4,
		LIGHT3_CHANGED	=	1<<5,
		TEXTURE0_CHANGED=	1<<6,
		TEXTURE1_CHANGED=	1<<7,
		TEXTURE2_CHANGED=	1<<8,
		TEXTURE3_CHANGED=	1<<9,
		MATERIAL_CHANGED=	1<<14,
		SHADER_CHANGED	=	1<<15,
		VERTEX_BUFFER_CHANGED = 1<<16,
		INDEX_BUFFER_CHANGED = 1 << 17,
		WORLD_IDENTITY=	1<<18,
		VIEW_IDENTITY=		1<<19,

		TEXTURES_CHANGED=
			TEXTURE0_CHANGED|TEXTURE1_CHANGED|TEXTURE2_CHANGED|TEXTURE3_CHANGED,
		LIGHTS_CHANGED=
			LIGHT0_CHANGED|LIGHT1_CHANGED|LIGHT2_CHANGED|LIGHT3_CHANGED,
	};

	void Draw_Sorting_IB_VB(
		unsigned primitive_type,
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);

	void Draw(
		unsigned primitive_type,
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index=0,
		unsigned short vertex_count=0);

public:
	explicit DX9Backend(bool lite = false);
	virtual ~DX9Backend() override;
	static DX9Backend *Create(void * window, bool lite);

	bool Initialize(void * hwnd, bool lite = false);
	void Shutdown();

	/*
	** Some WW3D sub-systems need to be initialized after the device is created and shutdown
	** before the device is released.
	*/
	void	Do_Onetime_Device_Dependent_Inits();
	void Do_Onetime_Device_Dependent_Shutdowns();

	bool Is_Device_Lost() const { return IsDeviceLost; }
	bool Is_Initted() const { return IsInitted; }

	bool Has_Stencil () const;
	void Get_Format_Name(unsigned int format, StringClass *tex_format);
	HRESULT Get_Last_Create_Device_HRESULT(void) { return LastCreateDeviceHRESULT; }
	const char* Get_Last_Set_Render_Device_Error(void) { return LastSetRenderDeviceError; }

	/*
	** Rendering
	*/
	void Begin_Scene();
	void End_Scene(bool flip_frame = true);

	// Flip until the primary buffer is visible.
	void Flip_To_Primary();

	void Clear(bool clear_color, bool clear_z_stencil, const Vector3 &color, float dest_alpha=0.0f, float z=1.0f, unsigned int stencil=0);

	void	Set_Viewport(CONST D3DVIEWPORT9* pViewport);

	void Set_Vertex_Buffer(const VertexBufferClass* vb, unsigned stream=0);
	void Set_Vertex_Buffer(const DynamicVBAccessClass& vba);
	void Set_Index_Buffer(const IndexBufferClass* ib,unsigned short index_base_offset);
	void Set_Index_Buffer(const DynamicIBAccessClass& iba,unsigned short index_base_offset);
	void Set_Index_Buffer_Index_Offset(unsigned offset);

	static void Get_Render_State(RenderStateStruct& state);
	static void Set_Render_State(const RenderStateStruct& state);
	static void Release_Render_State();

	void Set_DX9_Material(const D3DMATERIAL9* mat);

	void Set_Gamma(float gamma,float bright,float contrast,bool calibrate=true,bool uselimit=true);

	// Set_ and Get_Transform() functions take the matrix in Westwood convention format.

	void Set_Projection_Transform_With_Z_Bias(const Matrix4x4& matrix,float znear, float zfar);	// pointer to 16 matrices

	void Set_Transform(D3DTRANSFORMSTATETYPE transform,const Matrix4x4& m);
	void Set_Transform(D3DTRANSFORMSTATETYPE transform,const Matrix3D& m);
	void Get_Transform(D3DTRANSFORMSTATETYPE transform, Matrix4x4& m);
	void Set_World_Identity();
	void Set_View_Identity();
	bool Is_World_Identity();
	bool Is_View_Identity();

	// Note that *_DX9_Transform() functions take the matrix in DX9 format - transposed from Westwood convention.

	void _Set_DX9_Transform(D3DTRANSFORMSTATETYPE transform, const D3DMATRIX& m);
	void _Get_DX9_Transform(D3DTRANSFORMSTATETYPE transform, D3DMATRIX& m);

	static void Set_DX9_Light(int index,D3DLIGHT9* light);
	static void Set_DX9_Render_State(D3DRENDERSTATETYPE state, unsigned value);
	static void Set_DX9_Clip_Plane(DWORD Index, CONST float* pPlane);
	static void Set_DX9_Texture_Stage_State(unsigned stage, D3DTEXTURESTAGESTATETYPE state, unsigned value);
	static void Set_DX9_Texture(unsigned int stage, IDirect3DBaseTexture9* texture);
	void Set_Light_Environment(LightEnvironmentClass* light_env);
	LightEnvironmentClass* Get_Light_Environment() { return Light_Environment; }
	void Set_Fog(bool enable, const Vector3 &color, float start, float end);

	static WWINLINE const RenderBackendLight& Peek_Light(unsigned index);
	static WWINLINE bool Is_Light_Enabled(unsigned index);

	bool Validate_Device(void);

	// Deferred

	void Set_Shader(const ShaderClass& shader);
	void Get_Shader(ShaderClass& shader);
	void Set_Texture(unsigned stage,TextureBaseClass* texture);
	void Set_Material(const VertexMaterialClass* material);
	void Set_Light(unsigned index,const D3DLIGHT9* light);
	void Set_Light(unsigned index,const LightClass &light);

	void Apply_Render_State_Changes();	// Apply deferred render state changes (will be called automatically by Draw...)

	void Draw_Triangles(
		unsigned buffer_type,
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);
	void Draw_Triangles(
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);
	void Draw_Strip(
		unsigned short start_index,
		unsigned short index_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);

	/*
	** Resources
	*/

	static IDirect3DVolumeTexture9* _Create_DX9_Volume_Texture
	(
		unsigned int width,
		unsigned int height,
		unsigned int depth,
		WW3DFormat format,
		MipCountType mip_level_count,
		D3DPOOL pool=D3DPOOL_MANAGED
	);

	static IDirect3DCubeTexture9* _Create_DX9_Cube_Texture
	(
		unsigned int width,
		unsigned int height,
		WW3DFormat format,
		MipCountType mip_level_count,
		D3DPOOL pool=D3DPOOL_MANAGED,
		bool rendertarget=false
	);

	static IDirect3DTexture9* _Create_DX9_ZTexture
	(
		unsigned int width,
		unsigned int height,
		WW3DZFormat zformat,
		MipCountType mip_level_count,
		D3DPOOL pool=D3DPOOL_MANAGED
	);

	static IDirect3DTexture9 * _Create_DX9_Texture
	(
		unsigned int width,
		unsigned int height,
		WW3DFormat format,
		MipCountType mip_level_count,
		D3DPOOL pool=D3DPOOL_MANAGED,
		bool rendertarget=false
	);
	static IDirect3DTexture9 * _Create_DX9_Texture(const char *filename, MipCountType mip_level_count);
	static IDirect3DTexture9 * _Create_DX9_Texture(IDirect3DSurface9 *surface, MipCountType mip_level_count);

	static IDirect3DSurface9 * _Create_DX9_Surface(unsigned int width, unsigned int height, WW3DFormat format);
	static IDirect3DSurface9 * _Create_DX9_Surface(const char *filename);
	static IDirect3DSurface9 * _Get_DX9_Front_Buffer();
	static SurfaceClass * _Get_DX9_Back_Buffer(unsigned int num=0);

	static void _Copy_DX9_Rects(
			IDirect3DSurface9* pSourceSurface,
			CONST RECT* pSourceRectsArray,
			UINT cRects,
			IDirect3DSurface9* pDestinationSurface,
			CONST POINT* pDestPointsArray
	);

	static void _Update_Texture(TextureClass *system, TextureClass *video);
	static void Flush_DX9_Resource_Manager(unsigned int bytes=0);
	static unsigned int Get_Free_Texture_RAM();

	static unsigned _Get_Main_Thread_ID() { return _MainThreadID; }
	static const D3DADAPTER_IDENTIFIER9& Get_Current_Adapter_Identifier() { return CurrentAdapterIdentifier; }

	/*
	** Statistics
	*/
	static void Begin_Statistics();
	static void End_Statistics();
	static const RenderFrameStatistics& Get_Last_Frame_Statistics();
	static unsigned long Get_FrameCount();
	static void Increment_DX9_CallCount() { DX9_RECORD_DX9_CALLS(); }

	// Needed by shader class
	static bool						Get_Fog_Enable() { return FogEnable; }
	static D3DCOLOR				Get_D3D_Fog_Color() { return FogColor; }

	// Utilities
	static Vector4 Convert_Color(unsigned color);
	static unsigned int Convert_Color(const Vector4& color);
	static unsigned int Convert_Color(const Vector3& color, const float alpha);
	static void Clamp_Color(Vector4& color);
	static unsigned int Convert_Color_Clamp(const Vector4& color);

	static void			  Set_Alpha (const float alpha, unsigned int &color);

	static void _Enable_Triangle_Draw(bool enable) { _EnableTriangleDraw=enable; }
	static bool _Is_Triangle_Draw_Enabled() { return _EnableTriangleDraw; }

	/*
	** Additional swap chain interface
	**
	**		Use this interface to render to multiple windows (in windowed mode).
	**	To render to an additional window, the sequence of calls should look
	**	something like this:
	**
	**	DX9Backend::Set_Render_Target (swap_chain_ptr);
	**
	**	WW3D::Begin_Render (true, true, Vector3 (0, 0, 0));
	**	WW3D::Render (scene, camera, FALSE, FALSE);
	**	WW3D::End_Render ();
	**
	**	swap_chain_ptr->Present (nullptr, nullptr, nullptr, nullptr);
	**
	**	DX9Backend::Set_Render_Target ((IDirect3DSurface9 *)nullptr);
	**
	*/
	IDirect3DSwapChain9 *	Create_Additional_Swap_Chain (HWND render_window);

	/*
	** Render target interface. If render target format is WW3D_FORMAT_UNKNOWN, current display format is used.
	*/
	TextureClass *	Create_Render_Target (int width, int height, WW3DFormat format = WW3D_FORMAT_UNKNOWN);

	void					Set_Render_Target (IDirect3DSurface9 *render_target, bool use_default_depth_buffer = false);
	void					Set_Render_Target (IDirect3DSurface9* render_target, IDirect3DSurface9* dpeth_buffer);

	void					Set_Render_Target (IDirect3DSwapChain9 *swap_chain);
	bool					Is_Render_To_Texture(void) const { return IsRenderToTexture; }

	// for depth map support KJM V
	void Create_Render_Target
	(
		int width,
		int height,
		WW3DFormat format,
		WW3DZFormat zformat,
		TextureClass** target,
		ZTextureClass** depth_buffer
	);
	void					Set_Render_Target_With_Z (TextureClass * texture, ZTextureClass* ztexture=nullptr);

	static void Set_Shadow_Map(int idx, ZTextureClass* ztex) { Shadow_Map[idx]=ztex; }
	static ZTextureClass* Get_Shadow_Map(int idx) { return Shadow_Map[idx]; }
	// for depth map support KJM ^

	// shader system updates KJM v
	void Apply_Default_State();

	void Set_Vertex_Shader(uintptr_t vertex_shader,
		const RenderBackendVertexShaderInputLayout * input_layout = nullptr) override;
	void Set_Pixel_Shader(uintptr_t pixel_shader);
	void Release_Vertex_Shader(uintptr_t shader);
	void Release_Pixel_Shader(uintptr_t shader);

	static void Set_Vertex_Shader_Constant(int reg, const void* data, int count);
	static void Set_Pixel_Shader_Constant(int reg, const void* data, int count);

	static DWORD Get_Vertex_Processing_Behavior() { return Vertex_Processing_Behavior; }

	// Needed by scene lighting class
	void						Set_Ambient(const Vector3& color);
	static const Vector3&		Get_Ambient() { return Ambient_Color; }
	// shader system updates KJM ^

	static IDirect3DDevice9* _Get_D3D_Device() { return D3DDevice; }
	static IDirect3D9* _Get_D3D9() { return D3DInterface; }
	/// Returns the display format - added by TR for video playback - not part of W3D
	WW3DFormat	getBackBufferFormat() const;
	bool Reset_Device(bool reload_assets=true);

	static const DX9Caps*	Get_Current_Caps() { WWASSERT(CurrentCaps); return CurrentCaps; }

	bool Registry_Save_Render_Device( const char * sub_key );
	bool Registry_Load_Render_Device( const char * sub_key, bool resize_window );

	static const char* Get_DX9_Render_State_Name(D3DRENDERSTATETYPE state);
	static const char* Get_DX9_Texture_Stage_State_Name(D3DTEXTURESTAGESTATETYPE state);
	static unsigned Get_DX9_Render_State(D3DRENDERSTATETYPE state) { return RenderStates[state]; }

	// Names of the specific values of render states and texture stage states
	static void Get_DX9_Texture_Stage_State_Value_Name(StringClass& name, D3DTEXTURESTAGESTATETYPE state, unsigned value);
	static void Get_DX9_Render_State_Value_Name(StringClass& name, D3DRENDERSTATETYPE state, unsigned value);

	static const char* Get_DX9_Texture_Address_Name(unsigned value);
	static const char* Get_DX9_Texture_Filter_Name(unsigned value);
	static const char* Get_DX9_Texture_Arg_Name(unsigned value);
	static const char* Get_DX9_Texture_Op_Name(unsigned value);
	static const char* Get_DX9_Texture_Transform_Flag_Name(unsigned value);
	static const char* Get_DX9_ZBuffer_Type_Name(unsigned value);
	static const char* Get_DX9_Fill_Mode_Name(unsigned value);
	static const char* Get_DX9_Shade_Mode_Name(unsigned value);
	static const char* Get_DX9_Blend_Name(unsigned value);
	static const char* Get_DX9_Cull_Mode_Name(unsigned value);
	static const char* Get_DX9_Cmp_Func_Name(unsigned value);
	static const char* Get_DX9_Fog_Mode_Name(unsigned value);
	static const char* Get_DX9_Stencil_Op_Name(unsigned value);
	static const char* Get_DX9_Material_Source_Name(unsigned value);
	static const char* Get_DX9_Vertex_Blend_Flag_Name(unsigned value);
	static const char* Get_DX9_Patch_Edge_Style_Name(unsigned value);
	static const char* Get_DX9_Debug_Monitor_Token_Name(unsigned value);
	static const char* Get_DX9_Blend_Op_Name(unsigned value);

	void Invalidate_Cached_Render_States(void);
	static bool Is_Valid_D3D_Object_Ptr(const void* ptr, const char* context);

	static void Set_Draw_Polygon_Low_Bound_Limit(unsigned n) { DrawPolygonLowBoundLimit=n; }

public:

	bool	Create_Device();
	void Release_Device();

	void Reset_Statistics();
	void Enumerate_Devices();
	void Set_Default_Global_Render_States();

	virtual bool Supports_TnL() const override;
	virtual bool Supports_DXTC() const override;
	virtual bool Supports_NPatches() const override;
	virtual bool Supports_Bump_Envmap() const override;
	virtual bool Supports_Bump_Envmap_Luminance() const override;
	virtual bool Supports_Z_Bias() const override;
	virtual bool Supports_Anisotropic_Filtering() const override;
	virtual bool Supports_Modulate_Alpha_Add_Color() const override;
	virtual bool Supports_Dot3() const override;
	virtual bool Supports_Point_Sprites() const override;
	virtual bool Supports_Cubemaps() const override;
	virtual bool Supports_Color_Write_Mask() const override;
	virtual bool Supports_Texture_Operation(RenderBackendTextureOperation operation) const override;
	virtual bool Supports_Texture_Filter(RenderBackendTextureFilterType type,
		RenderBackendTextureFilter filter) const override;
	virtual bool Is_Fog_Allowed() const override;
	virtual bool Is_Fog_Enabled() const override;
	virtual unsigned Get_Fog_Color() const override;
	virtual bool Supports_Texture_Format(WW3DFormat format) const override;
	virtual bool Supports_Render_To_Texture_Format(WW3DFormat format) const override;
	virtual bool Supports_Depth_Stencil_Format(WW3DZFormat format) const override;
	virtual WW3DFormat Get_Back_Buffer_Format() const override;
	virtual RenderBackendDeviceStatus Get_Device_Status() const override;
	virtual bool Is_Device_Ready() const override;
	virtual bool Is_Render_Thread() const override;
	virtual bool Get_Adapter_Info(RenderBackendAdapterInfo & info) const override;
	virtual bool Get_Texture_Limits(RenderBackendTextureLimits & limits) const override;
	virtual int Get_Max_Textures_Per_Pass() const override;
	virtual bool Is_3DFX_Voodoo3() const override;
	virtual unsigned Pack_Color(const Vector4 & color) const override;
	virtual unsigned Pack_Color(const Vector3 & color, float alpha) const override;
	virtual unsigned Pack_Color_Clamped(const Vector4 & color) const override;
	virtual Vector4 Unpack_Color(unsigned color) const override;
	virtual bool Is_Triangle_Draw_Enabled() const override;
	virtual void Set_Triangle_Draw_Enabled(bool enable) override;
	virtual RenderBackendDebugSettings & Get_Debug_Settings() override;
	virtual void Set_Cleanup_Hook(RenderBackendCleanupHook * hook) override;
	virtual void Invalidate_Renderer_Caches() override;
	virtual RenderBackendFont * Create_Font(int height, const char * face_name,
		bool bold = false, int width = 0) override;
	virtual void Release_Font(RenderBackendFont * font) override;
	virtual bool Get_Font_Metrics(RenderBackendFont * font,
		RenderBackendFontMetrics & metrics) const override;
	virtual bool Get_Font_Glyph(RenderBackendFont * font, unsigned int character,
		RenderBackendFontGlyph & glyph) override;
	virtual void Draw_Font(RenderBackendFont * font, const char * text,
		unsigned text_length, const RenderBackendRect & rect,
		unsigned flags, unsigned color) override;
	virtual bool Initialize_Browser(const char * bad_page_url = nullptr,
		const char * loading_page_url = nullptr,
		const char * mouse_filename = nullptr,
		const char * mouse_busy_filename = nullptr) override;
	virtual void Shutdown_Browser() override;
	virtual void Update_Browser() override;
	virtual void Render_Browser(int backbuffer_index) override;
	virtual void Create_Browser(const char * browser_name, const char * url,
		int x, int y, int width, int height, int update_ticks = 0,
		unsigned options = RenderBackendBrowserOptionScrollbars |
			RenderBackendBrowserOption3DBorder,
		void * game_dispatch = nullptr) override;
	virtual void Destroy_Browser(const char * browser_name) override;
	virtual bool Is_Browser_Open(const char * browser_name) const override;
	virtual void Navigate_Browser(const char * browser_name, const char * url) override;

	virtual void Set_Viewport(const RenderBackendViewport & viewport) override;
	virtual bool Get_Viewport(RenderBackendViewport & viewport) const override;
	virtual void Set_Material_Values(const RenderBackendMaterial & material) override;
	virtual void Set_Fill_Mode(RenderBackendFillMode mode) override;
	virtual RenderBackendFillMode Get_Fill_Mode() const override;
	virtual void Set_Color_Write_Mask(RenderBackendColorWriteMask mask) override;
	virtual RenderBackendColorWriteMask Get_Color_Write_Mask() const override;
	virtual void Set_Alpha_Blend_Enabled(bool enable) override;
	virtual void Set_Blend_Operation(RenderBackendBlendOperation operation) override;
	virtual void Set_Blend_Factors(RenderBackendBlendFactor source,
		RenderBackendBlendFactor destination) override;
	virtual void Set_Source_Blend_Factor(RenderBackendBlendFactor factor) override;
	virtual void Set_Destination_Blend_Factor(RenderBackendBlendFactor factor) override;
	virtual void Set_Alpha_Test_Enabled(bool enable) override;
	virtual void Set_Alpha_Test_Function(RenderBackendCompareFunction function) override;
	virtual void Set_Alpha_Test_Reference(unsigned reference) override;
	virtual void Set_Fog_Enabled(bool enable) override;
	virtual void Set_Fog_Color(unsigned color) override;
	virtual void Set_Depth_Bias(unsigned bias) override;
	virtual void Set_Texture_Factor(unsigned color) override;
	virtual void Set_Depth_Test_Enabled(bool enable) override;
	virtual void Set_Depth_Write_Enabled(bool enable) override;
	virtual void Set_Depth_Function(RenderBackendCompareFunction function) override;
	virtual void Set_Cull_Mode(RenderBackendCullMode mode) override;
	virtual RenderBackendCullMode Get_Cull_Mode() const override;
	virtual void Set_Point_Sprite_Enabled(bool enable) override;
	virtual void Set_Point_Scale_Enabled(bool enable) override;
	virtual void Set_Point_Size(float size) override;
	virtual void Set_Point_Size_Min(float size) override;
	virtual void Set_Point_Size_Max(float size) override;
	virtual void Set_Point_Scale(float scale_a, float scale_b, float scale_c) override;
	virtual void Set_Shade_Mode(RenderBackendShadeMode mode) override;
	virtual void Set_Lighting_Enabled(bool enable) override;
	virtual void Set_Normalize_Normals(bool enable) override;
	virtual void Set_Specular_Enabled(bool enable) override;
	virtual void Set_Material_Color_Sources(RenderBackendMaterialSource ambient,
		RenderBackendMaterialSource diffuse,
		RenderBackendMaterialSource emissive) override;
	virtual void Set_NPatch_Segments(float segments) override;
	virtual void Set_Stencil_Enabled(bool enable) override;
	virtual void Set_Stencil_Function(RenderBackendCompareFunction function) override;
	virtual void Set_Stencil_Reference(unsigned reference) override;
	virtual void Set_Stencil_Read_Mask(unsigned mask) override;
	virtual void Set_Stencil_Write_Mask(unsigned mask) override;
	virtual void Set_Stencil_Z_Fail_Operation(RenderBackendStencilOperation operation) override;
	virtual void Set_Stencil_Fail_Operation(RenderBackendStencilOperation operation) override;
	virtual void Set_Stencil_Pass_Operation(RenderBackendStencilOperation operation) override;
	virtual void Set_Texture_Operation(unsigned stage,
		RenderBackendTextureComponent component,
		RenderBackendTextureOperation operation) override;
	virtual void Set_Texture_Argument(unsigned stage,
		RenderBackendTextureComponent component,
		unsigned argument_index,
		RenderBackendTextureArgument argument,
		RenderBackendTextureArgumentModifiers modifiers =
			RenderBackendTextureArgumentModifiers::None) override;
	virtual void Set_Texture_Coordinate_Source(unsigned stage,
		RenderBackendTextureCoordinateSource source,
		unsigned uv_array_index = 0) override;
	virtual void Set_Texture_Transform_Flags(unsigned stage,
		RenderBackendTextureTransformFlags flags) override;
	virtual void Set_Texture_Address_Mode(unsigned stage,
		bool u_coordinate,
		RenderBackendTextureAddressMode mode) override;
	virtual void Set_Texture_Filter(unsigned stage,
		RenderBackendTextureFilterType type,
		RenderBackendTextureFilter filter) override;
	virtual void Set_Texture_Max_Anisotropy(unsigned stage, unsigned level) override;
	virtual void Set_Texture_Bump_Environment_Matrix(unsigned stage,
		float m00, float m01, float m10, float m11,
		float scale = 1.0f, float offset = 0.0f) override;
	virtual void Set_Render_Target(TextureClass * render_target,
		ZTextureClass * depth_target = nullptr) override;
	virtual void Set_Multisample_Mode(RenderBackendMultisampleMode mode) override;
	virtual RenderBackendMultisampleMode Get_Multisample_Mode() const override;

	virtual RenderBackendSurface * Create_System_Memory_Surface(unsigned width,
		unsigned height, WW3DFormat format) override;
	virtual SurfaceClass * Create_Surface(unsigned width, unsigned height,
		WW3DFormat format) override;
	virtual RenderBackendSurface * Create_Surface_From_File(const char * filename) override;
	virtual bool Get_Surface_Description(RenderBackendSurface * surface,
		RenderBackendSurfaceDescription & description) const override;
	virtual bool Lock_Surface(RenderBackendSurface * surface,
		RenderBackendLockedSurface & locked_surface,
		const RenderBackendRect * rect = nullptr,
		RenderBackendSurfaceLockMode mode = RenderBackendSurfaceLockMode::ReadWrite) override;
	virtual void Unlock_Surface(RenderBackendSurface * surface) override;
	virtual void Release_Surface(RenderBackendSurface * surface) override;
	virtual void Copy_Surface_Rect(RenderBackendSurface * source,
		const RenderBackendRect & source_rect,
		SurfaceClass * destination,
		const RenderBackendPoint & destination_point) override;
	virtual bool Copy_Surface_Rect(SurfaceClass * source,
		const RenderBackendRect & source_rect,
		RenderBackendSurface * destination,
		const RenderBackendPoint & destination_point) override;
	virtual bool Copy_Surface(SurfaceClass * source, SurfaceClass * destination) override;
	virtual bool Copy_Surface_Rect(SurfaceClass * source, const RenderBackendRect & source_rect,
		SurfaceClass * destination, const RenderBackendPoint & destination_point) override;
	virtual bool Copy_Surface_Stretch(SurfaceClass * source,
		const RenderBackendRect & source_rect, SurfaceClass * destination,
		const RenderBackendRect & destination_rect) override;
	virtual int Read_Back_Buffer_Rect(void * buffer, int buffer_size,
		int x, int y, int width, int height) override;
	virtual RenderBackendTextureHandle Create_Transient_Render_Texture(unsigned width, unsigned height,
		WW3DFormat format) override;
	virtual bool Copy_Back_Buffer_To_Texture(RenderBackendTextureHandle texture) override;
	virtual bool Copy_Texture_To_Surface(RenderBackendTextureHandle texture, SurfaceClass * destination) override;
	virtual bool Copy_Render_Target_To_Surface(TextureClass * source, SurfaceClass * destination) override;
	virtual void Release_Transient_Render_Texture(RenderBackendTextureHandle texture) override;
	virtual RenderBackendTextureHandle Create_Texture_Handle(unsigned width, unsigned height, WW3DFormat format,
		unsigned mip_levels, bool dynamic, bool render_target = false) override;
	virtual RenderBackendTextureHandle Create_ZTexture_Handle(unsigned width, unsigned height, WW3DZFormat format,
		unsigned mip_levels) override;
	virtual RenderBackendTextureHandle Create_Surface_Handle(unsigned width, unsigned height, WW3DFormat format) override;
	virtual RenderBackendTextureHandle Create_Surface_Handle(const char *filename) override;
	virtual RenderBackendTextureHandle Create_Texture_Handle_Pooled(unsigned width, unsigned height, WW3DFormat format,
		unsigned mip_levels, RenderBackendTexturePool pool, bool render_target) override;
	virtual RenderBackendTextureHandle Create_ZTexture_Handle_Pooled(unsigned width, unsigned height, WW3DZFormat format,
		unsigned mip_levels, RenderBackendTexturePool pool) override;
	virtual RenderBackendTextureHandle Create_Cube_Texture_Handle(unsigned width, unsigned height, WW3DFormat format,
		unsigned mip_levels, RenderBackendTexturePool pool, bool render_target) override;
	virtual RenderBackendTextureHandle Create_Volume_Texture_Handle(unsigned width, unsigned height, unsigned depth,
		WW3DFormat format, unsigned mip_levels, RenderBackendTexturePool pool) override;
	virtual RenderBackendTextureHandle Create_Texture_From_Surface(RenderBackendSurface * surface,
		unsigned mip_levels) override;
	virtual SurfaceClass * Get_Texture_Surface_Level(RenderBackendTextureHandle texture, unsigned level) override;
	virtual RenderBackendTextureHandle Create_Texture_From_File_Handle(const char *filename, unsigned mip_levels) override;
	virtual RenderBackendTextureHandle Add_Texture_Reference(RenderBackendTextureHandle texture) override;
	virtual void Release_Texture_Handle(RenderBackendTextureHandle texture) override;
	virtual unsigned Get_Texture_Level_Count(RenderBackendTextureHandle texture) const override;
	virtual bool Get_Texture_Description(RenderBackendTextureHandle texture, unsigned level,
		RenderBackendTextureDescription & description) const override;
	virtual bool Lock_Texture(RenderBackendTextureHandle texture, unsigned level,
		RenderBackendTextureLock & locked_texture, bool read_only = false) override;
	virtual void Unlock_Texture(RenderBackendTextureHandle texture, unsigned level) override;
	virtual bool Lock_Cube_Texture(RenderBackendTextureHandle texture, RenderBackendCubeFace face,
		unsigned level, RenderBackendTextureLock & locked_texture, bool read_only = false) override;
	virtual void Unlock_Cube_Texture(RenderBackendTextureHandle texture, RenderBackendCubeFace face,
		unsigned level) override;
	virtual bool Lock_Volume_Texture(RenderBackendTextureHandle texture, unsigned level,
		RenderBackendTextureLock & locked_texture, bool read_only = false) override;
	virtual void Unlock_Volume_Texture(RenderBackendTextureHandle texture, unsigned level) override;
	virtual bool Update_Texture(RenderBackendTextureHandle source,
		RenderBackendTextureHandle destination) override;
	virtual bool Generate_Texture_Mipmaps(RenderBackendTextureHandle texture) override;
	virtual void Set_Texture_LOD(RenderBackendTextureHandle texture, unsigned lod) override;
	virtual unsigned Get_Texture_Priority(RenderBackendTextureHandle texture) const override;
	virtual unsigned Set_Texture_Priority(RenderBackendTextureHandle texture, unsigned priority) override;
	virtual bool Is_Missing_Texture_Handle(RenderBackendTextureHandle texture) const override;
	virtual RenderBackendTextureHandle Create_Missing_Texture() override;
	virtual RenderBackendSurface * Create_Missing_Surface() override;
	virtual void Register_Texture(TextureBaseClass * texture, RenderBackendTextureKind kind,
		unsigned width, unsigned height, unsigned depth, WW3DFormat format,
		WW3DZFormat depth_format, unsigned mip_levels, bool render_target) override;
	virtual void Unregister_Texture(TextureBaseClass * texture) override;
	virtual RenderBackendVertexBuffer * Create_Vertex_Buffer(unsigned size_bytes,
		const RenderBackendVertexLayout &layout, unsigned usage = BUFFER_USAGE_DEFAULT) override;
	virtual RenderBackendIndexBuffer * Create_Index_Buffer(unsigned size_bytes,
		unsigned usage = BUFFER_USAGE_DEFAULT) override;
	virtual bool Lock_Vertex_Buffer(RenderBackendVertexBuffer * buffer,
		unsigned offset_bytes, unsigned size_bytes, void ** data,
		RenderBackendBufferLockMode mode) override;
	virtual bool Lock_Index_Buffer(RenderBackendIndexBuffer * buffer,
		unsigned offset_bytes, unsigned size_bytes, void ** data,
		RenderBackendBufferLockMode mode) override;
	virtual void Unlock_Vertex_Buffer(RenderBackendVertexBuffer * buffer) override;
	virtual void Unlock_Index_Buffer(RenderBackendIndexBuffer * buffer) override;
	virtual void Release_Vertex_Buffer(RenderBackendVertexBuffer * buffer) override;
	virtual void Release_Index_Buffer(RenderBackendIndexBuffer * buffer) override;
	virtual void Set_Vertex_Buffer(RenderBackendVertexBuffer * buffer,
		unsigned offset_bytes, unsigned stride_bytes, unsigned stream = 0) override;
	virtual void Set_Index_Buffer(RenderBackendIndexBuffer * buffer) override;
	virtual void Set_Vertex_Format(RenderBackendVertexFormat format) override;
	virtual bool Process_Vertices(VertexBufferClass * destination, unsigned vertex_count) override;
	virtual void Draw_Indexed_Primitives(RenderBackendPrimitiveType primitive_type,
		unsigned base_vertex_index, unsigned min_vertex_index,
		unsigned vertex_count, unsigned start_index, unsigned primitive_count) override;
	virtual void Draw_Primitive_Up(RenderBackendPrimitiveType primitive_type,
		unsigned primitive_count, const void * vertices, unsigned stride_bytes,
		RenderBackendVertexFormat format) override;
	virtual void Draw_Primitive(RenderBackendPrimitiveType primitive_type,
		unsigned start_vertex, unsigned primitive_count) override;

	virtual void Set_Transform(RenderBackendTransform transform, const Matrix4x4 & matrix) override;
	virtual void Show_Cursor(bool show) override;
	virtual bool Set_Cursor_Properties(int hotspot_x, int hotspot_y, SurfaceClass * surface) override;
	virtual void Set_Cursor_Position(int x, int y) override;
	virtual void Set_Transform(RenderBackendTransform transform, const Matrix3D & matrix) override;
	virtual void Get_Transform(RenderBackendTransform transform, Matrix4x4 & matrix) override;
	virtual void Set_Transform(RenderBackendTransform transform, const float * matrix_elements) override;
	virtual void Get_Transform(RenderBackendTransform transform, float * matrix_elements) override;
	virtual void Set_Render_State(unsigned state, unsigned value) override;
	virtual unsigned Get_Render_State(unsigned state) const override;
	virtual void Set_Texture_Stage_State(unsigned stage, unsigned state, unsigned value) override;

	virtual void Set_Vertex_Shader_Constant(unsigned reg, const void * data,
		unsigned count) override;
	virtual void Set_Pixel_Shader_Constant(unsigned reg, const void * data,
		unsigned count) override;
	virtual bool Create_Pixel_Shader(const void * bytecode, uintptr_t * shader) override;
	virtual bool Create_Pixel_Shader_From_Source(const char * source, uintptr_t * shader) override;
	virtual bool Create_Vertex_Shader(const void * bytecode, uintptr_t * shader,
		const RenderBackendVertexShaderInputLayout * input_layout = nullptr) override;
	virtual void Set_Texture_Resource(unsigned stage, const TextureBaseClass * texture) override;
	virtual void Set_Texture_Handle(unsigned stage, uintptr_t texture) override;
	virtual SurfaceClass * Get_Back_Buffer_Surface() override;
	virtual int Get_Pixel_Shader_Major_Version() const override;
	virtual int Get_Pixel_Shader_Minor_Version() const override;
	virtual void Disable_Light(unsigned index) override;
	virtual void Set_Light_From_State(unsigned index, const RenderBackendLight * light) override;
	virtual void Capture_Render_State(RenderStateStruct & state) override;
	virtual void Apply_Render_State(const RenderStateStruct & state) override;
	virtual void Begin_Backend_Statistics() override;
	virtual void End_Backend_Statistics() override;
	virtual void Restore_Render_State() override;
	virtual void Initialize_Mesh_Renderer() override;
	virtual void Shutdown_Mesh_Renderer() override;
	virtual void Invalidate_Mesh_Renderer(bool shutdown = false) override;
	virtual void Clear_Mesh_Renderer_Delete_Lists() override;
	virtual void Set_Mesh_Renderer_Camera(CameraClass * camera) override;
	virtual void Flush_Mesh_Renderer() override;
	virtual void Register_Mesh_Type(MeshModelClass * mesh) override;
	virtual void Unregister_Mesh_Type(MeshModelClass * mesh) override;
	virtual void Add_Decal_Mesh(DecalMeshClass * mesh) override;
	virtual void Set_Mesh_Renderer_Lighting(bool enable) override;
	virtual void Set_Force_Multiply(bool enable) override;
	virtual void Add_Renderer_Debug_Mesh(MeshClass * mesh) override;
	virtual bool Has_Mesh_Renderers(const MeshModelClass * mesh) const override;
	virtual unsigned Get_Mesh_Renderer_Vertex_Offset(const MeshModelClass * mesh) const override;
	virtual unsigned Get_Mesh_Renderer_Count(const MeshModelClass * mesh) const override;
	virtual void Update_Mesh_Texture(MeshModelClass * mesh, TextureClass * texture,
		TextureClass * new_texture, unsigned pass, unsigned stage) override;
	virtual void Update_Mesh_Material(MeshModelClass * mesh, VertexMaterialClass * material,
		VertexMaterialClass * new_material, unsigned pass) override;
	virtual void Add_Mesh_Render_Tasks(MeshModelClass * mesh, MeshClass * instance) override;
	virtual void Add_Mesh_Material_Pass(MeshModelClass * mesh, MaterialPassClass * pass,
		MeshClass * instance, bool delayed) override;
	virtual void Add_Mesh_Skin(MeshModelClass * mesh, MeshClass * instance) override;
	virtual void Render_Mesh_Pass(MeshModelClass * mesh, int base_vertex_offset) override;
	virtual void Initialize_Sorting_Renderer() override;
	virtual void Shutdown_Sorting_Renderer() override;
	virtual void Set_Sorting_Min_Vertex_Buffer_Size(unsigned value) override;
	virtual void Insert_Sorted_Triangles(const SphereClass & bounding_sphere,
		unsigned short start_index, unsigned short polygon_count,
		unsigned short min_vertex_index, unsigned short vertex_count) override;
	virtual void Insert_Sorted_Triangles(unsigned short start_index,
		unsigned short polygon_count, unsigned short min_vertex_index,
		unsigned short vertex_count) override;
	virtual void Flush_Sorting_Renderer() override;

	/*
	** Device Selection Code.
	** For backward compatibility, the public interface for these functions is in the ww3d.
	** header file.  These functions are protected so that we aren't exposing two interfaces.
	*/
	bool Set_Any_Render_Device();
	bool	Set_Render_Device(const char * dev_name,int width=-1,int height=-1,int bits=-1,int windowed=-1,bool resize_window=false);
	bool	Set_Render_Device(int dev=-1,int resx=-1,int resy=-1,int bits=-1,int windowed=-1,bool resize_window = false, bool reset_device = false, bool restore_assets=true);
	void Set_Fullscreen_Mode(RenderBackendFullscreenMode mode) override;
	bool Set_Next_Render_Device();
	bool Toggle_Windowed();

	int	Get_Render_Device_Count() const;
	int	Get_Render_Device() const;
	const RenderDeviceDescClass & Get_Render_Device_Desc(int deviceidx) const;
	const char * Get_Render_Device_Name(int device_index) const;
	bool Set_Device_Resolution(int width=-1,int height=-1,int bits=-1,int windowed=-1, bool resize_window=false);
	void Get_Device_Resolution(int & set_w,int & set_h,int & set_bits,bool & set_windowed) const;
	void Get_Render_Target_Resolution(int & set_w,int & set_h,int & set_bits,bool & set_windowed) const;
	int	Get_Device_Resolution_Width() const { return ResolutionWidth; }
	int	Get_Device_Resolution_Height() const { return ResolutionHeight; }

	bool Registry_Save_Render_Device( const char *sub_key, int device, int width, int height, int depth, bool windowed, int texture_depth);
	bool Registry_Load_Render_Device( const char * sub_key, char *device, int device_len, int &width, int &height, int &depth, int &windowed, int &texture_depth);
	bool Is_Windowed() const { return IsWindowed; }

	void	Set_Texture_Bitdepth(int depth)	{ WWASSERT(depth==16 || depth==32); TextureBitDepth = depth; }
	int	Get_Texture_Bitdepth() const			{ return TextureBitDepth; }

	static void Set_MSAA_Mode(D3DMULTISAMPLE_TYPE mode) { MultiSampleAntiAliasing = mode; }
	static D3DMULTISAMPLE_TYPE Get_MSAA_Mode() { return MultiSampleAntiAliasing; }

	void	Set_Swap_Interval(int swap);
	int	Get_Swap_Interval() const;
	static void Set_Polygon_Mode(int mode);

	/*
	** Internal functions
	*/
	void Release_Render_Target_Cache();
	void Resize_And_Position_Window();
	bool Find_Color_And_Z_Mode(int resx,int resy,int bitdepth,D3DFORMAT * set_colorbuffer,D3DFORMAT * set_backbuffer, D3DFORMAT * set_zmode);
	bool Find_Color_Mode(D3DFORMAT colorbuffer, int resx, int resy, UINT *mode);
	bool Find_Z_Mode(D3DFORMAT colorbuffer,D3DFORMAT backbuffer, D3DFORMAT *zmode);
	bool Test_Z_Mode(D3DFORMAT colorbuffer,D3DFORMAT backbuffer, D3DFORMAT zmode);
	void Compute_Caps(WW3DFormat display_format);
	bool Set_Vertex_Shader_Input_Layout(const RenderBackendVertexShaderInputLayout & layout);
	void Release_Vertex_Shader_Input_Layout();

	/*
	** Protected Member Variables
	*/

	static RenderStateStruct			render_state;
	static unsigned						render_state_changed;
	static D3DMATRIX						DX9Transforms[D3DTS_WORLD+1];

	static bool								IsInitted;
	static bool								IsDeviceLost;
	static void *							Hwnd;
	static unsigned						_MainThreadID;

	static bool								_EnableTriangleDraw;

	static int								CurRenderDevice;
	static int								ResolutionWidth;
	static int								ResolutionHeight;
	static int								BitDepth;
	static int								TextureBitDepth;
	static bool								IsWindowed;
	static RenderBackendFullscreenMode		FullscreenMode;
	static DWORD								PresentationInterval;
	static D3DFORMAT					DisplayFormat;
	static D3DMULTISAMPLE_TYPE	MultiSampleAntiAliasing;

	// shader system updates KJM v
	static uintptr_t					Vertex_Shader;
	static uintptr_t					Pixel_Shader;

	static Vector4							Vertex_Shader_Constants[MAX_VERTEX_SHADER_CONSTANTS];
	static Vector4							Pixel_Shader_Constants[MAX_PIXEL_SHADER_CONSTANTS];

	static LightEnvironmentClass*		Light_Environment;

	static DWORD							Vertex_Processing_Behavior;

	static ZTextureClass*				Shadow_Map[MAX_SHADOW_MAPS];

	static Vector3							Ambient_Color;
	// shader system updates KJM ^

	static bool								world_identity;
	static unsigned						RenderStates[256];
	static unsigned						TextureStageStates[MAX_TEXTURE_STAGES][32];
	static IDirect3DBaseTexture9 *	Textures[MAX_TEXTURE_STAGES];

	// These fog settings are constant for all objects in a given scene,
	// unlike the matching renderstates which vary based on shader settings.
	static bool								FogEnable;
	static D3DCOLOR						FogColor;

	static RenderFrameStatistics			FrameStatistics;
	static bool								CurrentDX9LightEnables[4];

	static unsigned long FrameCount;

	static DX9Caps*						CurrentCaps;

	static D3DADAPTER_IDENTIFIER9		CurrentAdapterIdentifier;
	static HRESULT						LastCreateDeviceHRESULT;
	static char							LastSetRenderDeviceError[128];

	static IDirect3D9 *					D3DInterface;
	static IDirect3DDevice9 *			D3DDevice;
	static IDirect3DVertexDeclaration9 *	Vertex_Declaration;

	static IDirect3DSurface9 *			CurrentRenderTarget;
	static IDirect3DSurface9 *			CurrentDepthBuffer;
	static IDirect3DSurface9 *			DefaultRenderTarget;
	static IDirect3DSurface9 *			DefaultDepthBuffer;

	static unsigned							DrawPolygonLowBoundLimit;

	static bool								IsRenderToTexture;

	static int								ZBias;
	static float							ZNear;
	static float							ZFar;
	static D3DMATRIX					ProjectionMatrix;

	friend void DX9_Assert();
	friend class WW3D;
	friend class DX9Backend;
	bool Lite;
	RenderBackendDebugSettings DebugSettings;
	RenderBackendCleanupHook * CleanupHook;
};

// shader system updates KJM v
WWINLINE void DX9Backend::Set_Vertex_Shader(uintptr_t vertex_shader,
	const RenderBackendVertexShaderInputLayout * input_layout)
{
#if 0 //(gth) some code is bypassing this accessor function so we can't count on this variable...
	// may be incorrect if shaders are created and destroyed dynamically
	if (Vertex_Shader==vertex_shader) return;
#endif

	if (input_layout != nullptr)
	{
		Set_Vertex_Shader_Input_Layout(*input_layout);
	}

	Vertex_Shader=vertex_shader;
	if (Vertex_Shader == 0) {
		DX9CALL(SetVertexShader(nullptr));
		return;
	}
	if (Vertex_Shader < 0x10000 || (Vertex_Shader & D3DFVF_RESERVED0) != 0) {
		DX9CALL(SetVertexShader(nullptr));
		DX9CALL(SetFVF(Vertex_Shader));
	} else {
		IDirect3DVertexShader9* const shader = reinterpret_cast<IDirect3DVertexShader9*>(Vertex_Shader);
		if (!Is_Valid_D3D_Object_Ptr(shader, "Set_Vertex_Shader")) {
			DX9CALL(SetVertexShader(nullptr));
			Vertex_Shader = 0;
			return;
		}
		DX9CALL(SetVertexShader(shader));
	}
}

WWINLINE void DX9Backend::Release_Vertex_Shader(uintptr_t shader)
{
	if (shader != 0 && shader >= 0x10000)
		reinterpret_cast<IDirect3DVertexShader9 *>(shader)->Release();
}

WWINLINE void DX9Backend::Release_Pixel_Shader(uintptr_t shader)
{
	if (shader != 0)
		reinterpret_cast<IDirect3DPixelShader9 *>(shader)->Release();
}

WWINLINE void DX9Backend::Set_Vertex_Shader_Constant(int reg, const void* data, int count)
{
	int memsize=sizeof(Vector4)*count;

	// may be incorrect if shaders are created and destroyed dynamically
	if (memcmp(data, &Vertex_Shader_Constants[reg],memsize)==0) return;

	memcpy(&Vertex_Shader_Constants[reg],data,memsize);
	DX9CALL(SetVertexShaderConstantF(reg,static_cast<const float*>(data),count));
}

WWINLINE void DX9Backend::Set_Pixel_Shader_Constant(int reg, const void* data, int count)
{
	int memsize=sizeof(Vector4)*count;

	// may be incorrect if shaders are created and destroyed dynamically
	if (memcmp(data, &Pixel_Shader_Constants[reg],memsize)==0) return;

	memcpy(&Pixel_Shader_Constants[reg],data,memsize);
	DX9CALL(SetPixelShaderConstantF(reg,static_cast<const float*>(data),count));
}
// shader system updates KJM ^

WWINLINE void DX9Backend::_Set_DX9_Transform(D3DTRANSFORMSTATETYPE transform, const D3DMATRIX& m)
{
	WWASSERT(transform<=D3DTS_WORLD);
#if 0 // (gth) this optimization is breaking generals because they set the transform behind our backs.
	if (mtx!=DX9Transforms[transform])
#endif
	{
		DX9Transforms[transform]=m;
		SNAPSHOT_SAY(("DX9 - SetTransform %d [%f,%f,%f,%f][%f,%f,%f,%f][%f,%f,%f,%f]",
			transform,
			m.m[0][0],m.m[0][1],m.m[0][2],m.m[0][3],
			m.m[1][0],m.m[1][1],m.m[1][2],m.m[1][3],
			m.m[2][0],m.m[2][1],m.m[2][2],m.m[2][3]));
		DX9_RECORD_MATRIX_CHANGE();
		DX9CALL(SetTransform(transform,&m));
	}
}

WWINLINE void DX9Backend::_Get_DX9_Transform(D3DTRANSFORMSTATETYPE transform, D3DMATRIX& m)
{
	DX9CALL(GetTransform(transform,&m));
}

// ----------------------------------------------------------------------------
//
// Set the index offset for the current index buffer
//
// ----------------------------------------------------------------------------

WWINLINE void DX9Backend::Set_Index_Buffer_Index_Offset(unsigned offset)
{
	if (render_state.index_base_offset==offset) return;
	render_state.index_base_offset=offset;
	render_state_changed|=INDEX_BUFFER_CHANGED;
}

// ----------------------------------------------------------------------------
// Set the fog settings. This function should be used, rather than setting the
// appropriate renderstates directly, because the shader sets some of the
// renderstates on a per-mesh / per-pass basis depending on global fog states
// (stored in the wrapper) as well as the shader settings.
// This function should be called rarely - once per scene would be appropriate.
// ----------------------------------------------------------------------------

WWINLINE void DX9Backend::Set_Fog(bool enable, const Vector3 &color, float start, float end)
{
	// Set global states
	FogEnable = enable;
	FogColor = Convert_Color(color,0.0f);

	// Invalidate the current shader (since the renderstates set by the shader
	// depend on the global fog settings as well as the actual shader settings)
	ShaderClass::Invalidate();

	// Set renderstates which are not affected by the shader
	Set_DX9_Render_State(D3DRS_FOGSTART, *(DWORD *)(&start));
	Set_DX9_Render_State(D3DRS_FOGEND,   *(DWORD *)(&end));
}

WWINLINE void DX9Backend::Set_Ambient(const Vector3& color)
{
	Ambient_Color=color;
	Set_DX9_Render_State(D3DRS_AMBIENT, DX9Backend::Convert_Color(color,0.0f));
}

// ----------------------------------------------------------------------------
//
// Set vertex buffer to be used in the subsequent render calls. If there was
// a vertex buffer being used earlier, release the reference to it. Passing
// nullptr just will release the vertex buffer.
//
// ----------------------------------------------------------------------------

WWINLINE void DX9Backend::Set_DX9_Material(const D3DMATERIAL9* mat)
{
	DX9_RECORD_MATERIAL_CHANGE();
	WWASSERT(mat);
	SNAPSHOT_SAY(("DX9 - SetMaterial"));
	DX9CALL(SetMaterial(mat));
}

WWINLINE void DX9Backend::Set_DX9_Light(int index, D3DLIGHT9* light)
{
	if (light) {
		DX9_RECORD_LIGHT_CHANGE();
		DX9CALL(SetLight(index,light));
		DX9CALL(LightEnable(index,TRUE));
		CurrentDX9LightEnables[index]=true;
		SNAPSHOT_SAY(("DX9 - SetLight %d",index));
	}
	else if (CurrentDX9LightEnables[index]) {
		DX9_RECORD_LIGHT_CHANGE();
		CurrentDX9LightEnables[index]=false;
		DX9CALL(LightEnable(index,FALSE));
		SNAPSHOT_SAY(("DX9 - DisableLight %d",index));
	}
}

WWINLINE void DX9Backend::Set_DX9_Render_State(D3DRENDERSTATETYPE state, unsigned value)
{
	if (state == D3DRS_LINEPATTERN ||
		state == D3DRS_ZVISIBLE ||
		state == D3DRS_EDGEANTIALIAS ||
		state == D3DRS_SOFTWAREVERTEXPROCESSING ||
		state == D3DRS_PATCHSEGMENTS) {
		return;
	}

	// Can't monitor state changes because setShader call to GERD may change the states!
	if (RenderStates[state]==value) return;

#ifdef MESH_RENDER_SNAPSHOT_ENABLED
	if (WW3D::Is_Snapshot_Activated()) {
		StringClass value_name(0,true);
		Get_DX9_Render_State_Value_Name(value_name,state,value);
		SNAPSHOT_SAY(("DX9 - SetRenderState(state: %s, value: %s)",
			Get_DX9_Render_State_Name(state),
			value_name.str()));
	}
#endif

	RenderStates[state]=value;
	DX9CALL(SetRenderState( state, value ));
	DX9_RECORD_RENDER_STATE_CHANGE();
}

WWINLINE void DX9Backend::Set_DX9_Clip_Plane(DWORD Index, CONST float* pPlane)
{
	DX9CALL(SetClipPlane( Index, pPlane ));
}

WWINLINE void DX9Backend::Set_DX9_Texture_Stage_State(unsigned stage, D3DTEXTURESTAGESTATETYPE state, unsigned value)
{
	D3DSAMPLERSTATETYPE sampler_state = D3DSAMP_FORCE_DWORD;
	switch (state) {
	case D3DTSS_ADDRESSU: sampler_state = D3DSAMP_ADDRESSU; break;
	case D3DTSS_ADDRESSV: sampler_state = D3DSAMP_ADDRESSV; break;
	case D3DTSS_BORDERCOLOR: sampler_state = D3DSAMP_BORDERCOLOR; break;
	case D3DTSS_MAGFILTER: sampler_state = D3DSAMP_MAGFILTER; break;
	case D3DTSS_MINFILTER: sampler_state = D3DSAMP_MINFILTER; break;
	case D3DTSS_MIPFILTER: sampler_state = D3DSAMP_MIPFILTER; break;
	case D3DTSS_MIPMAPLODBIAS: sampler_state = D3DSAMP_MIPMAPLODBIAS; break;
	case D3DTSS_MAXMIPLEVEL: sampler_state = D3DSAMP_MAXMIPLEVEL; break;
	case D3DTSS_MAXANISOTROPY: sampler_state = D3DSAMP_MAXANISOTROPY; break;
	case D3DTSS_ADDRESSW: sampler_state = D3DSAMP_ADDRESSW; break;
	default: break;
	}
	if (sampler_state != D3DSAMP_FORCE_DWORD) {
		DX9CALL(SetSamplerState(stage, sampler_state, value));
		DX9_RECORD_TEXTURE_STAGE_STATE_CHANGE();
		return;
	}

  	if (stage >= MAX_TEXTURE_STAGES)
  	{	DX9CALL(SetTextureStageState( stage, state, value ));
  		return;
  	}

	// Can't monitor state changes because setShader call to GERD may change the states!
	if (TextureStageStates[stage][(unsigned int)state]==value) return;
#ifdef MESH_RENDER_SNAPSHOT_ENABLED
	if (WW3D::Is_Snapshot_Activated()) {
		StringClass value_name(0,true);
		Get_DX9_Texture_Stage_State_Value_Name(value_name,state,value);
		SNAPSHOT_SAY(("DX9 - SetTextureStageState(stage: %d, state: %s, value: %s)",
			stage,
			Get_DX9_Texture_Stage_State_Name(state),
			value_name.str()));
	}
#endif

	TextureStageStates[stage][(unsigned int)state]=value;
	DX9CALL(SetTextureStageState( stage, state, value ));
	DX9_RECORD_TEXTURE_STAGE_STATE_CHANGE();
}

WWINLINE void DX9Backend::Set_DX9_Texture(unsigned int stage, IDirect3DBaseTexture9* texture)
{
	if (!Is_Valid_D3D_Object_Ptr(texture, "Set_DX9_Texture")) {
		texture = nullptr;
	}

  	if (stage >= MAX_TEXTURE_STAGES)
  	{	DX9CALL(SetTexture(stage, texture));
  		return;
  	}

	SNAPSHOT_SAY(("DX9 - SetTexture(%x) ",texture));

	if (Textures[stage] != texture)
	{
		if (Textures[stage]) Textures[stage]->Release();
		Textures[stage] = texture;
		if (Textures[stage]) Textures[stage]->AddRef();
	}
	DX9CALL(SetTexture(stage, texture));
	DX9_RECORD_TEXTURE_CHANGE();
}

WWINLINE Vector4 DX9Backend::Convert_Color(unsigned color)
{
	Vector4 col;
	col[3]=((color&0xff000000)>>24)/255.0f;
	col[0]=((color&0xff0000)>>16)/255.0f;
	col[1]=((color&0xff00)>>8)/255.0f;
	col[2]=((color&0xff)>>0)/255.0f;
//	col=Vector4(1.0f,1.0f,1.0f,1.0f);
	return col;
}

#if 0
WWINLINE unsigned int DX9Backend::Convert_Color(const Vector3& color, const float alpha)
{
	WWASSERT(color.X<=1.0f);
	WWASSERT(color.Y<=1.0f);
	WWASSERT(color.Z<=1.0f);
	WWASSERT(alpha<=1.0f);
	WWASSERT(color.X>=0.0f);
	WWASSERT(color.Y>=0.0f);
	WWASSERT(color.Z>=0.0f);
	WWASSERT(alpha>=0.0f);

	return D3DCOLOR_COLORVALUE(color.X,color.Y,color.Z,alpha);
}
WWINLINE unsigned int DX9Backend::Convert_Color(const Vector4& color)
{
	WWASSERT(color.X<=1.0f);
	WWASSERT(color.Y<=1.0f);
	WWASSERT(color.Z<=1.0f);
	WWASSERT(color.W<=1.0f);
	WWASSERT(color.X>=0.0f);
	WWASSERT(color.Y>=0.0f);
	WWASSERT(color.Z>=0.0f);
	WWASSERT(color.W>=0.0f);

	return D3DCOLOR_COLORVALUE(color.X,color.Y,color.Z,color.W);
}
#else

// ----------------------------------------------------------------------------
//
// Convert RGBA color from float vector to 32 bit integer
// Note: Color vector needs to be clamped to [0...1] range!
//
// ----------------------------------------------------------------------------

WWINLINE unsigned int DX9Backend::Convert_Color(const Vector3& color,float alpha)
{
#if defined(_MSC_VER) && _MSC_VER < 1300
	const float scale = 255.0;
	unsigned int col;

	// Multiply r, g, b and a components (0.0,...,1.0) by 255 and convert to integer. Or the integer values togerher
	// such that 32 bit integer has AAAAAAAARRRRRRRRGGGGGGGGBBBBBBBB.
	__asm
	{
		sub	esp,20					// space for a, r, g and b float plus fpu rounding mode

		// Store the fpu rounding mode

		fwait
		fstcw		[esp+16]				// store control word to stack
		mov		eax,[esp+16]		// load it to eax
		mov		edi,eax				// take copy
		and		eax,~(1024|2048)	// mask out certain bits
		or			eax,(1024|2048)	// or with precision control value "truncate"
		sub		edi,eax				// did it change?
		jz			skip					// .. if not, skip
		mov		[esp],eax			// .. change control word
		fldcw		[esp]
skip:

		// Convert the color

		mov	esi,dword ptr color
		fld	dword ptr[scale]

		fld	dword ptr[esi]			// r
		fld	dword ptr[esi+4]		// g
		fld	dword ptr[esi+8]		// b
		fld	dword ptr[alpha]		// a
		fld	st(4)
		fmul	st(4),st
		fmul	st(3),st
		fmul	st(2),st
		fmulp	st(1),st
		fistp	dword ptr[esp+0]		// a
		fistp	dword ptr[esp+4]		// b
		fistp	dword ptr[esp+8]		// g
		fistp	dword ptr[esp+12]		// r
		mov	ecx,[esp]				// a
		mov	eax,[esp+4]				// b
		mov	edx,[esp+8]				// g
		mov	ebx,[esp+12]			// r
		shl	ecx,24					// a << 24
		shl	ebx,16					// r << 16
		shl	edx,8						//	g << 8
		or		eax,ecx					// (a << 24) | b
		or		eax,ebx					// (a << 24) | (r << 16) | b
		or		eax,edx					// (a << 24) | (r << 16) | (g << 8) | b

		fstp	st(0)

		// Restore fpu rounding mode

		cmp	edi,0					// did we change the value?
		je		not_changed			// nope... skip now...
		fwait
		fldcw	[esp+16];
not_changed:
		add	esp,20

		mov	col,eax
	}
	return col;
#else
	return color.Convert_To_ARGB(alpha);
#endif // defined(_MSC_VER) && _MSC_VER < 1300
}

// ----------------------------------------------------------------------------
//
// Clamp color vector to [0...1] range
//
// ----------------------------------------------------------------------------

WWINLINE void DX9Backend::Clamp_Color(Vector4& color)
{
#if defined(_MSC_VER) && _MSC_VER < 1300
	if (CPUDetectClass::Has_CMOV_Instruction()) {
	__asm
	{
		mov	esi,dword ptr color

		mov edx,0x3f800000

		mov edi,dword ptr[esi]
		mov ebx,edi
		sar edi,31
		not edi			// mask is now zero if negative value
		and edi,ebx
		cmp edi,edx		// if no less than 1.0 set to 1.0
		cmovnb edi,edx
		mov dword ptr[esi],edi

		mov edi,dword ptr[esi+4]
		mov ebx,edi
		sar edi,31
		not edi			// mask is now zero if negative value
		and edi,ebx
		cmp edi,edx		// if no less than 1.0 set to 1.0
		cmovnb edi,edx
		mov dword ptr[esi+4],edi

		mov edi,dword ptr[esi+8]
		mov ebx,edi
		sar edi,31
		not edi			// mask is now zero if negative value
		and edi,ebx
		cmp edi,edx		// if no less than 1.0 set to 1.0
		cmovnb edi,edx
		mov dword ptr[esi+8],edi

		mov edi,dword ptr[esi+12]
		mov ebx,edi
		sar edi,31
		not edi			// mask is now zero if negative value
		and edi,ebx
		cmp edi,edx		// if no less than 1.0 set to 1.0
		cmovnb edi,edx
		mov dword ptr[esi+12],edi
	}
	return;
	}
#endif // defined(_MSC_VER) && _MSC_VER < 1300

	for (int i=0;i<4;++i) {
		float f=(color[i]<0.0f) ? 0.0f : color[i];
		color[i]=(f>1.0f) ? 1.0f : f;
	}
}

// ----------------------------------------------------------------------------
//
// Convert RGBA color from float vector to 32 bit integer
//
// ----------------------------------------------------------------------------

WWINLINE unsigned int DX9Backend::Convert_Color(const Vector4& color)
{
	return Convert_Color(reinterpret_cast<const Vector3&>(color),color[3]);
}

WWINLINE unsigned int DX9Backend::Convert_Color_Clamp(const Vector4& color)
{
	Vector4 clamped_color=color;
	DX9Backend::Clamp_Color(clamped_color);
	return Convert_Color(reinterpret_cast<const Vector3&>(clamped_color),clamped_color[3]);
}

#endif

WWINLINE void DX9Backend::Set_Alpha (const float alpha, unsigned int &color)
{
	unsigned char *component = (unsigned char*) &color;

	component [3] = 255.0f * alpha;
}

WWINLINE void DX9Backend::Get_Render_State(RenderStateStruct& state)
{
	state=render_state;
}

WWINLINE void DX9Backend::Get_Shader(ShaderClass& shader)
{
	shader=ShaderClass(render_state.shader_bits);
}

WWINLINE void DX9Backend::Set_Texture(unsigned stage,TextureBaseClass* texture)
{
	WWASSERT(stage<(unsigned int)CurrentCaps->Get_Max_Textures_Per_Pass());
	if (texture==render_state.Textures[stage]) return;
	REF_PTR_SET(render_state.Textures[stage],texture);
	render_state_changed|=(TEXTURE0_CHANGED<<stage);
}

WWINLINE void DX9Backend::Set_Material(const VertexMaterialClass* material)
{
/*	if (material && render_state.material &&
		// !stricmp(material->Get_Name(),render_state.material->Get_Name())) {
		material->Get_CRC()!=render_state.material->Get_CRC()) {
		return;
	}
*/
//	if (material==render_state.material) {
//		return;
//	}
	REF_PTR_SET(render_state.material,const_cast<VertexMaterialClass*>(material));
	render_state_changed|=MATERIAL_CHANGED;
	SNAPSHOT_SAY(("DX9Backend::Set_Material(%s)",material ? material->Get_Name() : "null"));
}

WWINLINE void DX9Backend::Set_Shader(const ShaderClass& shader)
{
	// ShaderClass describes the fixed-function WW3D state.  It must not leave a
	// custom vertex shader selected from an earlier backend-only draw (trees and
	// water are the current users of that path).  The vertex buffer application
	// below will select the buffer's fixed-function format when this draw is
	// submitted.  A custom shader caller can still select its shader explicitly
	// after setting the ShaderClass state.
	if (Vertex_Shader >= 0x10000 && (Vertex_Shader & D3DFVF_RESERVED0) == 0) {
		DX9CALL(SetVertexShader(nullptr));
		Vertex_Shader = 0;
	}

	if (!ShaderClass::Is_Dirty() && shader.Get_Bits()==render_state.shader_bits) {
		return;
	}
	render_state.shader_bits=shader.Get_Bits();
	render_state_changed|=SHADER_CHANGED;
#ifdef MESH_RENDER_SNAPSHOT_ENABLED
	StringClass str;
#endif
	SNAPSHOT_SAY(("DX9Backend::Set_Shader(%s)",shader.Get_Description(str).str()));
}

WWINLINE void DX9Backend::Set_Projection_Transform_With_Z_Bias(const Matrix4x4& matrix, float znear, float zfar)
{
	ZFar=zfar;
	ZNear=znear;
	ProjectionMatrix=To_D3DMATRIX(matrix);

	if (!Get_Current_Caps()->Support_ZBias() && ZNear!=ZFar) {
		D3DMATRIX tmp=ProjectionMatrix;
		float tmp_zbias=ZBias;
		tmp_zbias*=(1.0f/16.0f);
		tmp_zbias*=1.0f / (ZFar - ZNear);
		tmp.m[2][2]-=tmp_zbias*tmp.m[3][2];
		DX9CALL(SetTransform(D3DTS_PROJECTION,&tmp));
	}
	else {
		DX9CALL(SetTransform(D3DTS_PROJECTION,&ProjectionMatrix));
	}
}

WWINLINE void DX9Backend::Set_Transform(D3DTRANSFORMSTATETYPE transform,const Matrix4x4& m)
{
	switch ((int)transform) {
	case D3DTS_WORLD:
		render_state.world=m;
		render_state_changed|=(unsigned)WORLD_CHANGED;
		render_state_changed&=~(unsigned)WORLD_IDENTITY;
		break;
	case D3DTS_VIEW:
		render_state.view=m;
		render_state_changed|=(unsigned)VIEW_CHANGED;
		render_state_changed&=~(unsigned)VIEW_IDENTITY;
		break;
	case D3DTS_PROJECTION:
		{
			D3DMATRIX ProjectionMatrix=To_D3DMATRIX(m);
			ZFar=0.0f;
			ZNear=0.0f;
			DX9CALL(SetTransform(D3DTS_PROJECTION,&ProjectionMatrix));
		}
		break;
	default:
		DX9_RECORD_MATRIX_CHANGE();
		D3DMATRIX dxm=To_D3DMATRIX(m);
		DX9CALL(SetTransform(transform,&dxm));
		break;
	}
}

WWINLINE void DX9Backend::Set_Transform(D3DTRANSFORMSTATETYPE transform,const Matrix3D& m)
{
	switch ((int)transform) {
	case D3DTS_WORLD:
		render_state.world=Matrix4x4(m);
		render_state_changed|=(unsigned)WORLD_CHANGED;
		render_state_changed&=~(unsigned)WORLD_IDENTITY;
		break;
	case D3DTS_VIEW:
		render_state.view=Matrix4x4(m);
		render_state_changed|=(unsigned)VIEW_CHANGED;
		render_state_changed&=~(unsigned)VIEW_IDENTITY;
		break;
	default:
		DX9_RECORD_MATRIX_CHANGE();
		D3DMATRIX dxm=To_D3DMATRIX(m);
		DX9CALL(SetTransform(transform,&dxm));
		break;
	}
}

WWINLINE bool DX9Backend::Is_World_Identity()
{
	return !!(render_state_changed&(unsigned)WORLD_IDENTITY);
}

WWINLINE bool DX9Backend::Is_View_Identity()
{
	return !!(render_state_changed&(unsigned)VIEW_IDENTITY);
}

WWINLINE void DX9Backend::Get_Transform(D3DTRANSFORMSTATETYPE transform, Matrix4x4& m)
{
	switch ((int)transform) {
	case D3DTS_WORLD:
		if (render_state_changed&WORLD_IDENTITY) m.Make_Identity();
		else m=render_state.world;
		break;
	case D3DTS_VIEW:
		if (render_state_changed&VIEW_IDENTITY) m.Make_Identity();
		else m=render_state.view;
		break;
	default:
		D3DMATRIX dxm;
		DX9CALL(GetTransform(transform,&dxm));
		m=To_Matrix4x4(dxm);
		break;
	}
}

WWINLINE const RenderBackendLight& DX9Backend::Peek_Light(unsigned index)
{
	return render_state.Lights[index];
}

WWINLINE bool DX9Backend::Is_Light_Enabled(unsigned index)
{
	return render_state.LightEnable[index];
}

WWINLINE void DX9Backend::Set_Render_State(const RenderStateStruct& state)
{
	int i;

	if (render_state.index_buffer) {
		render_state.index_buffer->Release_Engine_Ref();
	}

	for (i=0;i<MAX_VERTEX_STREAMS;++i)
	{
		if (render_state.vertex_buffers[i])
		{
			render_state.vertex_buffers[i]->Release_Engine_Ref();
		}
	}

	render_state=state;
	render_state_changed=0xffffffff;

	if (render_state.index_buffer) {
		render_state.index_buffer->Add_Engine_Ref();
	}

	for (i=0;i<MAX_VERTEX_STREAMS;++i)
	{
		if (render_state.vertex_buffers[i])
		{
			render_state.vertex_buffers[i]->Add_Engine_Ref();
		}
	}
}

WWINLINE void DX9Backend::Release_Render_State()
{
	int i;

	if (render_state.index_buffer) {
		render_state.index_buffer->Release_Engine_Ref();
	}

	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		if (render_state.vertex_buffers[i]) {
			render_state.vertex_buffers[i]->Release_Engine_Ref();
		}
	}

	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		REF_PTR_RELEASE(render_state.vertex_buffers[i]);
	}
	REF_PTR_RELEASE(render_state.index_buffer);
	REF_PTR_RELEASE(render_state.material);

	for (i=0;i<MAX_TEXTURE_STAGES;++i)
	{
		REF_PTR_RELEASE(render_state.Textures[i]);
	}
}
