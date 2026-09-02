/* Modern explicit water material. */

#include "W3DDevice/GameClient/WaterMaterial.h"

#include <cstddef>

#include "WW3D2/WW3D.h"

namespace
{
	RenderBackendVertexShaderInputLayout Make_Ocean_Vertex_Layout()
	{
		RenderBackendVertexShaderInputLayout layout;
		layout.Add(0, 0, RenderBackendVertexInputType::Float3,
			RenderBackendVertexInputSemantic::Position, 0, 0);
		layout.Add(0, sizeof(float) * 3, RenderBackendVertexInputType::Color,
			RenderBackendVertexInputSemantic::Color, 0, 1);
		layout.Add(0, sizeof(float) * 4, RenderBackendVertexInputType::Float2,
			RenderBackendVertexInputSemantic::TextureCoordinate, 0, 2);
		return layout;
	}

	RenderBackendVertexShaderInputLayout Make_Surface_Vertex_Layout()
	{
		RenderBackendVertexShaderInputLayout layout;
		layout.Add(0, offsetof(WaterSurfaceVertex, x),
			RenderBackendVertexInputType::Float3,
			RenderBackendVertexInputSemantic::Position, 0, 0);
		layout.Add(0, offsetof(WaterSurfaceVertex, nx),
			RenderBackendVertexInputType::Float3,
			RenderBackendVertexInputSemantic::Normal, 0, 1);
		layout.Add(0, offsetof(WaterSurfaceVertex, diffuse),
			RenderBackendVertexInputType::Color,
			RenderBackendVertexInputSemantic::Color, 0, 3);
		layout.Add(0, offsetof(WaterSurfaceVertex, u1),
			RenderBackendVertexInputType::Float2,
			RenderBackendVertexInputSemantic::TextureCoordinate, 0, 5);
		layout.Add(0, offsetof(WaterSurfaceVertex, u2),
			RenderBackendVertexInputType::Float2,
			RenderBackendVertexInputSemantic::TextureCoordinate, 1, 6);
		return layout;
	}

	RenderBackendVertexShaderInputLayout Make_Displacement_Vertex_Layout()
	{
		RenderBackendVertexShaderInputLayout layout;
		layout.Add(0, 0, RenderBackendVertexInputType::Float4,
			RenderBackendVertexInputSemantic::Position, 0, 0);
		layout.Add(0, sizeof(float) * 4, RenderBackendVertexInputType::Float2,
			RenderBackendVertexInputSemantic::TextureCoordinate, 0, 2);
		return layout;
	}
}

WaterMaterialClass::WaterMaterialClass()
{
	m_pass.Set_Program(&m_ocean_program);
}

WaterMaterialClass::~WaterMaterialClass()
{
	Shutdown();
}

bool WaterMaterialClass::Initialize()
{
	if (m_ocean_program.Is_Initialized() &&
		m_displacement_program.Is_Initialized() &&
		m_surface_program.Is_Initialized() &&
		m_sky_program.Is_Initialized() &&
		m_track_program.Is_Initialized())
	{
		return true;
	}

	m_ocean_program.Shutdown();
	m_displacement_program.Shutdown();
	m_surface_program.Shutdown();
	m_sky_program.Shutdown();
	m_track_program.Shutdown();
	if (!m_ocean_program.Initialize("shaders/ocean.vso", "shaders/ocean.pso",
		Make_Ocean_Vertex_Layout()))
	{
		return false;
	}
	if (!m_displacement_program.Initialize("shaders/water_displacement.vso",
		"shaders/water_displacement.pso", Make_Displacement_Vertex_Layout()))
	{
		m_ocean_program.Shutdown();
		return false;
	}
	if (!m_surface_program.Initialize("shaders/water_surface.vso",
		"shaders/water_surface.pso", Make_Surface_Vertex_Layout()))
	{
		m_displacement_program.Shutdown();
		m_ocean_program.Shutdown();
		return false;
	}
	if (!m_sky_program.Initialize("shaders/water_sky.vso",
		"shaders/water_sky.pso", Make_Surface_Vertex_Layout()))
	{
		m_displacement_program.Shutdown();
		m_surface_program.Shutdown();
		m_ocean_program.Shutdown();
		return false;
	}
	if (!m_track_program.Initialize("shaders/water_track.vso",
		"shaders/water_track.pso", Make_Surface_Vertex_Layout()))
	{
		m_sky_program.Shutdown();
		m_displacement_program.Shutdown();
		m_surface_program.Shutdown();
		m_ocean_program.Shutdown();
		return false;
	}
	return true;
}

