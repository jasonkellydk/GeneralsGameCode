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
// of Direct3D types so another backend can implement it without inheriting the
// legacy DX8Wrapper compatibility facade.

#pragma once

#include <cstdint>

#include "WW3D2/ww3dformat.h"

class LightEnvironmentClass;
class LightClass;
class Matrix3D;
class Matrix4x4;
class DynamicIBAccessClass;
class DynamicVBAccessClass;
class IndexBufferClass;
class ShaderClass;
class TextureClass;
class TextureBaseClass;
class ZTextureClass;
class VertexBufferClass;
class VertexMaterialClass;
class Vector3;
class RenderDeviceDescClass;
class SurfaceClass;
class Vector4;

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

// The fixed-function vertex layouts still used by a few GenMD render paths.
// These names describe the data layout rather than the graphics API encoding.
enum class RenderBackendVertexFormat
{
	Position,
	PositionDiffuse,
	PositionTexture,
	PositionDiffuseTexture,
	PositionTexture2,
	PositionDiffuseTexture2,
	PositionNormal,
	PositionNormalDiffuse,
	PositionNormalTexture,
	PositionNormalDiffuseTexture,
	PositionNormalTexture2,
	PositionNormalDiffuseTexture2,
	TransformedPosition,
	TransformedPositionDiffuse,
	TransformedPositionTexture,
	TransformedPositionDiffuseTexture,
	TransformedPositionTexture2,
	TransformedPositionDiffuseTexture2
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

struct RenderBackendMaterial
{
	float diffuse[4];
	float ambient[4];
	float specular[4];
	float emissive[4];
	float power;
};

class RenderBackendSurface
{
public:
	virtual ~RenderBackendSurface() {}
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

struct RenderBackendLockedSurface
{
	void * bits;
	unsigned int pitch;
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
	virtual RenderBackendDeviceStatus Get_Device_Status() const = 0;
	virtual bool Is_Device_Ready() const = 0;
	virtual int Get_Max_Textures_Per_Pass() const = 0;
	virtual bool Is_3DFX_Voodoo3() const = 0;
	virtual unsigned Pack_Color(const Vector4 & color) const = 0;
	virtual unsigned Pack_Color(const Vector3 & color, float alpha) const = 0;
	virtual unsigned Pack_Color_Clamped(const Vector4 & color) const = 0;
	virtual Vector4 Unpack_Color(unsigned color) const = 0;
	virtual bool Is_Triangle_Draw_Enabled() const = 0;
	virtual void Set_Triangle_Draw_Enabled(bool enable) = 0;

	virtual bool Set_Render_Device(const char * device_name,
		int width, int height, int bits, int windowed, bool resize_window) = 0;
	virtual bool Set_Render_Device(int device, int width, int height, int bits,
		int windowed, bool resize_window, bool reset_device, bool restore_assets) = 0;
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
	virtual void Invalidate_Cached_Render_States() = 0;

	virtual void Set_Ambient(const Vector3 & color) = 0;
	virtual void Set_Light_Environment(LightEnvironmentClass * light_env) = 0;
	virtual void Set_Fog(bool enable, const Vector3 & color, float start, float end) = 0;
	// Applies the material value immediately. This is distinct from Set_Material,
	// which updates the deferred W3D render state.
	virtual void Set_Material_Values(const RenderBackendMaterial & material) = 0;
	virtual void Set_Fill_Mode(RenderBackendFillMode mode) = 0;
	virtual void Set_Color_Write_Mask(RenderBackendColorWriteMask mask) = 0;
	virtual RenderBackendColorWriteMask Get_Color_Write_Mask() const = 0;
	virtual void Set_Alpha_Blend_Enabled(bool enable) = 0;
	virtual void Set_Blend_Operation(RenderBackendBlendOperation operation) = 0;
	virtual void Set_Blend_Factors(RenderBackendBlendFactor source,
		RenderBackendBlendFactor destination) = 0;
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
		RenderBackendTextureArgument argument) = 0;
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
		float m00, float m01, float m10, float m11) = 0;
	virtual TextureClass * Create_Render_Target(int width, int height,
		WW3DFormat format = WW3D_FORMAT_UNKNOWN) = 0;
	virtual void Create_Render_Target(int width, int height, WW3DFormat format,
		WW3DZFormat depth_format, TextureClass ** target, ZTextureClass ** depth_target) = 0;
	virtual void Set_Render_Target(TextureClass * render_target,
		ZTextureClass * depth_target = nullptr) = 0;
	virtual RenderBackendSurface * Create_System_Memory_Surface(unsigned width,
		unsigned height, WW3DFormat format) = 0;
	virtual bool Lock_Surface(RenderBackendSurface * surface,
		RenderBackendLockedSurface & locked_surface) = 0;
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

	// These handles keep backend-owned D3D resources out of GenMD. They are
	// intended for legacy fixed-function paths that have not yet moved to the
	// W3D VertexBufferClass/IndexBufferClass wrappers.
	virtual RenderBackendVertexBuffer * Create_Vertex_Buffer(unsigned size_bytes,
		RenderBackendVertexFormat format, bool dynamic) = 0;
	virtual RenderBackendIndexBuffer * Create_Index_Buffer(unsigned size_bytes,
		bool dynamic) = 0;
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
	virtual void Draw_Indexed_Primitives(RenderBackendPrimitiveType primitive_type,
		unsigned base_vertex_index, unsigned min_vertex_index,
		unsigned vertex_count, unsigned start_index, unsigned primitive_count) = 0;
	virtual void Draw_Primitive_Up(RenderBackendPrimitiveType primitive_type,
		unsigned primitive_count, const void * vertices, unsigned stride_bytes,
		RenderBackendVertexFormat format) = 0;

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
	virtual void Set_World_Identity() = 0;
	virtual void Set_View_Identity() = 0;
	virtual bool Is_World_Identity() = 0;
	virtual bool Is_View_Identity() = 0;

	virtual void Set_Shader(const ShaderClass & shader) = 0;
	virtual void Get_Shader(ShaderClass & shader) = 0;
	// Shader handles are opaque backend-owned values. They are represented as
	// integers here so the interface does not expose a graphics API object.
	virtual void Set_Vertex_Shader(uintptr_t shader) = 0;
	virtual void Set_Pixel_Shader(uintptr_t shader) = 0;
	virtual void Set_Vertex_Shader_Constant(unsigned reg, const void * data,
		unsigned count) = 0;
	virtual void Set_Pixel_Shader_Constant(unsigned reg, const void * data,
		unsigned count) = 0;
	virtual void Set_Texture(unsigned stage, TextureBaseClass * texture) = 0;
	// Apply the underlying texture resource to a stage immediately. This is
	// separate from Set_Texture(), which updates the deferred W3D render state.
	virtual void Set_Texture_Resource(unsigned stage, const TextureBaseClass * texture) = 0;
	virtual void Set_Material(const VertexMaterialClass * material) = 0;
	virtual void Set_Light(unsigned index, const LightClass & light) = 0;
	virtual void Disable_Light(unsigned index) = 0;
	virtual void Apply_Render_State_Changes() = 0;

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
