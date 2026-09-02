/* Generals terrain material. */

#include "W3DDevice/GameClient/TerrainMaterial.h"

#include <cstddef>

#include "Common/GlobalData.h"
#include "WW3D2/Shader.h"
#include "WW3D2/Texture.h"
#include "WW3D2/VertexFormat.h"
#include "WW3D2/WW3D.h"

namespace
{
	RenderBackendVertexShaderInputLayout Make_Terrain_Vertex_Layout()
	{
		RenderBackendVertexShaderInputLayout layout;
		layout.Add(0, offsetof(VertexFormatXYZDUV2, x),
			RenderBackendVertexInputType::Float3,
			RenderBackendVertexInputSemantic::Position, 0, 0);
		layout.Add(0, offsetof(VertexFormatXYZDUV2, diffuse),
			RenderBackendVertexInputType::Color,
			RenderBackendVertexInputSemantic::Color, 0, 3);
		layout.Add(0, offsetof(VertexFormatXYZDUV2, u1),
			RenderBackendVertexInputType::Float2,
			RenderBackendVertexInputSemantic::TextureCoordinate, 0, 5);
		layout.Add(0, offsetof(VertexFormatXYZDUV2, u2),
			RenderBackendVertexInputType::Float2,
			RenderBackendVertexInputSemantic::TextureCoordinate, 1, 6);
		return layout;
	}

	RenderBackendVertexShaderInputLayout Make_Terrain_Overlay_Vertex_Layout()
	{
		RenderBackendVertexShaderInputLayout layout;
		layout.Add(0, offsetof(VertexFormatXYZNDUV2, x),
			RenderBackendVertexInputType::Float3,
			RenderBackendVertexInputSemantic::Position, 0, 0);
		layout.Add(0, offsetof(VertexFormatXYZNDUV2, diffuse),
			RenderBackendVertexInputType::Color,
			RenderBackendVertexInputSemantic::Color, 0, 3);
		layout.Add(0, offsetof(VertexFormatXYZNDUV2, u1),
			RenderBackendVertexInputType::Float2,
			RenderBackendVertexInputSemantic::TextureCoordinate, 0, 5);
		layout.Add(0, offsetof(VertexFormatXYZNDUV2, u2),
			RenderBackendVertexInputType::Float2,
			RenderBackendVertexInputSemantic::TextureCoordinate, 1, 6);
		return layout;
	}

	RenderBackendCullMode Get_Terrain_Cull_Mode()
	{
		return ShaderClass::Is_Backface_Culling_Inverted()
			? RenderBackendCullMode::CounterClockwise
			: RenderBackendCullMode::Clockwise;
	}
}

TerrainMaterialClass::TerrainMaterialClass()
{
	m_pass.Set_Program(&m_program);
}

TerrainMaterialClass::~TerrainMaterialClass()
{
	Shutdown();
}

bool TerrainMaterialClass::Initialize()
{
	if (m_program.Is_Initialized() && m_overlay_program.Is_Initialized())
	{
		return true;
	}
	m_program.Shutdown();
	m_overlay_program.Shutdown();
	if (!m_program.Initialize("shaders/terrain.vso", "shaders/terrain.pso",
		Make_Terrain_Vertex_Layout()))
	{
		return false;
	}
	if (!m_overlay_program.Initialize("shaders/terrain.vso", "shaders/terrain.pso",
		Make_Terrain_Overlay_Vertex_Layout()))
	{
		m_program.Shutdown();
		return false;
	}
	return true;
}