void WaterMaterialClass::Set_Common_Constants(
	const WaterMaterialParameters &parameters)
{
	const Vector4 vertex_constants[] = {
		parameters.shroud_projection,
		parameters.animation,
		parameters.camera_position,
		parameters.displacement_domain,
		parameters.surface_options};
	const Vector4 pixel_constants[] = {
		parameters.tint,
		parameters.effects,
		parameters.animation,
		parameters.camera_position,
		parameters.displacement_domain,
		parameters.surface_options};
	m_pass.Set_Vertex_Constants(0, vertex_constants,
		static_cast<unsigned>(std::size(vertex_constants)));
	m_pass.Set_Pixel_Constants(0, pixel_constants,
		static_cast<unsigned>(std::size(pixel_constants)));
}

void WaterMaterialClass::Set_Displacement_State()
{
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend == nullptr)
	{
		return;
	}

	backend->Set_Depth_Test_Enabled(false);
	backend->Set_Depth_Write_Enabled(false);
	backend->Set_Depth_Function(RenderBackendCompareFunction::Always);
	backend->Set_Cull_Mode(RenderBackendCullMode::None);
	backend->Set_Lighting_Enabled(false);
	backend->Set_Color_Write_Mask(RenderBackendColorWriteMask::All);
	backend->Set_Alpha_Blend_Enabled(false);
	backend->Set_Source_Blend_Factor(RenderBackendBlendFactor::One);
	backend->Set_Destination_Blend_Factor(RenderBackendBlendFactor::Zero);
	backend->Set_Texture_Address_Mode(0, true,
		RenderBackendTextureAddressMode::Wrap);
	backend->Set_Texture_Address_Mode(0, false,
		RenderBackendTextureAddressMode::Wrap);
	backend->Set_Texture_Filter(0,
		RenderBackendTextureFilterType::Minification,
		RenderBackendTextureFilter::Linear);
	backend->Set_Texture_Filter(0,
		RenderBackendTextureFilterType::Magnification,
		RenderBackendTextureFilter::Linear);
}

void WaterMaterialClass::Set_Surface_State(bool additive_blend,
	unsigned reflection_stage)
{
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend == nullptr)
	{
		return;
	}

	backend->Set_Depth_Test_Enabled(true);
	backend->Set_Depth_Write_Enabled(false);
	backend->Set_Depth_Function(RenderBackendCompareFunction::LessEqual);
	backend->Set_Cull_Mode(RenderBackendCullMode::None);
	backend->Set_Lighting_Enabled(false);
	backend->Set_Color_Write_Mask(RenderBackendColorWriteMask::All);
	backend->Set_Alpha_Blend_Enabled(true);
	if (additive_blend)
	{
		backend->Set_Source_Blend_Factor(RenderBackendBlendFactor::One);
		backend->Set_Destination_Blend_Factor(RenderBackendBlendFactor::One);
	}
	else
	{
		backend->Set_Source_Blend_Factor(RenderBackendBlendFactor::SourceAlpha);
		backend->Set_Destination_Blend_Factor(
			RenderBackendBlendFactor::InverseSourceAlpha);
	}

	for (unsigned stage = 0; stage < MAX_TEXTURE_STAGES; ++stage)
	{
		backend->Set_Texture_Address_Mode(stage, true,
			RenderBackendTextureAddressMode::Wrap);
		backend->Set_Texture_Address_Mode(stage, false,
			RenderBackendTextureAddressMode::Wrap);
		backend->Set_Texture_Filter(stage,
			RenderBackendTextureFilterType::Minification,
			RenderBackendTextureFilter::Linear);
		backend->Set_Texture_Filter(stage,
			RenderBackendTextureFilterType::Magnification,
			RenderBackendTextureFilter::Linear);
	}
	backend->Set_Texture_Address_Mode(7, true,
		RenderBackendTextureAddressMode::Clamp);
	backend->Set_Texture_Address_Mode(7, false,
		RenderBackendTextureAddressMode::Clamp);
	// Ocean.fx samples the generated displacement field with a clamped
	// sampler. Wrapping this field at a patch edge creates wave spikes when
	// the +1 texel normal samples cross the border.
	backend->Set_Texture_Address_Mode(1, true,
		RenderBackendTextureAddressMode::Clamp);
	backend->Set_Texture_Address_Mode(1, false,
		RenderBackendTextureAddressMode::Clamp);
	backend->Set_Texture_Address_Mode(5, true,
		RenderBackendTextureAddressMode::Clamp);
	backend->Set_Texture_Address_Mode(5, false,
		RenderBackendTextureAddressMode::Clamp);
	if (reflection_stage < MAX_TEXTURE_STAGES)
	{
		backend->Set_Texture_Address_Mode(reflection_stage, true,
			RenderBackendTextureAddressMode::Clamp);
		backend->Set_Texture_Address_Mode(reflection_stage, false,
			RenderBackendTextureAddressMode::Clamp);
	}
}

