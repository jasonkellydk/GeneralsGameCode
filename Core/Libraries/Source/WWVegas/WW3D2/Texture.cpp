/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/Texture.cpp                            $*
 *                                                                                             *
 *                  $Org Author:: Steve_t                                                     $*
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 08/05/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 85                                                          $*
 *                                                                                             *
 * 06/27/02 KM Texture class abstraction																			*
 * 08/05/02 KM Texture class redesign (revisited)
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   FileListTextureClass::Load_Frame_Surface -- Load source texture                           *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "Texture.h"
#include "WW3D.h"
#include "Backend/RenderBackend.h"
#include "WWLib/TARGA.h"
#include <WWLib/nstrdup.h>
#include "W3DFile.h"
#include "AssetMgr.h"
#include "TextureLoader.h"
#include "MissingTexture.h"
#include "WWLib/ffactory.h"
#include "MeshMatDesc.h"
#include "TextureThumbnail.h"
#include "WWDebug/wwprofile.h"

const unsigned DEFAULT_INACTIVATION_TIME=20000;

/*
** Definitions of static members:
*/

static unsigned unused_texture_id;

// This throttles submissions to the background texture loading queue.
static unsigned TexturesAppliedPerFrame;
const unsigned MAX_TEXTURES_APPLIED_PER_FRAME=2;

static unsigned Compute_Surface_Size_Bytes(const RenderBackendTextureDescription &desc)
{
	const unsigned width = desc.width;
	const unsigned height = desc.height;

	switch (desc.format)
	{
	case WW3D_FORMAT_DXT1:
	{
		const unsigned blocks_x = ((width + 3U) / 4U) ? ((width + 3U) / 4U) : 1U;
		const unsigned blocks_y = ((height + 3U) / 4U) ? ((height + 3U) / 4U) : 1U;
		return blocks_x * blocks_y * 8U;
	}
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
	{
		const unsigned blocks_x = ((width + 3U) / 4U) ? ((width + 3U) / 4U) : 1U;
		const unsigned blocks_y = ((height + 3U) / 4U) ? ((height + 3U) / 4U) : 1U;
		return blocks_x * blocks_y * 16U;
	}
	case WW3D_FORMAT_UNKNOWN:
		if (desc.depth_format == WW3D_ZFORMAT_D16 || desc.depth_format == WW3D_ZFORMAT_D15S1 ||
			desc.depth_format == WW3D_ZFORMAT_D16_LOCKABLE)
		{
			return width * height * 2U;
		}
		if (desc.depth_format != WW3D_ZFORMAT_UNKNOWN)
		{
			return width * height * 4U;
		}
		break;
	default:
		break;
	}

	return width * height * Get_Bytes_Per_Pixel(desc.format);
}


/*!
 * KM General base constructor for texture classes
 */
TextureBaseClass::TextureBaseClass
(
	unsigned int width,
	unsigned int height,
	enum MipCountType mip_level_count,
	enum PoolType pool,
	bool rendertarget,
	bool reducible
)
:	MipLevelCount(mip_level_count),
	BackendTexture(0),
	Initialized(false),
   Name(""),
	FullPath(""),
	texture_id(unused_texture_id++),
	IsLightmap(false),
	IsProcedural(false),
	IsReducible(reducible),
	RenderTarget(rendertarget),
	ProceduralTextureRecreationEnabled(false),
	IsCompressionAllowed(false),
	InactivationTime(0),
	ExtendedInactivationTime(0),
	LastInactivationSyncTime(0),
	LastAccessed(0),
	Width(width),
	Height(height),
	Pool(pool),
	Dirty(false),
	TextureLoadTask(nullptr),
	ThumbnailLoadTask(nullptr),
	HSVShift(0.0f,0.0f,0.0f)
{
}


//**********************************************************************************************
//! Base texture class destructor
/*! KJM
*/
TextureBaseClass::~TextureBaseClass()
{
	delete TextureLoadTask;
	TextureLoadTask=nullptr;
	delete ThumbnailLoadTask;
	ThumbnailLoadTask=nullptr;

	if (BackendTexture != 0)
	{
		WW3D::Get_Render_Backend()->Release_Texture_Handle(BackendTexture);
		BackendTexture = 0;
	}

	WW3D::Get_Render_Backend()->Unregister_Texture(this);
}

static RenderBackendTextureHandle Create_Texture(unsigned width, unsigned height, WW3DFormat format,
	MipCountType mip_levels, TextureBaseClass::PoolType pool, bool render_target)
{
	return WW3D::Get_Render_Backend()->Create_Texture_Handle_Pooled(
		width, height, format, mip_levels, static_cast<RenderBackendTexturePool>(pool), render_target);
}

static RenderBackendTextureHandle Create_Texture(RenderBackendSurface *surface, MipCountType mip_levels)
{
	return WW3D::Get_Render_Backend()->Create_Texture_From_Surface(surface, mip_levels);
}

static RenderBackendTextureHandle Create_ZTexture(unsigned width, unsigned height, WW3DZFormat format,
	MipCountType mip_levels, TextureBaseClass::PoolType pool)
{
	return WW3D::Get_Render_Backend()->Create_ZTexture_Handle_Pooled(
		width, height, format, mip_levels, static_cast<RenderBackendTexturePool>(pool));
}

static RenderBackendTextureHandle Create_Cube_Texture(unsigned width, unsigned height, WW3DFormat format,
	MipCountType mip_levels, TextureBaseClass::PoolType pool, bool render_target)
{
	return WW3D::Get_Render_Backend()->Create_Cube_Texture_Handle(
		width, height, format, mip_levels, static_cast<RenderBackendTexturePool>(pool), render_target);
}

static RenderBackendTextureHandle Create_Volume_Texture(unsigned width, unsigned height, unsigned depth,
	WW3DFormat format, MipCountType mip_levels, TextureBaseClass::PoolType pool)
{
	return WW3D::Get_Render_Backend()->Create_Volume_Texture_Handle(
		width, height, depth, format, mip_levels, static_cast<RenderBackendTexturePool>(pool));
}