bool TerrainMaterialClass::Apply(TextureBaseClass *base_texture,
	TextureBaseClass *blend_texture, TextureBaseClass *cloud_texture,
	TextureBaseClass *lightmap_texture, TextureBaseClass *shroud_texture,
	const TerrainMaterialParameters &parameters, bool extra_blend)
{
	if (!Initialize())
	{
		return false;
	}

	m_pass.Set_Program(extra_blend ? &m_overlay_program : &m_program);
	m_pass.Set_Texture(0, base_texture);
	m_pass.Set_Texture(1, blend_texture);
	m_pass.Set_Texture(2, cloud_texture);
	m_pass.Set_Texture(3, lightmap_texture);
	m_pass.Set_Texture(4, shroud_texture);
	// Optional terrain modulation is part of the material contract, so its
	// feature bits must describe the resources that are actually usable in
	// this pass.  File-backed modulation textures can be released and rebuilt
	// independently of the terrain atlas during a device reset.
	const bool cloud_ready = cloud_texture != nullptr &&
		cloud_texture->Ensure_Render_Backend_Texture() &&
		!cloud_texture->Is_Missing_Texture();
	const bool lightmap_ready = lightmap_texture != nullptr &&
		lightmap_texture->Ensure_Render_Backend_Texture() &&
		!lightmap_texture->Is_Missing_Texture();
	const bool shroud_ready = parameters.options.Y > 0.5f &&
		shroud_texture != nullptr &&
		shroud_texture->Ensure_Render_Backend_Texture() &&
		!shroud_texture->Is_Missing_Texture();
	Vector4 effective_features = parameters.features;
	effective_features.X = parameters.features.X > 0.5f && cloud_ready ?
		1.0f : 0.0f;
	effective_features.Y = parameters.features.Y > 0.5f && lightmap_ready ?
		1.0f : 0.0f;
	const Vector4 vertex_constants[] = {
		parameters.cloud_projection,
		parameters.lightmap_projection,
		parameters.shroud_projection,
		parameters.vertex_options};
	const Vector4 pixel_constants[] = {
		effective_features,
		parameters.lighting,
		Vector4(parameters.options.X, shroud_ready ? 1.0f : 0.0f,
			parameters.options.Z, parameters.options.W)};
	m_pass.Set_Vertex_Constants(0, vertex_constants,
		4);
	m_pass.Set_Pixel_Constants(0, pixel_constants,
		3);

	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend == nullptr)
	{
		return false;
	}

	// Terrain atlas textures are packed and must not bleed into their
	// neighbours. Projected modulation textures repeat over world space.
	for (unsigned stage = 0; stage < 2; ++stage)
	{
		backend->Set_Texture_Address_Mode(stage, true,
			RenderBackendTextureAddressMode::Clamp);
		backend->Set_Texture_Address_Mode(stage, false,
			RenderBackendTextureAddressMode::Clamp);
	}
	for (unsigned stage = 2; stage < 5; ++stage)
	{
		backend->Set_Texture_Address_Mode(stage, true,
			RenderBackendTextureAddressMode::Wrap);
		backend->Set_Texture_Address_Mode(stage, false,
			RenderBackendTextureAddressMode::Wrap);
	}

	const bool filtered = TheGlobalData != nullptr &&
		(TheGlobalData->m_bilinearTerrainTex || TheGlobalData->m_trilinearTerrainTex);
	for (unsigned stage = 0; stage < 2; ++stage)
	{
		const RenderBackendTextureFilter filter = filtered ?
			RenderBackendTextureFilter::Linear : RenderBackendTextureFilter::Point;
		backend->Set_Texture_Filter(stage,
			RenderBackendTextureFilterType::Minification, filter);
		backend->Set_Texture_Filter(stage,
			RenderBackendTextureFilterType::Magnification, filter);
		backend->Set_Texture_Filter(stage,
			RenderBackendTextureFilterType::MipMap,
			TheGlobalData != nullptr && TheGlobalData->m_trilinearTerrainTex ?
				RenderBackendTextureFilter::Linear : RenderBackendTextureFilter::Point);
	}
	backend->Set_Texture_Filter(2, RenderBackendTextureFilterType::Minification,
		RenderBackendTextureFilter::Linear);
	backend->Set_Texture_Filter(2, RenderBackendTextureFilterType::Magnification,
		RenderBackendTextureFilter::Linear);
	backend->Set_Texture_Filter(3, RenderBackendTextureFilterType::Minification,
		RenderBackendTextureFilter::Point);
	backend->Set_Texture_Filter(3, RenderBackendTextureFilterType::Magnification,
		RenderBackendTextureFilter::Linear);
	backend->Set_Texture_Filter(4, RenderBackendTextureFilterType::Minification,
		RenderBackendTextureFilter::Linear);
	backend->Set_Texture_Filter(4, RenderBackendTextureFilterType::Magnification,
		RenderBackendTextureFilter::Linear);

	// The regular material is opaque. Tile blending is resolved in the pixel
	// shader; vertex alpha is terrain blend data, not render-target alpha. The
	// extra pass intentionally uses that alpha for compositing.
	backend->Set_Depth_Test_Enabled(true);
	backend->Set_Depth_Write_Enabled(!extra_blend);
	backend->Set_Depth_Function(RenderBackendCompareFunction::LessEqual);
	backend->Set_Cull_Mode(Get_Terrain_Cull_Mode());
	backend->Set_Lighting_Enabled(false);
	backend->Set_Color_Write_Mask(extra_blend ? RenderBackendColorWriteMask::All :
		RenderBackendColorWriteMask::RGB);
	backend->Set_Alpha_Blend_Enabled(extra_blend);
	if (extra_blend)
	{
		backend->Set_Source_Blend_Factor(RenderBackendBlendFactor::SourceAlpha);
		backend->Set_Destination_Blend_Factor(
			RenderBackendBlendFactor::InverseSourceAlpha);
	}
	return m_pass.Apply();
}