void WaterMaterialClass::Set_Track_State()
{
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend == nullptr)
	{
		return;
	}

	backend->Set_Depth_Test_Enabled(true);
	backend->Set_Depth_Write_Enabled(false);
	backend->Set_Depth_Function(RenderBackendCompareFunction::LessEqual);
	backend->Set_Cull_Mode(RenderBackendCullMode::None);
	backend->Set_Lighting_Enabled(false);
	backend->Set_Color_Write_Mask(RenderBackendColorWriteMask::All);
	backend->Set_Alpha_Blend_Enabled(true);
	backend->Set_Source_Blend_Factor(RenderBackendBlendFactor::SourceAlpha);
	backend->Set_Destination_Blend_Factor(
		RenderBackendBlendFactor::InverseSourceAlpha);

	for (unsigned stage = 0; stage < MAX_TEXTURE_STAGES; ++stage)
	{
		backend->Set_Texture_Address_Mode(stage, true,
			RenderBackendTextureAddressMode::Wrap);
		backend->Set_Texture_Address_Mode(stage, false,
			RenderBackendTextureAddressMode::Wrap);
		backend->Set_Texture_Filter(stage,
			RenderBackendTextureFilterType::Minification,
			RenderBackendTextureFilter::Linear);
		backend->Set_Texture_Filter(stage,
			RenderBackendTextureFilterType::Magnification,
			RenderBackendTextureFilter::Linear);
	}
}

void WaterMaterialClass::Set_Sky_State(bool alpha_blend, bool clamp_texture)
{
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend == nullptr)
	{
		return;
	}

	// The reflection sky is an explicit background pass. It does not read or
	// write depth, does not depend on lighting, and owns its blend state.
	backend->Set_Depth_Test_Enabled(false);
	backend->Set_Depth_Write_Enabled(false);
	backend->Set_Depth_Function(RenderBackendCompareFunction::Always);
	backend->Set_Cull_Mode(RenderBackendCullMode::None);
	backend->Set_Lighting_Enabled(false);
	backend->Set_Color_Write_Mask(RenderBackendColorWriteMask::All);
	backend->Set_Alpha_Blend_Enabled(alpha_blend);
	if (alpha_blend)
	{
		backend->Set_Source_Blend_Factor(RenderBackendBlendFactor::SourceAlpha);
		backend->Set_Destination_Blend_Factor(
			RenderBackendBlendFactor::InverseSourceAlpha);
	}
	else
	{
		backend->Set_Source_Blend_Factor(RenderBackendBlendFactor::One);
		backend->Set_Destination_Blend_Factor(RenderBackendBlendFactor::Zero);
	}

	const RenderBackendTextureAddressMode address_mode = clamp_texture ?
		RenderBackendTextureAddressMode::Clamp :
		RenderBackendTextureAddressMode::Wrap;
	backend->Set_Texture_Address_Mode(0, true, address_mode);
	backend->Set_Texture_Address_Mode(0, false, address_mode);
	backend->Set_Texture_Filter(0,
		RenderBackendTextureFilterType::Minification,
		RenderBackendTextureFilter::Linear);
	backend->Set_Texture_Filter(0,
		RenderBackendTextureFilterType::Magnification,
		RenderBackendTextureFilter::Linear);
}

