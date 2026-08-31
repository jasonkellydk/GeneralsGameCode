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

// TheSuperHackers @refactor Render backend interface. Keep this contract free
// of graphics API and platform types so another backend can implement it.

#pragma once

#include <stdint.h>

#include "WW3D2/Buffer.h"
#include "WW3D2/VertexFormat.h"
#include "WW3D2/WW3DFormat.h"

class LightEnvironmentClass;
class LightClass;
class Matrix3D;
class Matrix4x4;
class DynamicIBAccessClass;
class DynamicVBAccessClass;
class IndexBufferClass;
class CameraClass;
class DecalMeshClass;
class MaterialPassClass;
class MeshClass;
class MeshModelClass;
class ShaderClass;
class TextureClass;
class TextureBaseClass;
class ZTextureClass;
class SphereClass;
struct RenderStateStruct;
class VertexBufferClass;
class VertexMaterialClass;
class Vector3;
class RenderDeviceDescClass;
class SurfaceClass;
class Vector4;

// These limits are part of the WW3D contract, rather than a property of one
// graphics API.  Backends may support fewer active stages, but the state
// objects and callers use these stable upper bounds.
constexpr unsigned MAX_TEXTURE_STAGES = 8;
constexpr unsigned MAX_VERTEX_STREAMS = 2;
constexpr unsigned MAX_VERTEX_SHADER_CONSTANTS = 96;
constexpr unsigned MAX_PIXEL_SHADER_CONSTANTS = 8;
constexpr unsigned MAX_SHADOW_MAPS = 1;
constexpr unsigned RENDER_BACKEND_MAX_VERTEX_INPUT_ELEMENTS = 16;

// A programmable vertex shader can use a different register layout from the
// fixed-function WW3D vertex format.  This describes that input contract
// without exposing a graphics API declaration or resource handle to callers.
enum class RenderBackendVertexInputType
{
	Float1,
	Float2,
	Float3,
	Float4,
	Color
};

enum class RenderBackendVertexInputSemantic
{
	Position,
	BlendWeight,
	BlendIndices,
	Normal,
	PointSize,
	Color,
	TextureCoordinate
};

struct RenderBackendVertexInputElement
{
	unsigned stream;
	unsigned offset;
	RenderBackendVertexInputType type;
	RenderBackendVertexInputSemantic semantic;
	unsigned semantic_index;
	unsigned shader_register;
};

struct RenderBackendVertexShaderInputLayout
{
	unsigned element_count;
	RenderBackendVertexInputElement elements[
		RENDER_BACKEND_MAX_VERTEX_INPUT_ELEMENTS];

	RenderBackendVertexShaderInputLayout() : element_count(0)
	{
	}

	bool Add(unsigned stream, unsigned offset,
		RenderBackendVertexInputType type,
		RenderBackendVertexInputSemantic semantic,
		unsigned semantic_index, unsigned shader_register)
	{
		if (element_count >= RENDER_BACKEND_MAX_VERTEX_INPUT_ELEMENTS)
		{
			return false;
		}

		elements[element_count++] = {
			stream, offset, type, semantic, semantic_index, shader_register};
		return true;
	}
};

enum class RenderBackendTransform
{
	World,
	View,
	Projection,
	Texture0,
	Texture1,
	Texture2,
	Texture3,
	Texture4,
	Texture5,
	Texture6,
	Texture7
};

// A backend may present a fullscreen application through an exclusive swap
// chain or through a borderless window-system surface. This is separate from
// the logical windowed state reported by Is_Windowed().
enum class RenderBackendFullscreenMode
{
	Exclusive,
	Borderless
};

inline RenderBackendTransform RenderBackend_Texture_Transform(unsigned stage)
{
	return static_cast<RenderBackendTransform>(
		static_cast<unsigned>(RenderBackendTransform::Texture0) + stage);
}

enum class RenderBackendFillMode
{
	Point,
	Wireframe,
	Solid
};

enum class RenderBackendCullMode
{
	None,
	Clockwise,
	CounterClockwise
};

enum class RenderBackendShadeMode
{
	Flat,
	Gouraud,
	Phong
};

enum class RenderBackendMaterialSource
{
	MaterialValue,
	Color1,
	Color2
};

enum class RenderBackendColorWriteMask : unsigned
{
	None = 0,
	Red = 1u,
	Green = 2u,
	Blue = 4u,
	Alpha = 8u,
	RGB = 7u,
	All = 15u
};

enum class RenderBackendBlendOperation
{
	Add,
	Subtract,
	ReverseSubtract,
	Minimum,
	Maximum
};

enum class RenderBackendBlendFactor
{
	Zero,
	One,
	SourceColor,
	InverseSourceColor,
	SourceAlpha,
	InverseSourceAlpha,
	DestinationAlpha,
	InverseDestinationAlpha,
	DestinationColor,
	InverseDestinationColor
};

enum class RenderBackendCompareFunction
{
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always
};

enum class RenderBackendStencilOperation
{
	Keep,
	Zero,
	Replace,
	IncrementSaturate,
	DecrementSaturate,
	Invert,
	Increment,
	Decrement
};

enum class RenderBackendTextureOperation
{
	Disable,
	SelectArgument1,
	SelectArgument2,
	Modulate,
	AddSmooth,
	Add,
	Subtract,
	BlendTextureAlpha,
	BlendCurrentAlpha,
	AddSigned,
	AddSigned2X,
	Modulate2X,
	ModulateAlphaAddColor,
	BumpEnvironmentMap,
	BumpEnvironmentMapLuminance,
	DotProduct3,
	MultiplyAdd
};

enum class RenderBackendTextureComponent
{
	Color,
	Alpha
};