//**********************************************************************************************
//! Invalidate old unused textures
/*!
*/
void TextureBaseClass::Invalidate_Old_Unused_Textures(unsigned invalidation_time_override)
{
	// Texture eviction is also required when thumbnails are disabled. GeneralsMD loads
	// full-resolution managed textures in that mode, and the DX9 backend relies on this
	// path to recover memory when a new texture cannot be allocated.
	// Zero the texture apply count in this function because this is called every frame...(this wasn't in E&B main branch KJM)
	TexturesAppliedPerFrame=0;

	unsigned synctime=WW3D::Get_Sync_Time();
	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager

	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		TextureClass* tex=ite.Peek_Value();

		// Consider invalidating if texture has been initialized and defines inactivation time
		if (tex->Initialized && tex->InactivationTime)
		{
			unsigned age=synctime-tex->LastAccessed;

			if (invalidation_time_override)
			{
				if (age>invalidation_time_override)
				{
					tex->Invalidate();
					tex->LastInactivationSyncTime=synctime;
				}
			}
			else
			{
				// Not used in the last n milliseconds?
				if (age>(tex->InactivationTime+tex->ExtendedInactivationTime))
				{
					tex->Invalidate();
					tex->LastInactivationSyncTime=synctime;
				}
			}
		}
	}
}





//**********************************************************************************************
//! Invalidate this texture
/*!
*/
void TextureBaseClass::Invalidate()
{
	if (TextureLoadTask) {
		return;
	}
	if (ThumbnailLoadTask) {
		return;
	}

	// Don't invalidate procedural textures
	if (IsProcedural) {
		return;
	}

	if (BackendTexture != 0)
	{
		WW3D::Get_Render_Backend()->Release_Texture_Handle(BackendTexture);
		BackendTexture = 0;
	}

	Initialized=false;

	LastAccessed=WW3D::Get_Sync_Time();
/*	was battlefield version// If the texture has already been initialised we should exit now
	if (Initialized) return;

	WWPROFILE(("TextureClass::Init()"));

	// If the texture has recently been inactivated, increase the inactivation time (this texture obviously
	// should not have been inactivated yet).

	if (InactivationTime && LastInactivationSyncTime) {
		if ((WW3D::Get_Sync_Time()-LastInactivationSyncTime)<InactivationTime) {
			ExtendedInactivationTime=3*InactivationTime;
		}
		LastInactivationSyncTime=0;
	}

	if (ThumbnailLoadTask)
	{
		return;
	}

	// Don't invalidate procedural textures
	if (IsProcedural)
	{
		return;
	}

	Initialized=false;

	LastAccessed=WW3D::Get_Sync_Time();*/
}

//**********************************************************************************************
//! Returns the opaque texture resource owned by the backend
/*!
*/
RenderBackendTextureHandle TextureBaseClass::Peek_Render_Backend_Texture() const
{
	LastAccessed=WW3D::Get_Sync_Time();
	return BackendTexture;
}

//**********************************************************************************************
//! Replace the backend texture resource, taking ownership of the handle.
/*!
*/
void TextureBaseClass::Set_Render_Backend_Texture(RenderBackendTextureHandle texture)
{
	LastAccessed=WW3D::Get_Sync_Time();
	if (BackendTexture == texture)
	{
		// A zero handle is the canonical released state.  Keep the engine-side
		// flag cleared even when the backend is asked to release an already
		// released resource.
		if (texture == 0)
		{
			Initialized = false;
		}
		return;
	}
	if (BackendTexture != 0)
	{
		WW3D::Get_Render_Backend()->Release_Texture_Handle(BackendTexture);
	}
	BackendTexture = texture;
	// A zero handle means that the native resource was released.  Keep the
	// engine-side initialization bit in sync with the opaque backend handle so
	// Ensure_Render_Backend_Texture() can recreate file-backed resources after a
	// device reset instead of treating the cleared object as still initialized.
	if (texture == 0)
	{
		Initialized = false;
	}
}

//**********************************************************************************************
//! Ensure the resource used by a custom render path is available.
/*
 * Most texture users call Apply(), which initializes a file-backed texture before binding it.
 * Terrain and shader code also has a legacy immediate-resource path, though, and that path can
 * bind a texture after the texture manager has evicted it. Keep the recovery decision here so
 * callers never need to know which renderer owns the resource.
 */
bool TextureBaseClass::Ensure_Render_Backend_Texture()
{
	if (BackendTexture != 0)
	{
		return true;
	}

	// A load task owns initialization while it is in flight. Starting another load here would
	// race the loader's state machine and can replace a valid pending resource.
	if (TextureLoadTask != nullptr || ThumbnailLoadTask != nullptr)
	{
		return false;
	}

	if (IsProcedural)
	{
		if (Recreate_Procedural_Texture())
		{
			Initialized = true;
		}
		return BackendTexture != 0;
	}

	if (!Initialized)
	{
		Init();
	}

	return BackendTexture != 0;
}

bool TextureBaseClass::Recreate_Procedural_Texture()
{
	return false;
}


//**********************************************************************************************
//! Load locked surface
/*!
*/
void TextureBaseClass::Load_Locked_Surface()
{
	WWPROFILE(("TextureClass::Load_Locked_Surface()"));
	Set_Render_Backend_Texture(0);
	TextureLoader::Request_Thumbnail(this);
	Initialized=false;
}


//**********************************************************************************************
//! Is missing texture
/*!
*/
bool TextureBaseClass::Is_Missing_Texture()
{
	return WW3D::Get_Render_Backend()->Is_Missing_Texture_Handle(BackendTexture);
}


//**********************************************************************************************
//! Set texture name
/*!
*/
void TextureBaseClass::Set_Texture_Name(const char * name)
{
	Name=name;
}




//**********************************************************************************************
//! Get priority
/*!
*/
unsigned int TextureBaseClass::Get_Priority()
{
	if (BackendTexture == 0)
	{
		WWASSERT_PRINT(0, "Get_Priority: texture resource is null!");
		return 0;
	}

	return WW3D::Get_Render_Backend()->Get_Texture_Priority(BackendTexture);
}


//**********************************************************************************************
//! Set priority
/*!
*/
unsigned int TextureBaseClass::Set_Priority(unsigned int priority)
{
	if (BackendTexture == 0)
	{
		WWASSERT_PRINT(0, "Set_Priority: texture resource is null!");
		return 0;
	}

	return WW3D::Get_Render_Backend()->Set_Texture_Priority(BackendTexture, priority);
}


