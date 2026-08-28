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

#pragma once

#include "WW3D2/IRenderBackend.h"

class DX9Backend : public IRenderBackend
{
public:
	static DX9Backend *Create(void * window, bool lite);

	virtual ~DX9Backend() override;

	virtual bool Is_Initted() const override;
	virtual bool Is_Render_To_Texture() const override;
	virtual bool Has_Stencil() const override;
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
	virtual int Get_Max_Textures_Per_Pass() const override;
	virtual bool Is_3DFX_Voodoo3() const override;
	virtual unsigned Pack_Color(const Vector4 & color) const override;
	virtual unsigned Pack_Color(const Vector3 & color, float alpha) const override;
	virtual unsigned Pack_Color_Clamped(const Vector4 & color) const override;
	virtual Vector4 Unpack_Color(unsigned color) const override;
	virtual bool Is_Triangle_Draw_Enabled() const override;
	virtual void Set_Triangle_Draw_Enabled(bool enable) override;

	virtual bool Set_Render_Device(const char * device_name,
		int width, int height, int bits, int windowed, bool resize_window) override;
	virtual bool Set_Render_Device(int device, int width, int height, int bits,
		int windowed, bool resize_window, bool reset_device, bool restore_assets) override;
	virtual bool Set_Any_Render_Device() override;
	virtual bool Set_Next_Render_Device() override;
	virtual bool Is_Windowed() const override;
	virtual bool Toggle_Windowed() override;
	virtual int Get_Render_Device() const override;
	virtual const RenderDeviceDescClass & Get_Render_Device_Desc(int device) const override;
	virtual int Get_Render_Device_Count() const override;
	virtual const char * Get_Render_Device_Name(int device) const override;
	virtual bool Set_Device_Resolution(int width, int height, int bits,
		int windowed, bool resize_window) override;
	virtual void Get_Device_Resolution(int & width, int & height, int & bits,
		bool & windowed) const override;
	virtual void Get_Render_Target_Resolution(int & width, int & height, int & bits,
		bool & windowed) const override;
	virtual int Get_Device_Resolution_Width() const override;
	virtual int Get_Device_Resolution_Height() const override;
	virtual void Set_Swap_Interval(int swap) override;
	virtual int Get_Swap_Interval() const override;
	virtual bool Reset_Device(bool reload_assets) override;
	virtual bool Registry_Save_Render_Device(const char * sub_key) override;
	virtual bool Registry_Save_Render_Device(const char * sub_key, int device,
		int width, int height, int depth, bool windowed, int texture_depth) override;
	virtual bool Registry_Load_Render_Device(const char * sub_key, bool resize_window) override;
	virtual bool Registry_Load_Render_Device(const char * sub_key, char * device,
		int device_len, int & width, int & height, int & depth, int & windowed,
		int & texture_depth) override;
	virtual void Set_Texture_Bitdepth(int depth) override;
	virtual int Get_Texture_Bitdepth() const override;
	virtual void Set_Multisample_Mode(RenderBackendMultisampleMode mode) override;
	virtual RenderBackendMultisampleMode Get_Multisample_Mode() const override;

	virtual void Set_Gamma(float gamma, float bright, float contrast,
		bool calibrate, bool uselimit) override;

	virtual void Begin_Scene() override;
	virtual void End_Scene(bool flip_frame) override;
	virtual void Flip_To_Primary() override;
	virtual void Clear(bool clear_color, bool clear_z_stencil,
		const Vector3 & color,
		float dest_alpha, float z, unsigned int stencil) override;
	virtual void Set_Viewport(const RenderBackendViewport & viewport) override;
	virtual void Invalidate_Cached_Render_States() override;