enum class RenderBackendTextureArgument
{
	Current,
	Diffuse,
	Texture,
	TextureFactor,
	CurrentAlpha,
	DiffuseAlpha,
	TextureAlpha,
	TextureFactorAlpha
};

enum class RenderBackendTextureArgumentModifiers : unsigned
{
	None = 0,
	Complement = 1u,
	AlphaReplicate = 2u
};

inline RenderBackendTextureArgumentModifiers operator | (
	RenderBackendTextureArgumentModifiers left,
	RenderBackendTextureArgumentModifiers right)
{
	return static_cast<RenderBackendTextureArgumentModifiers>(
		static_cast<unsigned>(left) | static_cast<unsigned>(right));
}

enum class RenderBackendTextureCoordinateSource
{
	PassThrough,
	CameraSpacePosition,
	CameraSpaceNormal,
	CameraSpaceReflectionVector
};

enum class RenderBackendTextureTransformFlags
{
	Disabled,
	Count2,
	Count3,
	ProjectedCount3
};

enum class RenderBackendTextureAddressMode
{
	Wrap,
	Clamp
};

enum class RenderBackendTextureFilterType
{
	Minification,
	Magnification,
	MipMap
};

enum class RenderBackendTextureFilter
{
	None,
	Point,
	Linear,
	Anisotropic
};

enum class RenderBackendPrimitiveType
{
	PointList,
	LineList,
	LineStrip,
	TriangleList,
	TriangleStrip
};

enum class RenderBackendBufferLockMode
{
	Normal,
	Discard,
	NoOverwrite
};

enum class RenderBackendMultisampleMode
{
	None,
	Samples2,
	Samples4,
	Samples8
};

enum class RenderBackendDeviceStatus
{
	Ready,
	Lost,
	NeedsReset,
	Error
};

struct RenderBackendViewport
{
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
	float min_z;
	float max_z;
};

struct RenderBackendAdapterInfo
{
	unsigned vendor_id = 0;
	unsigned device_id = 0;
	unsigned driver_version_high = 0;
	unsigned driver_version_low = 0;
};

struct RenderBackendTextureLimits
{
	unsigned max_width = 0;
	unsigned max_height = 0;
	unsigned max_volume_extent = 0;
	unsigned max_aspect_ratio = 0;
};

// Resource handles are deliberately opaque to WW3D2.  The active backend owns
// the object represented by a handle and is responsible for its lifetime.
using RenderBackendResourceHandle = uintptr_t;
using RenderBackendTextureHandle = RenderBackendResourceHandle;

enum class RenderBackendTexturePool
{
	Default,
	Managed,
	SystemMemory
};

enum class RenderBackendTextureKind
{
	Texture2D,
	DepthStencil,
	Cube,
	Volume
};

struct RenderBackendTextureDescription
{
	RenderBackendTextureKind kind = RenderBackendTextureKind::Texture2D;
	WW3DFormat format = WW3D_FORMAT_UNKNOWN;
	WW3DZFormat depth_format = WW3D_ZFORMAT_UNKNOWN;
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int depth = 1;
	unsigned int mip_levels = 0;
};

struct RenderBackendTextureLock
{
	void * bits = nullptr;
	unsigned int row_pitch = 0;
	unsigned int slice_pitch = 0;
};

enum class RenderBackendCubeFace
{
	PositiveX,
	NegativeX,
	PositiveY,
	NegativeY,
	PositiveZ,
	NegativeZ
};

struct RenderBackendMaterial
{
	float diffuse[4];
	float ambient[4];
	float specular[4];
	float emissive[4];
	float power;
};

enum class RenderBackendLightType
{
	Unknown,
	Point,
	Spot,
	Directional
};

struct RenderBackendLight
{
	RenderBackendLightType type = RenderBackendLightType::Unknown;
	float diffuse[4] = {};
	float specular[4] = {};
	float ambient[4] = {};
	float position[3] = {};
	float direction[3] = {};
	float range = 0.0f;
	float falloff = 0.0f;
	float attenuation0 = 0.0f;
	float attenuation1 = 0.0f;
	float attenuation2 = 0.0f;
	float theta = 0.0f;
	float phi = 0.0f;
};

class RenderBackendSurface
{
public:
	virtual ~RenderBackendSurface() {}
};

// Opaque per-mesh storage owned by the active renderer. WW3D2 keeps only this
// handle; the concrete backend decides how polygon batches are represented.
class RenderBackendMeshData
{
public:
	virtual ~RenderBackendMeshData() {}
};

class RenderBackendVertexBuffer
{
public:
	virtual ~RenderBackendVertexBuffer() {}
};

class RenderBackendIndexBuffer
{
public:
	virtual ~RenderBackendIndexBuffer() {}
};

class RenderBackendFont
{
public:
	virtual ~RenderBackendFont() {}
};

struct RenderBackendFontMetrics
{
	int height = 0;
	int ascent = 0;
	int overhang = 0;
};

struct RenderBackendFontGlyph
{
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int pitch = 0;
	const unsigned char *pixels = nullptr;
};

struct RenderBackendLockedSurface
{
	void * bits;
	unsigned int pitch;
};

struct RenderBackendSurfaceDescription
{
	WW3DFormat format = WW3D_FORMAT_UNKNOWN;
	unsigned int width = 0;
	unsigned int height = 0;
};

enum class RenderBackendSurfaceLockMode
{
	ReadWrite,
	ReadOnly
};