//**********************************************************************************************
//! Get reduction mip levels
/*!
*/
unsigned TextureBaseClass::Get_Reduction() const
{
	// don't reduce if the texture is too small already or
	// has no mip map levels
	if (MipLevelCount==MIP_LEVELS_1) return 0;
	if (Width <= 32 || Height <= 32) return 0;

	int reduction=WW3D::Get_Texture_Reduction();

	// 'large texture extra reduction' causes textures above 256x256 to be reduced one more step.
	if (WW3D::Is_Large_Texture_Extra_Reduction_Enabled() && (Width > 256 || Height > 256)) {
		reduction++;
	}
	if (MipLevelCount && reduction>MipLevelCount) {
		reduction=MipLevelCount;
	}
	return reduction;
}



//**********************************************************************************************
//! Apply null texture state
/*!
*/
void TextureBaseClass::Apply_Null(unsigned int stage)
{
	// This function sets the render states for a "null" texture
	WW3D::Get_Render_Backend()->Set_Texture_Resource(stage, nullptr);
}

// ----------------------------------------------------------------------------
// Setting HSV_Shift value is always relative to the original texture. This function invalidates the
// texture surface and causes the texture to be reloaded. For thumbnailable textures, the hue shifting
// is done in the background loading thread.
// ----------------------------------------------------------------------------
void TextureBaseClass::Set_HSV_Shift(const Vector3 &hsv_shift)
{
	Invalidate();
	HSVShift=hsv_shift;
}

//**********************************************************************************************
//! Get total locked surface size
/*! KM
*/
int TextureBaseClass::_Get_Total_Locked_Surface_Size()
{
	int total_locked_surface_size=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		// Get the current texture
		TextureBaseClass* tex=ite.Peek_Value();
		if (!tex->Initialized)
		{
			total_locked_surface_size+=tex->Get_Texture_Memory_Usage();
		}
	}
	return total_locked_surface_size;
}

//**********************************************************************************************
//! Get total texture size
/*! KM
*/
int TextureBaseClass::_Get_Total_Texture_Size()
{
	int total_texture_size=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		// Get the current texture
		TextureBaseClass* tex=ite.Peek_Value();
		total_texture_size+=tex->Get_Texture_Memory_Usage();
	}
	return total_texture_size;
}

// ----------------------------------------------------------------------------


//**********************************************************************************************
//! Get total lightmap texture size
/*!
*/
int TextureBaseClass::_Get_Total_Lightmap_Texture_Size()
{
	int total_texture_size=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		// Get the current texture
		TextureBaseClass* tex=ite.Peek_Value();
		if (tex->Is_Lightmap())
		{
			total_texture_size+=tex->Get_Texture_Memory_Usage();
		}
	}
	return total_texture_size;
}


//**********************************************************************************************
//! Get total procedural texture size
/*!
*/
int TextureBaseClass::_Get_Total_Procedural_Texture_Size()
{
	int total_texture_size=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		// Get the current texture
		TextureBaseClass* tex=ite.Peek_Value();
		if (tex->Is_Procedural())
		{
			total_texture_size+=tex->Get_Texture_Memory_Usage();
		}
	}
	return total_texture_size;
}

//**********************************************************************************************
//! Get total texture count
/*!
*/
int TextureBaseClass::_Get_Total_Texture_Count()
{
	int texture_count=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		texture_count++;
	}

	return texture_count;
}

// ----------------------------------------------------------------------------


//**********************************************************************************************
//! Get total light map texture count
/*!
*/
int TextureBaseClass::_Get_Total_Lightmap_Texture_Count()
{
	int texture_count=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		if (ite.Peek_Value()->Is_Lightmap())
		{
			texture_count++;
		}
	}

	return texture_count;
}

//**********************************************************************************************
//! Get total procedural texture count
/*!
*/
int TextureBaseClass::_Get_Total_Procedural_Texture_Count()
{
	int texture_count=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		if (ite.Peek_Value()->Is_Procedural())
		{
			texture_count++;
		}
	}

	return texture_count;
}


//**********************************************************************************************
//! Get total locked surface count
/*!
*/
int TextureBaseClass::_Get_Total_Locked_Surface_Count()
{
	int texture_count=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		// Get the current texture
		TextureBaseClass* tex=ite.Peek_Value();
		if (!tex->Initialized)
		{
			texture_count++;
		}
	}

	return texture_count;
}

/*************************************************************************
**                             TextureClass
*************************************************************************/
TextureClass::TextureClass
(
	unsigned width,
	unsigned height,
	WW3DFormat format,
	MipCountType mip_level_count,
	PoolType pool,
	bool rendertarget,
	bool allow_reduction
)
:	TextureBaseClass(width, height, mip_level_count, pool, rendertarget,allow_reduction),
	Filter(mip_level_count),
	TextureFormat(format)
{
	Initialized=false;
	IsProcedural=true;
	IsReducible=false;
	Set_Procedural_Texture_Recreation_Enabled(true);

	switch (format)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	default : break;
	}

	Set_Render_Backend_Texture(Create_Texture(width, height, format, mip_level_count, pool, rendertarget));
	Initialized = Peek_Render_Backend_Texture() != 0;

	if (pool==POOL_DEFAULT)
	{
		Set_Dirty();
	}
	// DX11 has no managed-resource pool. Every procedural texture owns a
	// native resource that must be released and recreated across a device
	// reset, including procedural textures whose legacy pool was managed.
	if (IsProcedural)
	{
		WW3D::Get_Render_Backend()->Register_Texture(this, RenderBackendTextureKind::Texture2D,
			width, height, 1, format, WW3D_ZFORMAT_UNKNOWN, mip_level_count, rendertarget);
	}
	LastAccessed=WW3D::Get_Sync_Time();
}