	virtual void Set_Ambient(const Vector3 & color) override;
	virtual void Set_Light_Environment(LightEnvironmentClass * light_env) override;
	virtual void Set_Fog(bool enable, const Vector3 & color, float start, float end) override;
	virtual void Set_Material_Values(const RenderBackendMaterial & material) override;
	virtual void Set_Fill_Mode(RenderBackendFillMode mode) override;
	virtual void Set_Color_Write_Mask(RenderBackendColorWriteMask mask) override;
	virtual RenderBackendColorWriteMask Get_Color_Write_Mask() const override;
	virtual void Set_Alpha_Blend_Enabled(bool enable) override;
	virtual void Set_Blend_Operation(RenderBackendBlendOperation operation) override;
	virtual void Set_Blend_Factors(RenderBackendBlendFactor source,
		RenderBackendBlendFactor destination) override;
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
		RenderBackendTextureArgument argument) override;
	virtual void Set_Texture_Coordinate_Source(unsigned stage,
		RenderBackendTextureCoordinateSource source,
		unsigned uv_array_index) override;
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
		float m00, float m01, float m10, float m11) override;
	virtual TextureClass * Create_Render_Target(int width, int height,
		WW3DFormat format) override;
	virtual void Create_Render_Target(int width, int height, WW3DFormat format,
		WW3DZFormat depth_format, TextureClass ** target, ZTextureClass ** depth_target) override;
	virtual void Set_Render_Target(TextureClass * render_target,
		ZTextureClass * depth_target) override;
	virtual RenderBackendSurface * Create_System_Memory_Surface(unsigned width,
		unsigned height, WW3DFormat format) override;
	virtual bool Lock_Surface(RenderBackendSurface * surface,
		RenderBackendLockedSurface & locked_surface) override;
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
	virtual RenderBackendVertexBuffer * Create_Vertex_Buffer(unsigned size_bytes,
		RenderBackendVertexFormat format, bool dynamic) override;
	virtual RenderBackendIndexBuffer * Create_Index_Buffer(unsigned size_bytes,
		bool dynamic) override;
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
		unsigned offset_bytes, unsigned stride_bytes, unsigned stream) override;
	virtual void Set_Index_Buffer(RenderBackendIndexBuffer * buffer) override;
	virtual void Set_Vertex_Format(RenderBackendVertexFormat format) override;
	virtual void Draw_Indexed_Primitives(RenderBackendPrimitiveType primitive_type,
		unsigned base_vertex_index, unsigned min_vertex_index,
		unsigned vertex_count, unsigned start_index, unsigned primitive_count) override;
	virtual void Draw_Primitive_Up(RenderBackendPrimitiveType primitive_type,
		unsigned primitive_count, const void * vertices, unsigned stride_bytes,
		RenderBackendVertexFormat format) override;

	virtual void Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned stream) override;
	virtual void Set_Vertex_Buffer(const DynamicVBAccessClass & vba) override;
	virtual void Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset) override;
	virtual void Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset) override;
	virtual void Set_Index_Buffer_Index_Offset(unsigned offset) override;

	virtual void Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix,
		float znear, float zfar) override;
	virtual void Set_Transform(RenderBackendTransform transform, const Matrix4x4 & matrix) override;
	virtual void Set_Transform(RenderBackendTransform transform, const Matrix3D & matrix) override;
	virtual void Get_Transform(RenderBackendTransform transform, Matrix4x4 & matrix) override;
	virtual void Set_World_Identity() override;
	virtual void Set_View_Identity() override;
	virtual bool Is_World_Identity() override;
	virtual bool Is_View_Identity() override;

	virtual void Set_Shader(const ShaderClass & shader) override;
	virtual void Get_Shader(ShaderClass & shader) override;
	virtual void Set_Vertex_Shader(uintptr_t shader) override;
	virtual void Set_Pixel_Shader(uintptr_t shader) override;
	virtual void Set_Vertex_Shader_Constant(unsigned reg, const void * data,
		unsigned count) override;
	virtual void Set_Pixel_Shader_Constant(unsigned reg, const void * data,
		unsigned count) override;
	virtual void Set_Texture(unsigned stage, TextureBaseClass * texture) override;
	virtual void Set_Texture_Resource(unsigned stage, const TextureBaseClass * texture) override;
	virtual void Set_Material(const VertexMaterialClass * material) override;
	virtual void Set_Light(unsigned index, const LightClass & light) override;
	virtual void Disable_Light(unsigned index) override;
	virtual void Apply_Render_State_Changes() override;

	virtual void Draw_Triangles(unsigned buffer_type,
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count) override;
	virtual void Draw_Triangles(unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count) override;
	virtual void Draw_Strip(unsigned short start_index,
		unsigned short index_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count) override;

private:
	explicit DX9Backend(bool lite);

	bool Lite;
};