// Settings used by the game and tools while collecting extended render
// statistics. These belong to the active backend because they affect frame
// submission (for example the optional frame sleep), not just the caller
// displaying the statistics.
struct RenderBackendDebugSettings
{
	bool m_showingStats;
	bool m_disableTerrain;
	bool m_disableWater;
	bool m_disableObjects;
	bool m_disableOverhead;
	bool m_disableConsole;
	int m_debugLinesToShow;
	int m_sleepTime;

	RenderBackendDebugSettings() :
		m_showingStats(false),
		m_disableTerrain(false),
		m_disableWater(false),
		m_disableObjects(false),
		m_disableOverhead(false),
		m_disableConsole(false),
		m_debugLinesToShow(-1),
		m_sleepTime(0)
	{
	}
};

// Render resources outside WW3D (the WorldBuilder is the current user) can
// release and reacquire those resources around a device reset without seeing
// any backend-specific device type.
class RenderBackendCleanupHook
{
public:
	virtual ~RenderBackendCleanupHook() {}
	virtual void ReleaseResources() = 0;
	virtual void ReAcquireResources() = 0;
};

enum RenderBackendBrowserOption : unsigned
{
	RenderBackendBrowserOptionScrollbars = 0x0001,
	RenderBackendBrowserOption3DBorder = 0x0002
};

enum RenderBackendFontDrawFlag : unsigned
{
	RenderBackendFontDrawFlagLeft = 1u << 0,
	RenderBackendFontDrawFlagNoClip = 1u << 1,
	RenderBackendFontDrawFlagTop = 1u << 2,
	RenderBackendFontDrawFlagSingleLine = 1u << 3
};

struct RenderBackendRect
{
	int left;
	int top;
	int right;
	int bottom;
};

struct RenderBackendPoint
{
	int x;
	int y;
};

class IRenderBackend
{
public:
	virtual ~IRenderBackend() {}

	virtual bool Is_Initted() const = 0;
	virtual bool Is_Render_To_Texture() const = 0;
	virtual bool Has_Stencil() const = 0;
	virtual bool Supports_TnL() const = 0;
	virtual bool Supports_DXTC() const = 0;
	virtual bool Supports_NPatches() const = 0;
	virtual bool Supports_Bump_Envmap() const = 0;
	virtual bool Supports_Bump_Envmap_Luminance() const = 0;
	virtual bool Supports_Z_Bias() const = 0;
	virtual bool Supports_Anisotropic_Filtering() const = 0;
	virtual bool Supports_Modulate_Alpha_Add_Color() const = 0;
	virtual bool Supports_Dot3() const = 0;
	virtual bool Supports_Point_Sprites() const = 0;
	virtual bool Supports_Cubemaps() const = 0;
	virtual bool Supports_Color_Write_Mask() const = 0;
	virtual bool Supports_Texture_Operation(RenderBackendTextureOperation operation) const = 0;
	virtual bool Supports_Texture_Filter(RenderBackendTextureFilterType type,
		RenderBackendTextureFilter filter) const = 0;
	virtual bool Is_Fog_Allowed() const = 0;
	virtual bool Is_Fog_Enabled() const = 0;
	virtual unsigned Get_Fog_Color() const = 0;
	virtual bool Supports_Texture_Format(WW3DFormat format) const = 0;
	virtual bool Supports_Render_To_Texture_Format(WW3DFormat format) const = 0;
	virtual bool Supports_Depth_Stencil_Format(WW3DZFormat format) const = 0;
	virtual WW3DFormat Get_Back_Buffer_Format() const = 0;
	virtual SurfaceClass * Get_Back_Buffer_Surface() = 0;
	virtual RenderBackendDeviceStatus Get_Device_Status() const = 0;
	virtual bool Is_Device_Ready() const = 0;
	virtual bool Is_Render_Thread() const = 0;
	// Transitional handle for legacy paths that are being migrated incrementally.
	virtual bool Get_Adapter_Info(RenderBackendAdapterInfo & info) const = 0;
	virtual bool Get_Texture_Limits(RenderBackendTextureLimits & limits) const = 0;
	virtual int Get_Max_Textures_Per_Pass() const = 0;
	virtual int Get_Pixel_Shader_Major_Version() const = 0;
	virtual int Get_Pixel_Shader_Minor_Version() const = 0;
	virtual bool Is_3DFX_Voodoo3() const = 0;
	virtual unsigned Pack_Color(const Vector4 & color) const = 0;
	virtual unsigned Pack_Color(const Vector3 & color, float alpha) const = 0;
	virtual unsigned Pack_Color_Clamped(const Vector4 & color) const = 0;
	virtual Vector4 Unpack_Color(unsigned color) const = 0;
	virtual bool Is_Triangle_Draw_Enabled() const = 0;
	virtual void Set_Triangle_Draw_Enabled(bool enable) = 0;
	virtual RenderBackendDebugSettings & Get_Debug_Settings() = 0;
	virtual void Set_Cleanup_Hook(RenderBackendCleanupHook * hook) = 0;
	virtual void Invalidate_Renderer_Caches() = 0;
	virtual RenderBackendFont * Create_Font(int height, const char * face_name,
		bool bold = false, int width = 0) = 0;
	virtual void Release_Font(RenderBackendFont * font) = 0;
	virtual bool Get_Font_Metrics(RenderBackendFont * font,
		RenderBackendFontMetrics & metrics) const = 0;
	virtual bool Get_Font_Glyph(RenderBackendFont * font, unsigned int character,
		RenderBackendFontGlyph & glyph) = 0;
	virtual void Draw_Font(RenderBackendFont * font, const char * text,
		unsigned text_length, const RenderBackendRect & rect,
		unsigned flags, unsigned color) = 0;