// ----------------------------------------------------------------------------
TextureClass::TextureClass
(
	const char *name,
	const char *full_path,
	MipCountType mip_level_count,
	WW3DFormat texture_format,
	bool allow_compression,
	bool allow_reduction
)
:	TextureBaseClass(0, 0, mip_level_count),
	Filter(mip_level_count),
	TextureFormat(texture_format)
{
	IsCompressionAllowed=allow_compression;
	InactivationTime=DEFAULT_INACTIVATION_TIME;		// Default inactivation time 30 seconds
	IsReducible=allow_reduction;

	switch (TextureFormat)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	case WW3D_FORMAT_U8V8:		// Bumpmap
	case WW3D_FORMAT_L6V5U5:	// Bumpmap
	case WW3D_FORMAT_X8L8V8U8:	// Bumpmap
		// If requesting bumpmap format that isn't available we'll just return the surface in whatever color
		// format the texture file is in. (This is illegal case, the format support should always be queried
		// before creating a bump texture!)
		if (!WW3D::Is_Initted() || !WW3D::Get_Render_Backend()->Supports_Texture_Format(TextureFormat))
		{
			TextureFormat=WW3D_FORMAT_UNKNOWN;
		}
		// If bump format is valid, make sure compression is not allowed so that we don't even attempt to load
		// from a compressed file (quality isn't good enough for bump map). Also disable mipmapping.
		else
		{
			IsCompressionAllowed=false;
			MipLevelCount=MIP_LEVELS_1;
			Filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
		}
		break;
	default:	break;
	}

	WWASSERT_PRINT(name && name[0], "TextureClass CTor: null or empty texture name");
	int len=strlen(name);
	for (int i=0;i<len;++i)
	{
		if (name[i]=='+')
		{
			IsLightmap=true;

			// Set bilinear filtering for lightmaps (they are very stretched and
			// low detail so we don't care for anisotropic or trilinear filtering...)
			Filter.Set_Min_Filter(TextureFilterClass::FILTER_TYPE_FAST);
			Filter.Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_FAST);
			if (mip_level_count!=MIP_LEVELS_1) Filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_FAST);
			break;
		}
	}
	Set_Texture_Name(name);
	Set_Full_Path(full_path);
	WWASSERT(name[0]!='\0');
	if (!WW3D::Is_Texturing_Enabled())
	{
		Initialized=true;
		Set_Render_Backend_Texture(0);
	}

	// Find original size from the thumbnail (but don't create thumbnail texture yet!)
	ThumbnailClass* thumb=ThumbnailManagerClass::Peek_Thumbnail_Instance_From_Any_Manager(Get_Full_Path());
	if (thumb)
	{
		Width=thumb->Get_Original_Texture_Width();
		Height=thumb->Get_Original_Texture_Height();
 		if (MipLevelCount!=MIP_LEVELS_1) {
 			MipLevelCount=(MipCountType)thumb->Get_Original_Texture_Mip_Level_Count();
 		}
	}

	LastAccessed=WW3D::Get_Sync_Time();

	// If the thumbnails are not enabled, init the texture at this point to avoid stalling when the
	// mesh is rendered.
	if (!WW3D::Get_Thumbnail_Enabled())
	{
		if (TextureLoader::Is_Render_Thread())
		{
			Init();
		}
	}
}

// ----------------------------------------------------------------------------
TextureClass::TextureClass
(
	SurfaceClass *surface,
	MipCountType mip_level_count
)
:  TextureBaseClass(0,0,mip_level_count),
	Filter(mip_level_count),
	TextureFormat(surface->Get_Surface_Format())
{
	IsProcedural=true;
	Initialized=false;
	IsReducible=false;

	SurfaceClass::SurfaceDescription sd;
	surface->Get_Description(sd);
	Width=sd.Width;
	Height=sd.Height;
	switch (sd.Format)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	default: break;
	}

	Set_Render_Backend_Texture(Create_Texture(surface->Get_Render_Backend_Surface(), mip_level_count));
	Initialized = Peek_Render_Backend_Texture() != 0;
	LastAccessed=WW3D::Get_Sync_Time();
}

// ----------------------------------------------------------------------------
TextureClass::TextureClass(RenderBackendTextureHandle texture)
:	TextureBaseClass
	(
		0,
		0,
		static_cast<MipCountType>(WW3D::Get_Render_Backend()->Get_Texture_Level_Count(texture))
	),
	Filter(static_cast<MipCountType>(WW3D::Get_Render_Backend()->Get_Texture_Level_Count(texture))),
	TextureFormat(WW3D_FORMAT_UNKNOWN)
{
	Initialized=false;
	IsProcedural=true;
	IsReducible=false;

	IRenderBackend *backend = WW3D::Get_Render_Backend();
	Set_Render_Backend_Texture(texture != 0 ? backend->Add_Texture_Reference(texture) : 0);
	Initialized = Peek_Render_Backend_Texture() != 0;
	RenderBackendTextureDescription description;
	if (Peek_Render_Backend_Texture() != 0 && backend->Get_Texture_Description(Peek_Render_Backend_Texture(), 0, description))
	{
		Width=static_cast<int>(description.width);
		Height=static_cast<int>(description.height);
		TextureFormat=description.format;
	}
	switch (TextureFormat)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	default: break;
	}

	LastAccessed=WW3D::Get_Sync_Time();
}

//**********************************************************************************************
//! Initialise the texture
/*!
*/
bool TextureClass::Recreate_Procedural_Texture()
{
	if (!ProceduralTextureRecreationEnabled || Width <= 0 || Height <= 0 ||
		TextureFormat == WW3D_FORMAT_UNKNOWN)
	{
		return false;
	}

	Set_Render_Backend_Texture(Create_Texture(
		static_cast<unsigned>(Width),
		static_cast<unsigned>(Height),
		TextureFormat,
		MipLevelCount,
		Get_Pool(),
		RenderTarget));

	return Peek_Render_Backend_Texture() != 0;
}

void TextureClass::Init()
{
	if (IsProcedural)
	{
		if (!Peek_Render_Backend_Texture())
		{
			Initialized = Recreate_Procedural_Texture();
		}
		else
		{
			Initialized = true;
		}

		LastAccessed=WW3D::Get_Sync_Time();
		return;
	}

	// If the texture has already been initialised we should exit now
	if (Initialized) return;

	WWPROFILE("TextureClass::Init");

	// If the texture has recently been inactivated, increase the inactivation time (this texture obviously
	// should not have been inactivated yet).
	if (InactivationTime && LastInactivationSyncTime)
	{
		if ((WW3D::Get_Sync_Time()-LastInactivationSyncTime)<InactivationTime)
		{
			ExtendedInactivationTime=3*InactivationTime;
		}
		LastInactivationSyncTime=0;
	}


	if (!Peek_Render_Backend_Texture())
	{
		if (!WW3D::Get_Thumbnail_Enabled() || MipLevelCount==MIP_LEVELS_1)
		{
//		if (MipLevelCount==MIP_LEVELS_1) {
			TextureLoader::Request_Foreground_Loading(this);
		}
		else
		{
			WW3DFormat format=TextureFormat;
			Load_Locked_Surface();
			TextureFormat=format;
		}
	}

	if (!Initialized)
	{
		TextureLoader::Request_Background_Loading(this);
	}

	LastAccessed=WW3D::Get_Sync_Time();
}