bool WaterMaterialClass::Apply_Ocean(TextureBaseClass *surface_texture,
	TextureBaseClass *displacement_texture,
	TextureBaseClass *normal_texture, TextureBaseClass *foam_texture,
	TextureBaseClass *reflection_texture, TextureBaseClass *refraction_texture,
	TextureBaseClass *environment_texture, TextureBaseClass *shroud_texture,
	TextureBaseClass *scene_depth_texture,
	const WaterMaterialParameters &parameters, bool additive_blend)
{
	if (!Initialize())
	{
		return false;
	}

	m_pass.Set_Program(&m_ocean_program);
	m_pass.Set_Texture(0, surface_texture);
	m_pass.Set_Texture(1, displacement_texture);
	m_pass.Set_Texture(2, normal_texture);
	m_pass.Set_Texture(3, foam_texture);
	m_pass.Set_Texture(4, reflection_texture);
	m_pass.Set_Texture(5, refraction_texture);
	m_pass.Set_Texture(6, environment_texture);
	m_pass.Set_Texture(7, shroud_texture);
	m_pass.Set_Texture(MAX_TEXTURE_STAGES, scene_depth_texture);
	Set_Common_Constants(parameters);
	Set_Surface_State(additive_blend, 4);
	return m_pass.Apply();
}

bool WaterMaterialClass::Apply_Displacement(
	TextureBaseClass *static_displacement_texture,
	const Vector4 &animation, const Vector4 &displacement_domain)
{
	if (!Initialize())
	{
		return false;
	}

	m_pass.Set_Program(&m_displacement_program);
	for (unsigned stage = 0; stage < MAX_TEXTURE_STAGES; ++stage)
	{
		m_pass.Set_Texture(stage, stage == 0 ? static_displacement_texture : nullptr);
	}
	const Vector4 constants[] = {animation, displacement_domain};
	m_pass.Set_Pixel_Constants(0, constants,
		static_cast<unsigned>(std::size(constants)));
	Set_Displacement_State();
	return m_pass.Apply();
}

bool WaterMaterialClass::Apply_Surface(TextureBaseClass *surface_texture,
	TextureBaseClass *normal_texture, TextureBaseClass *foam_texture,
	TextureBaseClass *edge_texture, TextureBaseClass *reflection_texture,
	TextureBaseClass *refraction_texture, TextureBaseClass *environment_texture,
	TextureBaseClass *shroud_texture, TextureBaseClass *scene_depth_texture,
	const WaterMaterialParameters &parameters, bool additive_blend)
{
	if (!Initialize())
	{
		return false;
	}

	m_pass.Set_Program(&m_surface_program);
	m_pass.Set_Texture(0, surface_texture);
	m_pass.Set_Texture(1, normal_texture);
	m_pass.Set_Texture(2, foam_texture);
	m_pass.Set_Texture(3, edge_texture);
	m_pass.Set_Texture(4, reflection_texture);
	m_pass.Set_Texture(5, refraction_texture);
	m_pass.Set_Texture(6, environment_texture);
	m_pass.Set_Texture(7, shroud_texture);
	m_pass.Set_Texture(MAX_TEXTURE_STAGES, scene_depth_texture);
	Set_Common_Constants(parameters);
	Set_Surface_State(additive_blend, 4);
	return m_pass.Apply();
}

bool WaterMaterialClass::Apply_Track(TextureBaseClass *wave_texture)
{
	if (!Initialize())
	{
		return false;
	}

	m_pass.Set_Program(&m_track_program);
	for (unsigned stage = 0; stage < MAX_TEXTURE_STAGES; ++stage)
	{
		m_pass.Set_Texture(stage, stage == 0 ? wave_texture : nullptr);
	}
	Set_Track_State();
	return m_pass.Apply();
}

bool WaterMaterialClass::Apply_Sky(TextureBaseClass *texture, bool alpha_blend,
	bool clamp_texture)
{
	if (!Initialize())
	{
		return false;
	}

	m_pass.Set_Program(&m_sky_program);
	for (unsigned stage = 0; stage < MAX_TEXTURE_STAGES; ++stage)
	{
		m_pass.Set_Texture(stage, stage == 0 ? texture : nullptr);
	}
	Set_Sky_State(alpha_blend, clamp_texture);
	return m_pass.Apply();
}

void WaterMaterialClass::Shutdown()
{
	m_pass.Reset();
	m_sky_program.Shutdown();
	m_track_program.Shutdown();
	m_surface_program.Shutdown();
	m_displacement_program.Shutdown();
	m_ocean_program.Shutdown();
}

bool WaterMaterialClass::ReacquireResources()
{
	Shutdown();
	return Initialize();
}

void WaterMaterialClass::Reset()
{
	m_pass.Reset();
}