	virtual bool Initialize_Browser(const char * bad_page_url = nullptr,
		const char * loading_page_url = nullptr,
		const char * mouse_filename = nullptr,
		const char * mouse_busy_filename = nullptr) = 0;
	virtual void Shutdown_Browser() = 0;
	virtual void Update_Browser() = 0;
	virtual void Render_Browser(int backbuffer_index) = 0;
	virtual void Create_Browser(const char * browser_name, const char * url,
		int x, int y, int width, int height, int update_ticks = 0,
		unsigned options = RenderBackendBrowserOptionScrollbars |
			RenderBackendBrowserOption3DBorder,
		void * game_dispatch = nullptr) = 0;
	virtual void Destroy_Browser(const char * browser_name) = 0;
	virtual bool Is_Browser_Open(const char * browser_name) const = 0;
	virtual void Navigate_Browser(const char * browser_name, const char * url) = 0;

	virtual bool Set_Render_Device(const char * device_name,
		int width, int height, int bits, int windowed, bool resize_window) = 0;
	virtual bool Set_Render_Device(int device, int width, int height, int bits,
		int windowed, bool resize_window, bool reset_device, bool restore_assets) = 0;
	virtual void Set_Fullscreen_Mode(RenderBackendFullscreenMode mode) = 0;
	virtual bool Set_Any_Render_Device() = 0;
	virtual bool Set_Next_Render_Device() = 0;
	virtual bool Is_Windowed() const = 0;
	virtual bool Toggle_Windowed() = 0;
	virtual int Get_Render_Device() const = 0;
	virtual const RenderDeviceDescClass & Get_Render_Device_Desc(int device) const = 0;
	virtual int Get_Render_Device_Count() const = 0;
	virtual const char * Get_Render_Device_Name(int device) const = 0;
	virtual bool Set_Device_Resolution(int width, int height, int bits,
		int windowed, bool resize_window) = 0;
	virtual void Get_Device_Resolution(int & width, int & height, int & bits,
		bool & windowed) const = 0;
	virtual void Get_Render_Target_Resolution(int & width, int & height, int & bits,
		bool & windowed) const = 0;
	virtual int Get_Device_Resolution_Width() const = 0;
	virtual int Get_Device_Resolution_Height() const = 0;
	virtual void Set_Swap_Interval(int swap) = 0;
	virtual int Get_Swap_Interval() const = 0;
	virtual bool Reset_Device(bool reload_assets = true) = 0;
	virtual bool Registry_Save_Render_Device(const char * sub_key) = 0;
	virtual bool Registry_Save_Render_Device(const char * sub_key, int device,
		int width, int height, int depth, bool windowed, int texture_depth) = 0;
	virtual bool Registry_Load_Render_Device(const char * sub_key, bool resize_window) = 0;
	virtual bool Registry_Load_Render_Device(const char * sub_key, char * device,
		int device_len, int & width, int & height, int & depth, int & windowed,
		int & texture_depth) = 0;
	virtual void Set_Texture_Bitdepth(int depth) = 0;
	virtual int Get_Texture_Bitdepth() const = 0;
	virtual void Set_Multisample_Mode(RenderBackendMultisampleMode mode) = 0;
	virtual RenderBackendMultisampleMode Get_Multisample_Mode() const = 0;

	virtual void Set_Gamma(float gamma, float bright, float contrast,
		bool calibrate = true, bool uselimit = true) = 0;

	virtual void Begin_Scene() = 0;
	virtual void End_Scene(bool flip_frame = true) = 0;
	virtual void Flip_To_Primary() = 0;
	virtual void Clear(bool clear_color, bool clear_z_stencil,
		const Vector3 & color,
		float dest_alpha = 0.0f, float z = 1.0f,
		unsigned int stencil = 0) = 0;
	virtual void Set_Viewport(const RenderBackendViewport & viewport) = 0;
	virtual bool Get_Viewport(RenderBackendViewport & viewport) const = 0;
	virtual void Show_Cursor(bool show) = 0;
	virtual bool Set_Cursor_Properties(int hotspot_x, int hotspot_y, SurfaceClass * surface) = 0;
	virtual void Set_Cursor_Position(int x, int y) = 0;
	virtual void Invalidate_Cached_Render_States() = 0;
	virtual void Set_Render_State(unsigned state, unsigned value) = 0;
	virtual unsigned Get_Render_State(unsigned state) const = 0;
	virtual void Set_Texture_Stage_State(unsigned stage, unsigned state, unsigned value) = 0;