//**********************************************************************************************
//! Apply new surface to texture
/*!
*/
void TextureClass::Apply_New_Surface
(
	RenderBackendTextureHandle texture,
	bool initialized,
	bool disable_auto_invalidation
)
{
	Set_Render_Backend_Texture(texture);

	if (initialized) Initialized=true;
	if (disable_auto_invalidation) InactivationTime = 0;

	WWASSERT(Peek_Render_Backend_Texture() != 0);
	RenderBackendTextureDescription description;
	if (initialized && WW3D::Get_Render_Backend()->Get_Texture_Description(
		Peek_Render_Backend_Texture(), 0, description))
	{
		TextureFormat=description.format;
		Width=static_cast<int>(description.width);
		Height=static_cast<int>(description.height);
	}
}


//**********************************************************************************************
//! Apply texture states
/*!
*/
void TextureClass::Apply(unsigned int stage)
{
	// Initialization needs to be done when texture is used if it hasn't been done before.
	// XBOX always initializes textures at creation time.
	if (!Initialized)
	{
		Init();

		/* was in battlefield// Non-thumbnailed textures are always initialized when used
		if (MipLevelCount==MIP_LEVELS_1)
		{
		}
		// Thumbnailed textures have delayed initialization and a background loading system
		else
		{
			// Limit the number of texture initializations per frame to reduce stuttering
			if (TexturesAppliedPerFrame<MAX_TEXTURES_APPLIED_PER_FRAME)
			{
				TexturesAppliedPerFrame++;
				Init();
			}
			else
			{
				// If texture can't be initialized in this frame, at least make sure we have the thumbnail.
				if (!Peek_Texture())
				{
					WW3DFormat format=TextureFormat;
					Load_Locked_Surface();
					TextureFormat=format;
				}
			}
		}*/
	}
	LastAccessed=WW3D::Get_Sync_Time();

	// Set texture itself
	if (WW3D::Is_Texturing_Enabled())
	{
		WW3D::Get_Render_Backend()->Set_Texture_Resource(stage, this);
	}
	else
	{
		WW3D::Get_Render_Backend()->Set_Texture_Resource(stage, nullptr);
	}

	Filter.Apply(stage);
}

//**********************************************************************************************
//! Get surface from mip level
/*!
*/
SurfaceClass *TextureClass::Get_Surface_Level(unsigned int level)
{
	if (!Ensure_Render_Backend_Texture())
	{
		return nullptr;
	}

	const RenderBackendTextureHandle texture = Peek_Render_Backend_Texture();
	if (texture == 0)
	{
		return nullptr;
	}

	return WW3D::Get_Render_Backend()->Get_Texture_Surface_Level(texture, level);
}

//**********************************************************************************************
//! Get surface description for a mip level
/*!
*/
void TextureClass::Get_Level_Description( SurfaceClass::SurfaceDescription & desc, unsigned int level )
{
	SurfaceClass * surf = Get_Surface_Level(level);
	if (surf != nullptr) {
		surf->Get_Description(desc);
	}
	REF_PTR_RELEASE(surf);
}

//**********************************************************************************************
//! Get texture memory usage
/*!
*/
unsigned TextureClass::Get_Texture_Memory_Usage() const
{
	unsigned size=0;
	if (Peek_Render_Backend_Texture() == 0) return 0;
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	for (unsigned i=0;i<backend->Get_Texture_Level_Count(Peek_Render_Backend_Texture());++i)
	{
		RenderBackendTextureDescription description;
		if (backend->Get_Texture_Description(Peek_Render_Backend_Texture(), i, description))
		{
			size += Compute_Surface_Size_Bytes(description);
		}
	}
	return size;
}


// Utility functions
TextureClass* Load_Texture(ChunkLoadClass & cload)
{
	// Assume failure
	TextureClass *newtex = nullptr;

	char name[256];
	if (cload.Open_Chunk () && (cload.Cur_Chunk_ID () == W3D_CHUNK_TEXTURE))
	{

		W3dTextureInfoStruct texinfo;
		bool hastexinfo = false;

		/*
		** Read in the texture filename, and a possible texture info structure.
		*/
		while (cload.Open_Chunk()) {
			switch (cload.Cur_Chunk_ID()) {
				case W3D_CHUNK_TEXTURE_NAME:
					cload.Read(&name,cload.Cur_Chunk_Length());
					break;

				case W3D_CHUNK_TEXTURE_INFO:
					cload.Read(&texinfo,sizeof(W3dTextureInfoStruct));
					hastexinfo = true;
					break;
			};
			cload.Close_Chunk();
		}
		cload.Close_Chunk();

		/*
		** Get the texture from the asset manager
		*/
		if (hastexinfo)
		{

			MipCountType mipcount;

			bool no_lod = ((texinfo.Attributes & W3DTEXTURE_NO_LOD) == W3DTEXTURE_NO_LOD);

			if (no_lod)
			{
				mipcount = MIP_LEVELS_1;
			}
			else
			{
				switch (texinfo.Attributes & W3DTEXTURE_MIP_LEVELS_MASK) {

					case W3DTEXTURE_MIP_LEVELS_ALL:
						mipcount = MIP_LEVELS_ALL;
						break;

					case W3DTEXTURE_MIP_LEVELS_2:
						mipcount = MIP_LEVELS_2;
						break;

					case W3DTEXTURE_MIP_LEVELS_3:
						mipcount = MIP_LEVELS_3;
						break;

					case W3DTEXTURE_MIP_LEVELS_4:
						mipcount = MIP_LEVELS_4;
						break;

					default:
						WWASSERT (false);
						mipcount = MIP_LEVELS_ALL;
						break;
				}
			}

			WW3DFormat format=WW3D_FORMAT_UNKNOWN;

			switch (texinfo.Attributes & W3DTEXTURE_TYPE_MASK)
			{

				case W3DTEXTURE_TYPE_COLORMAP:
					// Do nothing.
					break;

				case W3DTEXTURE_TYPE_BUMPMAP:
				{
					if (WW3D::Is_Initted() && WW3D::Get_Render_Backend()->Supports_Bump_Envmap())
					{
						// No mipmaps to bumpmap for now
						mipcount=MIP_LEVELS_1;

						if (WW3D::Get_Render_Backend()->Supports_Texture_Format(WW3D_FORMAT_U8V8)) format=WW3D_FORMAT_U8V8;
						else if (WW3D::Get_Render_Backend()->Supports_Texture_Format(WW3D_FORMAT_X8L8V8U8)) format=WW3D_FORMAT_X8L8V8U8;
						else if (WW3D::Get_Render_Backend()->Supports_Texture_Format(WW3D_FORMAT_L6V5U5)) format=WW3D_FORMAT_L6V5U5;
					}
					break;
				}

				default:
					WWASSERT (false);
					break;
			}

			newtex = WW3DAssetManager::Get_Instance()->Get_Texture (name, mipcount, format);

			if (no_lod)
			{
				newtex->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
			}
			bool u_clamp = ((texinfo.Attributes & W3DTEXTURE_CLAMP_U) != 0);
			newtex->Get_Filter().Set_U_Addr_Mode(u_clamp ? TextureFilterClass::TEXTURE_ADDRESS_CLAMP : TextureFilterClass::TEXTURE_ADDRESS_REPEAT);
			bool v_clamp = ((texinfo.Attributes & W3DTEXTURE_CLAMP_V) != 0);
			newtex->Get_Filter().Set_V_Addr_Mode(v_clamp ? TextureFilterClass::TEXTURE_ADDRESS_CLAMP : TextureFilterClass::TEXTURE_ADDRESS_REPEAT);

		} else
		{
			newtex = WW3DAssetManager::Get_Instance()->Get_Texture(name);
		}

		WWASSERT(newtex);
	}

	// Return a pointer to the new texture
	return newtex;
}

