/*
** Command & Conquer Generals Zero Hour(tm)
**
** Backend-neutral modern water material. The water render object supplies
** geometry and material inputs; this class owns the explicit shader contract.
*/

#pragma once

#include "WW3D2/ProgrammableMaterial.h"

struct WaterMaterialParameters
{
	Vector4 shroud_projection;
	Vector4 animation;
	Vector4 camera_position;
	Vector4 tint;
	Vector4 effects;
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
		const WaterMaterialParameters &parameters, bool additive_blend);
	bool Apply_Surface(TextureBaseClass *surface_texture,
		TextureBaseClass *normal_texture, TextureBaseClass *foam_texture,
		TextureBaseClass *edge_texture, TextureBaseClass *reflection_texture,
		TextureBaseClass *refraction_texture, TextureBaseClass *environment_texture,
		TextureBaseClass *shroud_texture,
		const WaterMaterialParameters &parameters, bool additive_blend);

	void Shutdown();
	bool ReacquireResources();
	void Reset();

private:
	bool Initialize();
	void Set_Common_Constants(const WaterMaterialParameters &parameters);
	void Set_Surface_State(bool additive_blend, unsigned reflection_stage);

	ShaderProgramClass m_ocean_program;
	ShaderProgramClass m_surface_program;
	ProgrammableMaterialPass m_pass;
};