	virtual void Set_Ambient(const Vector3 & color) = 0;
	virtual void Set_Light_Environment(LightEnvironmentClass * light_env) = 0;
	virtual void Set_Fog(bool enable, const Vector3 & color, float start, float end) = 0;
	// Applies the material value immediately. This is distinct from Set_Material,
	// which updates the deferred W3D render state.
	virtual void Set_Material_Values(const RenderBackendMaterial & material) = 0;
	virtual void Set_Fill_Mode(RenderBackendFillMode mode) = 0;
	virtual RenderBackendFillMode Get_Fill_Mode() const = 0;
	virtual void Set_Color_Write_Mask(RenderBackendColorWriteMask mask) = 0;
	virtual RenderBackendColorWriteMask Get_Color_Write_Mask() const = 0;
	virtual void Set_Alpha_Blend_Enabled(bool enable) = 0;
	virtual void Set_Blend_Operation(RenderBackendBlendOperation operation) = 0;
	virtual void Set_Blend_Factors(RenderBackendBlendFactor source,
		RenderBackendBlendFactor destination) = 0;
	virtual void Set_Source_Blend_Factor(RenderBackendBlendFactor factor) = 0;
	virtual void Set_Destination_Blend_Factor(RenderBackendBlendFactor factor) = 0;
	virtual void Set_Alpha_Test_Enabled(bool enable) = 0;
	virtual void Set_Alpha_Test_Function(RenderBackendCompareFunction function) = 0;
	virtual void Set_Alpha_Test_Reference(unsigned reference) = 0;
	virtual void Set_Fog_Enabled(bool enable) = 0;
	virtual void Set_Fog_Color(unsigned color) = 0;
	virtual void Set_Depth_Bias(unsigned bias) = 0;
	virtual void Set_Texture_Factor(unsigned color) = 0;
	virtual void Set_Depth_Test_Enabled(bool enable) = 0;
	virtual void Set_Depth_Write_Enabled(bool enable) = 0;
	virtual void Set_Depth_Function(RenderBackendCompareFunction function) = 0;
	virtual void Set_Cull_Mode(RenderBackendCullMode mode) = 0;
	virtual RenderBackendCullMode Get_Cull_Mode() const = 0;
	virtual void Set_Point_Sprite_Enabled(bool enable) = 0;
	virtual void Set_Point_Scale_Enabled(bool enable) = 0;
	virtual void Set_Point_Size(float size) = 0;
	virtual void Set_Point_Size_Min(float size) = 0;
	virtual void Set_Point_Size_Max(float size) = 0;
	virtual void Set_Point_Scale(float scale_a, float scale_b, float scale_c) = 0;
	virtual void Set_Shade_Mode(RenderBackendShadeMode mode) = 0;
	virtual void Set_Lighting_Enabled(bool enable) = 0;
	virtual void Set_Normalize_Normals(bool enable) = 0;
	virtual void Set_Specular_Enabled(bool enable) = 0;
	virtual void Set_Material_Color_Sources(RenderBackendMaterialSource ambient,
		RenderBackendMaterialSource diffuse,
		RenderBackendMaterialSource emissive) = 0;
	virtual void Set_NPatch_Segments(float segments) = 0;
	virtual void Set_Stencil_Enabled(bool enable) = 0;
	virtual void Set_Stencil_Function(RenderBackendCompareFunction function) = 0;
	virtual void Set_Stencil_Reference(unsigned reference) = 0;
	virtual void Set_Stencil_Read_Mask(unsigned mask) = 0;
	virtual void Set_Stencil_Write_Mask(unsigned mask) = 0;
	virtual void Set_Stencil_Z_Fail_Operation(RenderBackendStencilOperation operation) = 0;
	virtual void Set_Stencil_Fail_Operation(RenderBackendStencilOperation operation) = 0;
	virtual void Set_Stencil_Pass_Operation(RenderBackendStencilOperation operation) = 0;
	virtual void Set_Texture_Operation(unsigned stage,
		RenderBackendTextureComponent component,
		RenderBackendTextureOperation operation) = 0;
	virtual void Set_Texture_Argument(unsigned stage,
		RenderBackendTextureComponent component,
		unsigned argument_index,
		RenderBackendTextureArgument argument,
		RenderBackendTextureArgumentModifiers modifiers =
			RenderBackendTextureArgumentModifiers::None) = 0;
	virtual void Set_Texture_Coordinate_Source(unsigned stage,
		RenderBackendTextureCoordinateSource source,
		unsigned uv_array_index = 0) = 0;
	virtual void Set_Texture_Transform_Flags(unsigned stage,
		RenderBackendTextureTransformFlags flags) = 0;
	virtual void Set_Texture_Address_Mode(unsigned stage,
		bool u_coordinate,
		RenderBackendTextureAddressMode mode) = 0;
	virtual void Set_Texture_Filter(unsigned stage,
		RenderBackendTextureFilterType type,
		RenderBackendTextureFilter filter) = 0;
	virtual void Set_Texture_Max_Anisotropy(unsigned stage, unsigned level) = 0;
	virtual void Set_Texture_Bump_Environment_Matrix(unsigned stage,
		float m00, float m01, float m10, float m11,
		float scale = 1.0f, float offset = 0.0f) = 0;
	virtual TextureClass * Create_Render_Target(int width, int height,
		WW3DFormat format = WW3D_FORMAT_UNKNOWN) = 0;
	virtual void Create_Render_Target(int width, int height, WW3DFormat format,
		WW3DZFormat depth_format, TextureClass ** target, ZTextureClass ** depth_target) = 0;
	virtual void Set_Render_Target(TextureClass * render_target,
		ZTextureClass * depth_target = nullptr) = 0;
	virtual RenderBackendSurface * Create_System_Memory_Surface(unsigned width,
		unsigned height, WW3DFormat format) = 0;
	virtual SurfaceClass * Create_Surface(unsigned width, unsigned height,
		WW3DFormat format) = 0;
	virtual RenderBackendSurface * Create_Surface_From_File(const char * filename) = 0;
	virtual bool Get_Surface_Description(RenderBackendSurface * surface,
		RenderBackendSurfaceDescription & description) const = 0;
	virtual bool Lock_Surface(RenderBackendSurface * surface,
		RenderBackendLockedSurface & locked_surface,
		const RenderBackendRect * rect = nullptr,
		RenderBackendSurfaceLockMode mode = RenderBackendSurfaceLockMode::ReadWrite) = 0;
	virtual void Unlock_Surface(RenderBackendSurface * surface) = 0;
	virtual void Release_Surface(RenderBackendSurface * surface) = 0;
	virtual void Copy_Surface_Rect(RenderBackendSurface * source,
		const RenderBackendRect & source_rect,
		SurfaceClass * destination,
		const RenderBackendPoint & destination_point) = 0;
	virtual bool Copy_Surface_Rect(SurfaceClass * source,
		const RenderBackendRect & source_rect,
		RenderBackendSurface * destination,
		const RenderBackendPoint & destination_point) = 0;
	virtual bool Copy_Surface(SurfaceClass * source, SurfaceClass * destination) = 0;
	virtual bool Copy_Surface_Rect(SurfaceClass * source, const RenderBackendRect & source_rect,
		SurfaceClass * destination, const RenderBackendPoint & destination_point) = 0;
	virtual bool Copy_Surface_Stretch(SurfaceClass * source,
		const RenderBackendRect & source_rect, SurfaceClass * destination,
		const RenderBackendRect & destination_rect) = 0;
	virtual int Read_Back_Buffer_Rect(void * buffer, int buffer_size,
		int x, int y, int width, int height) = 0;
	virtual RenderBackendTextureHandle Create_Transient_Render_Texture(unsigned width, unsigned height,
		WW3DFormat format) = 0;
	virtual bool Copy_Back_Buffer_To_Texture(RenderBackendTextureHandle texture) = 0;
	virtual bool Copy_Texture_To_Surface(RenderBackendTextureHandle texture, SurfaceClass * destination) = 0;
	virtual bool Copy_Render_Target_To_Surface(TextureClass * source, SurfaceClass * destination) = 0;
	virtual void Release_Transient_Render_Texture(RenderBackendTextureHandle texture) = 0;
	virtual RenderBackendTextureHandle Create_Texture_Handle(unsigned width, unsigned height, WW3DFormat format,
		unsigned mip_levels, bool dynamic, bool render_target = false) = 0;
	virtual RenderBackendTextureHandle Create_ZTexture_Handle(unsigned width, unsigned height, WW3DZFormat format,
		unsigned mip_levels) = 0;
	virtual RenderBackendTextureHandle Create_Surface_Handle(unsigned width, unsigned height, WW3DFormat format) = 0;
	virtual RenderBackendTextureHandle Create_Surface_Handle(const char *filename) = 0;
	virtual RenderBackendTextureHandle Create_Texture_Handle_Pooled(unsigned width, unsigned height, WW3DFormat format,
		unsigned mip_levels, RenderBackendTexturePool pool, bool render_target) = 0;
	virtual RenderBackendTextureHandle Create_ZTexture_Handle_Pooled(unsigned width, unsigned height, WW3DZFormat format,
		unsigned mip_levels, RenderBackendTexturePool pool) = 0;
	virtual RenderBackendTextureHandle Create_Cube_Texture_Handle(unsigned width, unsigned height, WW3DFormat format,
		unsigned mip_levels, RenderBackendTexturePool pool, bool render_target) = 0;
	virtual RenderBackendTextureHandle Create_Volume_Texture_Handle(unsigned width, unsigned height, unsigned depth,
		WW3DFormat format, unsigned mip_levels, RenderBackendTexturePool pool) = 0;
	virtual RenderBackendTextureHandle Create_Texture_From_Surface(RenderBackendSurface * surface,
		unsigned mip_levels) = 0;
	virtual SurfaceClass * Get_Texture_Surface_Level(RenderBackendTextureHandle texture, unsigned level) = 0;
	virtual RenderBackendTextureHandle Create_Texture_From_File_Handle(const char *filename, unsigned mip_levels) = 0;
	virtual RenderBackendTextureHandle Add_Texture_Reference(RenderBackendTextureHandle texture) = 0;
	virtual void Release_Texture_Handle(RenderBackendTextureHandle texture) = 0;
	virtual unsigned Get_Texture_Level_Count(RenderBackendTextureHandle texture) const = 0;
	virtual bool Get_Texture_Description(RenderBackendTextureHandle texture, unsigned level,
		RenderBackendTextureDescription & description) const = 0;
	virtual bool Lock_Texture(RenderBackendTextureHandle texture, unsigned level,
		RenderBackendTextureLock & locked_texture, bool read_only = false) = 0;
	virtual void Unlock_Texture(RenderBackendTextureHandle texture, unsigned level) = 0;
	virtual bool Lock_Cube_Texture(RenderBackendTextureHandle texture, RenderBackendCubeFace face,
		unsigned level, RenderBackendTextureLock & locked_texture, bool read_only = false) = 0;
	virtual void Unlock_Cube_Texture(RenderBackendTextureHandle texture, RenderBackendCubeFace face,
		unsigned level) = 0;
	virtual bool Lock_Volume_Texture(RenderBackendTextureHandle texture, unsigned level,
		RenderBackendTextureLock & locked_texture, bool read_only = false) = 0;
	virtual void Unlock_Volume_Texture(RenderBackendTextureHandle texture, unsigned level) = 0;
	virtual bool Update_Texture(RenderBackendTextureHandle source,
		RenderBackendTextureHandle destination) = 0;
	virtual bool Generate_Texture_Mipmaps(RenderBackendTextureHandle texture) = 0;
	virtual void Set_Texture_LOD(RenderBackendTextureHandle texture, unsigned lod) = 0;
	virtual unsigned Get_Texture_Priority(RenderBackendTextureHandle texture) const = 0;
	virtual unsigned Set_Texture_Priority(RenderBackendTextureHandle texture, unsigned priority) = 0;
	virtual bool Is_Missing_Texture_Handle(RenderBackendTextureHandle texture) const = 0;
	virtual RenderBackendTextureHandle Create_Missing_Texture() = 0;
	virtual RenderBackendSurface * Create_Missing_Surface() = 0;
	virtual void Register_Texture(TextureBaseClass * texture, RenderBackendTextureKind kind,
		unsigned width, unsigned height, unsigned depth, WW3DFormat format,
		WW3DZFormat depth_format, unsigned mip_levels, bool render_target) = 0;
	virtual void Unregister_Texture(TextureBaseClass * texture) = 0;