// Utility function used by Save_Texture
void setup_texture_attributes(TextureClass * tex, W3dTextureInfoStruct * texinfo)
{
	texinfo->Attributes = 0;

	if (tex->Get_Filter().Get_Mip_Mapping() == TextureFilterClass::FILTER_TYPE_NONE) texinfo->Attributes |= W3DTEXTURE_NO_LOD;
	if (tex->Get_Filter().Get_U_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_CLAMP) texinfo->Attributes |= W3DTEXTURE_CLAMP_U;
	if (tex->Get_Filter().Get_V_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_CLAMP) texinfo->Attributes |= W3DTEXTURE_CLAMP_V;
}


void Save_Texture(TextureClass * texture,ChunkSaveClass & csave)
{
	const char * filename;
	W3dTextureInfoStruct texinfo;
	memset(&texinfo,0,sizeof(texinfo));

	filename = texture->Get_Full_Path();

	setup_texture_attributes(texture, &texinfo);

	csave.Begin_Chunk(W3D_CHUNK_TEXTURE_NAME);
	csave.Write(filename,strlen(filename)+1);
	csave.End_Chunk();

	if ((texinfo.Attributes != 0) || (texinfo.AnimType != 0) || (texinfo.FrameCount != 0)) {
		csave.Begin_Chunk(W3D_CHUNK_TEXTURE_INFO);
		csave.Write(&texinfo, sizeof(texinfo));
		csave.End_Chunk();
	}
}


/*!
 *	KJM depth stencil texture constructor
 */
ZTextureClass::ZTextureClass
(
	unsigned width,
	unsigned height,
	WW3DZFormat zformat,
	MipCountType mip_level_count,
	PoolType pool
)
:	TextureBaseClass(width,height, mip_level_count, pool),
	DepthStencilTextureFormat(zformat)
{
	Set_Render_Backend_Texture(Create_ZTexture(width, height, zformat, mip_level_count, pool));
	Initialized = Peek_Render_Backend_Texture() != 0;

	if (pool==POOL_DEFAULT)
	{
		Set_Dirty();
		WW3D::Get_Render_Backend()->Register_Texture(this, RenderBackendTextureKind::DepthStencil,
			width, height, 1, WW3D_FORMAT_UNKNOWN, zformat, mip_level_count, false);
	}
	Initialized = Peek_Render_Backend_Texture() != 0;
	IsProcedural=true;
	IsReducible=false;
	Set_Procedural_Texture_Recreation_Enabled(true);

	LastAccessed=WW3D::Get_Sync_Time();
}

bool ZTextureClass::Recreate_Procedural_Texture()
{
	if (!ProceduralTextureRecreationEnabled || Width <= 0 || Height <= 0 ||
		DepthStencilTextureFormat == WW3D_ZFORMAT_UNKNOWN)
	{
		return false;
	}

	Set_Render_Backend_Texture(Create_ZTexture(
		static_cast<unsigned>(Width),
		static_cast<unsigned>(Height),
		DepthStencilTextureFormat,
		MipLevelCount,
		Get_Pool()));

	return Peek_Render_Backend_Texture() != 0;
}


//**********************************************************************************************
//! Apply depth stencil texture
/*! KM
*/
void ZTextureClass::Apply(unsigned int stage)
{
	WW3D::Get_Render_Backend()->Set_Texture_Resource(stage, this);
}

//**********************************************************************************************
//! Apply new surface to texture
/*! KM
*/
void ZTextureClass::Apply_New_Surface
(
	RenderBackendTextureHandle texture,
	bool initialized,
	bool disable_auto_invalidation
)
{
	Set_Render_Backend_Texture(texture);

	if (initialized) Initialized=true;
	if (disable_auto_invalidation) InactivationTime = 0;

	WWASSERT(Peek_Render_Backend_Texture() != 0);
	RenderBackendTextureDescription description;
	if (initialized && WW3D::Get_Render_Backend()->Get_Texture_Description(
		Peek_Render_Backend_Texture(), 0, description))
	{
		DepthStencilTextureFormat=description.depth_format;
		Width=static_cast<int>(description.width);
		Height=static_cast<int>(description.height);
	}
}

