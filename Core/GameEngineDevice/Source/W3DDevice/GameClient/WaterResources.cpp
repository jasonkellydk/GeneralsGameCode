/*
** Command & Conquer Generals Zero Hour(tm)
*/

#include "W3DDevice/GameClient/WaterResources.h"

#include "WW3D2/AssetMgr.h"
#include "WW3D2/Texture.h"

static void Initialize_Water_Depth_Lut(TextureClass *texture)
{
	if (texture == nullptr)
		return;

	SurfaceClass *surface = texture->Get_Surface_Level();
	if (surface == nullptr)
		return;

	int pitch = 0;
	void *bits = surface->Lock(&pitch);
	const unsigned int bytes_per_pixel = surface->Get_Bytes_Per_Pixel();
	if (bits != nullptr && bytes_per_pixel != 0)
	{
		for (unsigned int x = 0; x < 256; ++x)
		{
			const unsigned int value = x;
			const unsigned int color = 0xff000000u |
				(value << 16) | (value << 8) | value;
			surface->Draw_Pixel(static_cast<int>(x), 0, color,
				bytes_per_pixel, bits, pitch);
		}
	}
	surface->Unlock();
	REF_PTR_RELEASE(surface);
}

static void Initialize_Water_White_Texture(TextureClass *texture)
{
	if (texture == nullptr)
		return;

	SurfaceClass *surface = texture->Get_Surface_Level();
	if (surface == nullptr)
		return;

	int pitch = 0;
	void *bits = surface->Lock(&pitch);
	const unsigned int bytes_per_pixel = surface->Get_Bytes_Per_Pixel();
	if (bits != nullptr && bytes_per_pixel != 0)
		surface->Draw_Pixel(0, 0, 0xffffffff, bytes_per_pixel, bits, pitch);
	surface->Unlock();
	REF_PTR_RELEASE(surface);
}

TextureBaseClass *Load_Water_Texture(const char *name)
{
	return WW3DAssetManager::Get_Instance()->Get_Texture(name);
}

TextureBaseClass *Create_Water_White_Texture()
{
	TextureClass *texture = MSGNEW("TextureClass") TextureClass(
		1, 1, WW3D_FORMAT_A4R4G4B4, MIP_LEVELS_1);
	Initialize_Water_White_Texture(texture);
	return texture;
}

TextureBaseClass *Create_Water_Depth_Lut_Texture()
{
	TextureClass *texture = MSGNEW("TextureClass") TextureClass(
		256, 1, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1);
	Initialize_Water_Depth_Lut(texture);
	return texture;
}

void Reinitialize_Water_Procedural_Texture(TextureBaseClass *texture,
	bool depth_lut)
{
	if (texture == nullptr || texture->Is_Initialized())
		return;

	texture->Init();
	TextureClass *legacy_texture = texture->As_TextureClass();
	if (depth_lut)
		Initialize_Water_Depth_Lut(legacy_texture);
	else
		Initialize_Water_White_Texture(legacy_texture);
}

unsigned Get_Water_Texture_Width(const TextureBaseClass *texture)
{
	return texture == nullptr ? 0u : static_cast<unsigned>(texture->Get_Width());
}