	// These handles keep backend-owned resources out of GenMD. They are
	// intended for legacy fixed-function paths that have not yet moved to the
	// W3D VertexBufferClass/IndexBufferClass wrappers.
	virtual RenderBackendVertexBuffer * Create_Vertex_Buffer(unsigned size_bytes,
		const RenderBackendVertexLayout &layout, unsigned usage = BUFFER_USAGE_DEFAULT) = 0;
	RenderBackendVertexBuffer * Create_Vertex_Buffer(unsigned size_bytes,
		RenderBackendVertexFormat format, bool dynamic)
	{
		return Create_Vertex_Buffer(size_bytes, RenderBackend_Vertex_Layout(format),
			dynamic ? BUFFER_USAGE_DYNAMIC : BUFFER_USAGE_DEFAULT);
	}
	virtual RenderBackendIndexBuffer * Create_Index_Buffer(unsigned size_bytes,
		unsigned usage = BUFFER_USAGE_DEFAULT) = 0;
	RenderBackendIndexBuffer * Create_Index_Buffer(unsigned size_bytes, bool dynamic)
	{
		return Create_Index_Buffer(size_bytes,
			dynamic ? BUFFER_USAGE_DYNAMIC : BUFFER_USAGE_DEFAULT);
	}
	virtual bool Lock_Vertex_Buffer(RenderBackendVertexBuffer * buffer,
		unsigned offset_bytes, unsigned size_bytes, void ** data,
		RenderBackendBufferLockMode mode) = 0;
	virtual bool Lock_Index_Buffer(RenderBackendIndexBuffer * buffer,
		unsigned offset_bytes, unsigned size_bytes, void ** data,
		RenderBackendBufferLockMode mode) = 0;
	virtual void Unlock_Vertex_Buffer(RenderBackendVertexBuffer * buffer) = 0;
	virtual void Unlock_Index_Buffer(RenderBackendIndexBuffer * buffer) = 0;
	virtual void Release_Vertex_Buffer(RenderBackendVertexBuffer * buffer) = 0;
	virtual void Release_Index_Buffer(RenderBackendIndexBuffer * buffer) = 0;
	virtual void Set_Vertex_Buffer(RenderBackendVertexBuffer * buffer,
		unsigned offset_bytes, unsigned stride_bytes, unsigned stream = 0) = 0;
	virtual void Set_Index_Buffer(RenderBackendIndexBuffer * buffer) = 0;
	virtual void Set_Vertex_Format(RenderBackendVertexFormat format) = 0;
	virtual bool Process_Vertices(VertexBufferClass * destination, unsigned vertex_count) = 0;
	virtual void Draw_Indexed_Primitives(RenderBackendPrimitiveType primitive_type,
		unsigned base_vertex_index, unsigned min_vertex_index,
		unsigned vertex_count, unsigned start_index, unsigned primitive_count) = 0;
	virtual void Draw_Primitive_Up(RenderBackendPrimitiveType primitive_type,
		unsigned primitive_count, const void * vertices, unsigned stride_bytes,
		RenderBackendVertexFormat format) = 0;
	virtual void Draw_Primitive(RenderBackendPrimitiveType primitive_type,
		unsigned start_vertex, unsigned primitive_count) = 0;