//**********************************************************************************************
//! Get surface from mip level
/*!
*/
SurfaceClass *ZTextureClass::Get_Surface_Level(unsigned int level)
{
	if (!Ensure_Render_Backend_Texture())
	{
		return nullptr;
	}

	const RenderBackendTextureHandle texture = Peek_Render_Backend_Texture();
	if (texture == 0)
	{
		return nullptr;
	}

	return WW3D::Get_Render_Backend()->Get_Texture_Surface_Level(texture, level);
}

//**********************************************************************************************
//! Get texture memory usage
/*!
*/
unsigned ZTextureClass::Get_Texture_Memory_Usage() const
{
	unsigned size=0;
	if (Peek_Render_Backend_Texture() == 0) return 0;
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	for (unsigned i=0;i<backend->Get_Texture_Level_Count(Peek_Render_Backend_Texture());++i)
	{
		RenderBackendTextureDescription description;
		if (backend->Get_Texture_Description(Peek_Render_Backend_Texture(), i, description))
		{
			size += Compute_Surface_Size_Bytes(description);
		}
	}
	return size;
}



/*************************************************************************
**                             CubeTextureClass
*************************************************************************/
CubeTextureClass::CubeTextureClass
(
	unsigned width,
	unsigned height,
	WW3DFormat format,
	MipCountType mip_level_count,
	PoolType pool,
	bool rendertarget,
	bool allow_reduction
)
: TextureClass(width, height, mip_level_count, pool, rendertarget, format, allow_reduction)
{
	IsProcedural=true;
	IsReducible=false;
	Set_Procedural_Texture_Recreation_Enabled(true);

	switch (format)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	default : break;
	}

	Set_Render_Backend_Texture(Create_Cube_Texture(width, height, format, mip_level_count,
		pool, rendertarget));
	Initialized = Peek_Render_Backend_Texture() != 0;

	if (pool==POOL_DEFAULT)
	{
		Set_Dirty();
		WW3D::Get_Render_Backend()->Register_Texture(this, RenderBackendTextureKind::Cube,
			width, height, 1, format, WW3D_ZFORMAT_UNKNOWN, mip_level_count, rendertarget);
	}
	LastAccessed=WW3D::Get_Sync_Time();
}


bool CubeTextureClass::Recreate_Procedural_Texture()
{
	if (!ProceduralTextureRecreationEnabled || Width <= 0 || Height <= 0 ||
		TextureFormat == WW3D_FORMAT_UNKNOWN)
	{
		return false;
	}

	Set_Render_Backend_Texture(Create_Cube_Texture(
		static_cast<unsigned>(Width),
		static_cast<unsigned>(Height),
		TextureFormat,
		MipLevelCount,
		Get_Pool(),
		RenderTarget));

	return Peek_Render_Backend_Texture() != 0;
}



// ----------------------------------------------------------------------------
CubeTextureClass::CubeTextureClass
(
	const char *name,
	const char *full_path,
	MipCountType mip_level_count,
	WW3DFormat texture_format,
	bool allow_compression,
	bool allow_reduction
)
:	TextureClass(0,0,mip_level_count, POOL_MANAGED, false, texture_format)
{
	IsCompressionAllowed=allow_compression;
	InactivationTime=DEFAULT_INACTIVATION_TIME;		// Default inactivation time 30 seconds

	switch (TextureFormat)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	case WW3D_FORMAT_U8V8:		// Bumpmap
	case WW3D_FORMAT_L6V5U5:	// Bumpmap
	case WW3D_FORMAT_X8L8V8U8:	// Bumpmap
		// If requesting bumpmap format that isn't available we'll just return the surface in whatever color
		// format the texture file is in. (This is illegal case, the format support should always be queried
		// before creating a bump texture!)
		if (!WW3D::Is_Initted() || !WW3D::Get_Render_Backend()->Supports_Texture_Format(TextureFormat))
		{
			TextureFormat=WW3D_FORMAT_UNKNOWN;
		}
		// If bump format is valid, make sure compression is not allowed so that we don't even attempt to load
		// from a compressed file (quality isn't good enough for bump map). Also disable mipmapping.
		else
		{
			IsCompressionAllowed=false;
			MipLevelCount=MIP_LEVELS_1;
			Filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
		}
		break;
	default:	break;
	}

	WWASSERT_PRINT(name && name[0], "TextureClass CTor: null or empty texture name");
	int len=strlen(name);
	for (int i=0;i<len;++i)
	{
		if (name[i]=='+')
		{
			IsLightmap=true;

			// Set bilinear filtering for lightmaps (they are very stretched and
			// low detail so we don't care for anisotropic or trilinear filtering...)
			Filter.Set_Min_Filter(TextureFilterClass::FILTER_TYPE_FAST);
			Filter.Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_FAST);
			if (mip_level_count!=MIP_LEVELS_1) Filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_FAST);
			break;
		}
	}
	Set_Texture_Name(name);
	Set_Full_Path(full_path);
	WWASSERT(name[0]!='\0');
	if (!WW3D::Is_Texturing_Enabled())
	{
		Initialized=true;
		Set_Render_Backend_Texture(0);
	}

	// Find original size from the thumbnail (but don't create thumbnail texture yet!)
	ThumbnailClass* thumb=ThumbnailManagerClass::Peek_Thumbnail_Instance_From_Any_Manager(Get_Full_Path());
	if (thumb)
	{
		Width=thumb->Get_Original_Texture_Width();
		Height=thumb->Get_Original_Texture_Height();
 		if (MipLevelCount!=MIP_LEVELS_1) {
 			MipLevelCount=(MipCountType)thumb->Get_Original_Texture_Mip_Level_Count();
 		}
	}

	LastAccessed=WW3D::Get_Sync_Time();

	// If the thumbnails are not enabled, init the texture at this point to avoid stalling when the
	// mesh is rendered.
	if (!WW3D::Get_Thumbnail_Enabled())
	{
		if (TextureLoader::Is_Render_Thread())
		{
			Init();
		}
	}
}

