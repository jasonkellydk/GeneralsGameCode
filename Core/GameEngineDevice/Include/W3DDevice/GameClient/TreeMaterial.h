/*
** Command & Conquer Generals Zero Hour(tm)
**
** Explicit programmable material for the batched tree/grass renderer.
*/

#pragma once

#include "WW3D2/ProgrammableMaterial.h"

enum { TREE_MATERIAL_MAX_SWAY_TYPES = 10 };

struct TreeMaterialParameters
{
	Vector4 sway[TREE_MATERIAL_MAX_SWAY_TYPES];
	Vector4 shroud_projection;
	// x: shroud enabled, y: alpha cutoff, z: diffuse overbright multiplier.
	Vector4 options;
};

class TreeMaterialClass
{
public:
	TreeMaterialClass();
	~TreeMaterialClass();

	TreeMaterialClass(const TreeMaterialClass &) = delete;
	TreeMaterialClass &operator=(const TreeMaterialClass &) = delete;

	bool Apply(TextureBaseClass *tree_texture, TextureBaseClass *shroud_texture,
		const TreeMaterialParameters &parameters);
	void Shutdown();
	void Reset();

private:
	bool Initialize();

	ShaderProgramClass m_program;
	ProgrammableMaterialPass m_pass;
};