	// These methods use W3D types and deliberately expose no graphics API types.
	virtual void Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned stream = 0) = 0;
	virtual void Set_Vertex_Buffer(const DynamicVBAccessClass & vba) = 0;
	virtual void Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset) = 0;
	virtual void Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset) = 0;
	virtual void Set_Index_Buffer_Index_Offset(unsigned offset) = 0;

	virtual void Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix,
		float znear, float zfar) = 0;
	virtual void Set_Transform(RenderBackendTransform transform, const Matrix4x4 & matrix) = 0;
	virtual void Set_Transform(RenderBackendTransform transform, const Matrix3D & matrix) = 0;
	virtual void Get_Transform(RenderBackendTransform transform, Matrix4x4 & matrix) = 0;
	virtual void Set_Transform(RenderBackendTransform transform, const float * matrix_elements) = 0;
	virtual void Get_Transform(RenderBackendTransform transform, float * matrix_elements) = 0;
	virtual void Set_World_Identity() = 0;
	virtual void Set_View_Identity() = 0;
	virtual bool Is_World_Identity() = 0;
	virtual bool Is_View_Identity() = 0;

	virtual void Set_Shader(const ShaderClass & shader) = 0;
	virtual void Get_Shader(ShaderClass & shader) = 0;
	// Shader handles are opaque backend-owned values. They are represented as
	// integers here so the interface does not expose a graphics API object.
	virtual void Set_Vertex_Shader(uintptr_t shader,
		const RenderBackendVertexShaderInputLayout * input_layout = nullptr) = 0;
	virtual void Set_Pixel_Shader(uintptr_t shader) = 0;
	virtual bool Create_Pixel_Shader(const void * bytecode, uintptr_t * shader) = 0;
	virtual bool Create_Pixel_Shader_From_Source(const char * source, uintptr_t * shader) = 0;
	virtual bool Create_Vertex_Shader(const void * bytecode, uintptr_t * shader,
		const RenderBackendVertexShaderInputLayout * input_layout = nullptr) = 0;
	virtual void Release_Vertex_Shader(uintptr_t shader) = 0;
	virtual void Release_Pixel_Shader(uintptr_t shader) = 0;
	virtual void Set_Vertex_Shader_Constant(unsigned reg, const void * data,
		unsigned count) = 0;
	virtual void Set_Pixel_Shader_Constant(unsigned reg, const void * data,
		unsigned count) = 0;
	virtual void Set_Texture(unsigned stage, TextureBaseClass * texture) = 0;
	// Apply the underlying texture resource to a stage immediately. This is
	// separate from Set_Texture(), which updates the deferred W3D render state.
	virtual void Set_Texture_Resource(unsigned stage, const TextureBaseClass * texture) = 0;
	virtual void Set_Texture_Handle(unsigned stage, uintptr_t texture) = 0;
	virtual void Set_Material(const VertexMaterialClass * material) = 0;
	virtual void Set_Light(unsigned index, const LightClass & light) = 0;
	virtual void Set_Light_From_State(unsigned index, const RenderBackendLight * light) = 0;
	virtual void Disable_Light(unsigned index) = 0;
	virtual void Capture_Render_State(RenderStateStruct & state) = 0;
	virtual void Apply_Render_State(const RenderStateStruct & state) = 0;
	virtual void Begin_Backend_Statistics() = 0;
	virtual void End_Backend_Statistics() = 0;
	virtual void Restore_Render_State() = 0;
	virtual void Apply_Render_State_Changes() = 0;

	// Mesh and sorted-render submission are backend-owned services. Callers
	// should not include or reference a concrete mesh renderer.
	virtual void Initialize_Mesh_Renderer() = 0;
	virtual void Shutdown_Mesh_Renderer() = 0;
	virtual void Invalidate_Mesh_Renderer(bool shutdown = false) = 0;
	virtual void Clear_Mesh_Renderer_Delete_Lists() = 0;
	virtual void Set_Mesh_Renderer_Camera(CameraClass * camera) = 0;
	virtual void Flush_Mesh_Renderer() = 0;
	virtual void Register_Mesh_Type(MeshModelClass * mesh) = 0;
	virtual void Unregister_Mesh_Type(MeshModelClass * mesh) = 0;
	virtual void Add_Decal_Mesh(DecalMeshClass * mesh) = 0;
	virtual void Set_Mesh_Renderer_Lighting(bool enable) = 0;
	virtual void Set_Force_Multiply(bool enable) = 0;
	virtual void Add_Renderer_Debug_Mesh(MeshClass * mesh) = 0;
	virtual bool Has_Mesh_Renderers(const MeshModelClass * mesh) const = 0;
	virtual unsigned Get_Mesh_Renderer_Vertex_Offset(const MeshModelClass * mesh) const = 0;
	virtual unsigned Get_Mesh_Renderer_Count(const MeshModelClass * mesh) const = 0;
	virtual void Update_Mesh_Texture(MeshModelClass * mesh, TextureClass * texture,
		TextureClass * new_texture, unsigned pass, unsigned stage) = 0;
	virtual void Update_Mesh_Material(MeshModelClass * mesh, VertexMaterialClass * material,
		VertexMaterialClass * new_material, unsigned pass) = 0;
	virtual void Add_Mesh_Render_Tasks(MeshModelClass * mesh, MeshClass * instance) = 0;
	virtual void Add_Mesh_Material_Pass(MeshModelClass * mesh, MaterialPassClass * pass,
		MeshClass * instance, bool delayed) = 0;
	virtual void Add_Mesh_Skin(MeshModelClass * mesh, MeshClass * instance) = 0;
	virtual void Render_Mesh_Pass(MeshModelClass * mesh, int base_vertex_offset) = 0;

	virtual void Initialize_Sorting_Renderer() = 0;
	virtual void Shutdown_Sorting_Renderer() = 0;
	virtual void Set_Sorting_Min_Vertex_Buffer_Size(unsigned value) = 0;
	virtual void Insert_Sorted_Triangles(const SphereClass & bounding_sphere,
		unsigned short start_index, unsigned short polygon_count,
		unsigned short min_vertex_index, unsigned short vertex_count) = 0;
	virtual void Insert_Sorted_Triangles(unsigned short start_index,
		unsigned short polygon_count, unsigned short min_vertex_index,
		unsigned short vertex_count) = 0;
	virtual void Flush_Sorting_Renderer() = 0;

	virtual void Draw_Triangles(unsigned buffer_type,
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count) = 0;
	virtual void Draw_Triangles(unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count) = 0;
	virtual void Draw_Strip(unsigned short start_index,
		unsigned short index_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count) = 0;
};

class RenderBackendVertexBufferLock
{
public:
	RenderBackendVertexBufferLock(IRenderBackend * backend,
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

	~RenderBackendVertexBufferLock()
	{
		if (Locked)
		{
			Backend->Unlock_Vertex_Buffer(Buffer);
		}
	}

	void * Get_Data() const { return Data; }
	bool Is_Locked() const { return Locked; }

private:
	IRenderBackend * Backend;
	RenderBackendVertexBuffer * Buffer;
	void * Data;
	bool Locked;
};

class RenderBackendIndexBufferLock
{
public:
	RenderBackendIndexBufferLock(IRenderBackend * backend,
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

	~RenderBackendIndexBufferLock()
	{
		if (Locked)
		{
			Backend->Unlock_Index_Buffer(Buffer);
		}
	}

	void * Get_Data() const { return Data; }
	bool Is_Locked() const { return Locked; }

private:
	IRenderBackend * Backend;
	RenderBackendIndexBuffer * Buffer;
	void * Data;
	bool Locked;
};

inline void RenderBackend_Release_Vertex_Buffer(IRenderBackend * backend,
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

inline void RenderBackend_Release_Index_Buffer(IRenderBackend * backend,
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