//**********************************************************************************************
//! Apply new surface to texture
/*!
*/
void CubeTextureClass::Apply_New_Surface
(
	RenderBackendTextureHandle texture,
	bool initialized,
	bool disable_auto_invalidation
)
{
	Set_Render_Backend_Texture(texture);

	if (initialized) Initialized=true;
	if (disable_auto_invalidation) InactivationTime = 0;

	WWASSERT(Peek_Render_Backend_Texture() != 0);
	RenderBackendTextureDescription description;
	if (initialized && WW3D::Get_Render_Backend()->Get_Texture_Description(
		Peek_Render_Backend_Texture(), 0, description))
	{
		TextureFormat=description.format;
		Width=static_cast<int>(description.width);
		Height=static_cast<int>(description.height);
	}
}


/*************************************************************************
**                             VolumeTextureClass
*************************************************************************/
VolumeTextureClass::VolumeTextureClass
(
	unsigned width,
	unsigned height,
	unsigned depth,
	WW3DFormat format,
	MipCountType mip_level_count,
	PoolType pool,
	bool rendertarget,
	bool allow_reduction
)
: TextureClass(width, height, mip_level_count, pool, rendertarget, format, allow_reduction),
  Depth(depth)
{
	IsProcedural=true;
	IsReducible=false;
	Set_Procedural_Texture_Recreation_Enabled(true);

	switch (format)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	default : break;
	}

	Set_Render_Backend_Texture(Create_Volume_Texture(width, height, depth, format,
		mip_level_count, pool));
	Initialized = Peek_Render_Backend_Texture() != 0;

	if (pool==POOL_DEFAULT)
	{
		Set_Dirty();
		WW3D::Get_Render_Backend()->Register_Texture(this, RenderBackendTextureKind::Volume,
			width, height, depth, format, WW3D_ZFORMAT_UNKNOWN, mip_level_count, rendertarget);
	}
	LastAccessed=WW3D::Get_Sync_Time();
}


bool VolumeTextureClass::Recreate_Procedural_Texture()
{
	if (!ProceduralTextureRecreationEnabled || Width <= 0 || Height <= 0 ||
		Depth <= 0 || TextureFormat == WW3D_FORMAT_UNKNOWN)
	{
		return false;
	}

	Set_Render_Backend_Texture(Create_Volume_Texture(
		static_cast<unsigned>(Width),
		static_cast<unsigned>(Height),
		static_cast<unsigned>(Depth),
		TextureFormat,
		MipLevelCount,
		Get_Pool()));

	return Peek_Render_Backend_Texture() != 0;
}



// ----------------------------------------------------------------------------
VolumeTextureClass::VolumeTextureClass
(
	const char *name,
	const char *full_path,
	MipCountType mip_level_count,
	WW3DFormat texture_format,
	bool allow_compression,
	bool allow_reduction
)
:	TextureClass(0,0,mip_level_count, POOL_MANAGED, false, texture_format),
	Depth(0)
{
	IsCompressionAllowed=allow_compression;
	InactivationTime=DEFAULT_INACTIVATION_TIME;		// Default inactivation time 30 seconds

	switch (TextureFormat)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	case WW3D_FORMAT_U8V8:		// Bumpmap
	case WW3D_FORMAT_L6V5U5:	// Bumpmap
	case WW3D_FORMAT_X8L8V8U8:	// Bumpmap
		// If requesting bumpmap format that isn't available we'll just return the surface in whatever color
		// format the texture file is in. (This is illegal case, the format support should always be queried
		// before creating a bump texture!)
		if (!WW3D::Is_Initted() || !WW3D::Get_Render_Backend()->Supports_Texture_Format(TextureFormat))
		{
			TextureFormat=WW3D_FORMAT_UNKNOWN;
		}
		// If bump format is valid, make sure compression is not allowed so that we don't even attempt to load
		// from a compressed file (quality isn't good enough for bump map). Also disable mipmapping.
		else
		{
			IsCompressionAllowed=false;
			MipLevelCount=MIP_LEVELS_1;
			Filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
		}
		break;
	default:	break;
	}

	WWASSERT_PRINT(name && name[0], "TextureClass CTor: null or empty texture name");
	int len=strlen(name);
	for (int i=0;i<len;++i)
	{
		if (name[i]=='+')
		{
			IsLightmap=true;

			// Set bilinear filtering for lightmaps (they are very stretched and
			// low detail so we don't care for anisotropic or trilinear filtering...)
			Filter.Set_Min_Filter(TextureFilterClass::FILTER_TYPE_FAST);
			Filter.Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_FAST);
			if (mip_level_count!=MIP_LEVELS_1) Filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_FAST);
			break;
		}
	}
	Set_Texture_Name(name);
	Set_Full_Path(full_path);
	WWASSERT(name[0]!='\0');
	if (!WW3D::Is_Texturing_Enabled())
	{
		Initialized=true;
		Set_Render_Backend_Texture(0);
	}

	// Find original size from the thumbnail (but don't create thumbnail texture yet!)
	ThumbnailClass* thumb=ThumbnailManagerClass::Peek_Thumbnail_Instance_From_Any_Manager(Get_Full_Path());
	if (thumb)
	{
		Width=thumb->Get_Original_Texture_Width();
		Height=thumb->Get_Original_Texture_Height();
 		if (MipLevelCount!=MIP_LEVELS_1) {
 			MipLevelCount=(MipCountType)thumb->Get_Original_Texture_Mip_Level_Count();
 		}
	}

	LastAccessed=WW3D::Get_Sync_Time();

	// If the thumbnails are not enabled, init the texture at this point to avoid stalling when the
	// mesh is rendered.
	if (!WW3D::Get_Thumbnail_Enabled())
	{
		if (TextureLoader::Is_Render_Thread())
		{
			Init();
		}
	}
}

//**********************************************************************************************
//! Apply new surface to texture
/*!
*/
void VolumeTextureClass::Apply_New_Surface
(
	RenderBackendTextureHandle texture,
	bool initialized,
	bool disable_auto_invalidation
)
{
	Set_Render_Backend_Texture(texture);

	if (initialized) Initialized=true;
	if (disable_auto_invalidation) InactivationTime = 0;

	WWASSERT(Peek_Render_Backend_Texture() != 0);
	RenderBackendTextureDescription description;
	if (initialized && WW3D::Get_Render_Backend()->Get_Texture_Description(
		Peek_Render_Backend_Texture(), 0, description))
	{
		TextureFormat=description.format;
		Width=static_cast<int>(description.width);
		Height=static_cast<int>(description.height);
		Depth=static_cast<int>(description.depth);
	}
}
