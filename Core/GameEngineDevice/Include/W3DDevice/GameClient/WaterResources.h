/*
** Command & Conquer Generals Zero Hour(tm)
**
** Water resource boundary for the modern water render system. The renderer
** consumes TextureBaseClass handles; legacy asset/surface construction is
** contained in the implementation of this adapter.
*/

#pragma once

class TextureBaseClass;

TextureBaseClass *Load_Water_Texture(const char *name);
TextureBaseClass *Create_Water_White_Texture();
TextureBaseClass *Create_Water_Depth_Lut_Texture();
void Reinitialize_Water_Procedural_Texture(TextureBaseClass *texture,
	bool depth_lut);
unsigned Get_Water_Texture_Width(const TextureBaseClass *texture);
