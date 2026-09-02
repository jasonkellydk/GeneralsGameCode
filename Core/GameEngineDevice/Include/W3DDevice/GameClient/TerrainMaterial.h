/*
** Command & Conquer Generals Zero Hour(tm)
**
** Generals terrain material. The material is engine-side; it is expressed in
** terms of the reusable WW3D2 programmable material pass and contains no
** graphics API types.
*/

#pragma once

#include "WW3D2/ProgrammableMaterial.h"

struct TerrainMaterialParameters
{
	Vector4 cloud_projection;
	Vector4 lightmap_projection;
	Vector4 shroud_projection;
	Vector4 vertex_options;
	Vector4 features;
	Vector4 lighting;
	// x: disable texture sampling, y: sample the terrain shroud in the
	// unified material pass. The remaining components are reserved for future
	// terrain material features.
	Vector4 options;
};

class TerrainMaterialClass
{
public:
	TerrainMaterialClass();
	~TerrainMaterialClass();

	TerrainMaterialClass(const TerrainMaterialClass &) = delete;
	TerrainMaterialClass &operator=(const TerrainMaterialClass &) = delete;

	bool Apply(TextureBaseClass *base_texture, TextureBaseClass *blend_texture,
	TextureBaseClass *cloud_texture, TextureBaseClass *lightmap_texture,
	TextureBaseClass *shroud_texture,
		const TerrainMaterialParameters &parameters, bool extra_blend = false);
	bool Apply_Shroud(TextureBaseClass *shroud_texture,
		const TerrainMaterialParameters &parameters);
	void Shutdown();
	bool ReacquireResources();
	void Reset();

private:
	bool Initialize();

	ShaderProgramClass m_program;
	ShaderProgramClass m_overlay_program;
	ProgrammableMaterialPass m_pass;
};
