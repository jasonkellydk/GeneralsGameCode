/*
** Command & Conquer Generals Zero Hour(tm)
**
** Backend-neutral modern water material. The water render object supplies
** geometry and material inputs; this class owns the explicit shader contract.
*/

#pragma once

#include <cstdint>

#include "WW3D2/ProgrammableMaterial.h"

// The water shaders consume this explicit stream contract.  It is a
// backend-neutral submission type; it is intentionally not one of the old
// WW3D fixed-function VertexFormat structures.
struct WaterSurfaceVertex
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	std::uint32_t diffuse;
	float u1;
	float v1;
	float u2;
	float v2;
};

struct WaterMaterialParameters
{
	Vector4 shroud_projection;
	Vector4 animation;
	Vector4 camera_position;
	Vector4 displacement_domain;
	Vector4 tint;
	Vector4 effects;
	Vector4 surface_options;
};

class WaterMaterialClass
{
public:
	WaterMaterialClass();
	~WaterMaterialClass();

	WaterMaterialClass(const WaterMaterialClass &) = delete;
	WaterMaterialClass &operator=(const WaterMaterialClass &) = delete;

	bool Apply_Ocean(TextureBaseClass *surface_texture,
		TextureBaseClass *displacement_texture,
		TextureBaseClass *normal_texture, TextureBaseClass *foam_texture,
		TextureBaseClass *reflection_texture, TextureBaseClass *refraction_texture,
		TextureBaseClass *environment_texture, TextureBaseClass *shroud_texture,
		TextureBaseClass *scene_depth_texture,
		const WaterMaterialParameters &parameters, bool additive_blend);
	bool Apply_Displacement(TextureBaseClass *static_displacement_texture,
		const Vector4 &animation, const Vector4 &displacement_domain);
	bool Apply_Surface(TextureBaseClass *surface_texture,
		TextureBaseClass *normal_texture, TextureBaseClass *foam_texture,
		TextureBaseClass *edge_texture, TextureBaseClass *reflection_texture,
		TextureBaseClass *refraction_texture, TextureBaseClass *environment_texture,
		TextureBaseClass *shroud_texture, TextureBaseClass *scene_depth_texture,
		const WaterMaterialParameters &parameters, bool additive_blend);
	bool Apply_Track(TextureBaseClass *wave_texture);
	bool Apply_Sky(TextureBaseClass *texture, bool alpha_blend,
		bool clamp_texture);

	void Shutdown();
	bool ReacquireResources();
	void Reset();

private:
	bool Initialize();
	void Set_Common_Constants(const WaterMaterialParameters &parameters);
	void Set_Surface_State(bool additive_blend, unsigned reflection_stage);
	void Set_Track_State();
	void Set_Displacement_State();
	void Set_Sky_State(bool alpha_blend, bool clamp_texture);

	ShaderProgramClass m_ocean_program;
	ShaderProgramClass m_displacement_program;
	ShaderProgramClass m_surface_program;
	ShaderProgramClass m_sky_program;
	ShaderProgramClass m_track_program;
	ProgrammableMaterialPass m_pass;
};