bool TerrainMaterialClass::Apply_Shroud(TextureBaseClass *shroud_texture,
	const TerrainMaterialParameters &parameters)
{
	if (!Initialize())
	{
		return false;
	}

	// The shroud is a second pass over the already depth-tested terrain.  It
	// deliberately uses the same terrain program and vertex contract; the
	// pixel shader selects its shroud-only mode from TerrainFeatures.w.
	m_pass.Set_Program(&m_program);
	m_pass.Set_Texture(0, nullptr);
	m_pass.Set_Texture(1, nullptr);
	m_pass.Set_Texture(2, nullptr);
	m_pass.Set_Texture(3, nullptr);
	m_pass.Set_Texture(4, shroud_texture);
	const Vector4 vertex_constants[] = {
		parameters.cloud_projection,
		parameters.lightmap_projection,
		parameters.shroud_projection,
		parameters.vertex_options};
	const Vector4 pixel_constants[] = {
		parameters.features,
		parameters.lighting,
		parameters.options};
	m_pass.Set_Vertex_Constants(0, vertex_constants,
		4);
	m_pass.Set_Pixel_Constants(0, pixel_constants,
		3);

	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend == nullptr)
	{
		return false;
	}

	// W3DShroud owns this texture and already sets clamp addressing on it.
	// Keep the backend sampler state in agreement so the one-cell border is
	// sampled instead of wrapping into the opposite side of the map.
	for (unsigned stage = 0; stage < 4; ++stage)
	{
		backend->Set_Texture_Address_Mode(stage, true,
			RenderBackendTextureAddressMode::Clamp);
		backend->Set_Texture_Address_Mode(stage, false,
			RenderBackendTextureAddressMode::Clamp);
	}
	backend->Set_Texture_Address_Mode(4, true,
		RenderBackendTextureAddressMode::Clamp);
	backend->Set_Texture_Address_Mode(4, false,
		RenderBackendTextureAddressMode::Clamp);
	backend->Set_Texture_Filter(4,
		RenderBackendTextureFilterType::Minification,
		RenderBackendTextureFilter::Linear);
	backend->Set_Texture_Filter(4,
		RenderBackendTextureFilterType::Magnification,
		RenderBackendTextureFilter::Linear);

	// Match the original multiplicative shroud pass: only terrain pixels at
	// the existing depth are darkened, and the shroud RGB value is multiplied
	// into the framebuffer by the blend state.
	backend->Set_Depth_Test_Enabled(true);
	backend->Set_Depth_Write_Enabled(false);
	backend->Set_Depth_Function(RenderBackendCompareFunction::Equal);
	backend->Set_Cull_Mode(Get_Terrain_Cull_Mode());
	backend->Set_Lighting_Enabled(false);
	backend->Set_Color_Write_Mask(RenderBackendColorWriteMask::RGB);
	backend->Set_Alpha_Blend_Enabled(true);
	backend->Set_Source_Blend_Factor(RenderBackendBlendFactor::Zero);
	backend->Set_Destination_Blend_Factor(
		RenderBackendBlendFactor::SourceColor);

	return m_pass.Apply();
}

void TerrainMaterialClass::Shutdown()
{
	// The backend owns the native shader objects represented by these programs.
	// Release them with the terrain resources so a device reset can never leave
	// the material holding a valid-looking handle to an old native object.
	m_pass.Reset();
	m_overlay_program.Shutdown();
	m_program.Shutdown();
}

bool TerrainMaterialClass::ReacquireResources()
{
	Shutdown();
	return Initialize();
}

void TerrainMaterialClass::Reset()
{
	m_pass.Reset();
}
