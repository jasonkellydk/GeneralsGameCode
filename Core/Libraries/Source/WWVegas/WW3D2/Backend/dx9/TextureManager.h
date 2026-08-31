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
 *                 Project Name : DX9 Texture Manager                                          *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/dx9texman.h                            $*
 *                                                                                             *
 *              Original Author:: Hector Yee                                                   *
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 06/27/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 4                                                           $*
 *                                                                                             *
 * 06/27/02 KM Texture class abstraction																			*
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "WWLib/always.h"
#include "Texture.h"
#include "WW3D.h"
#include "WW3DFormat.h"
#include "List.h"
#include "WWLib/multilist.h"

class DX9TextureManagerClass;

class TextureTrackerClass : public MultiListObjectClass
{
public:
	TextureTrackerClass
	(
		unsigned int w,
		unsigned int h,
		MipCountType count,
		TextureBaseClass *tex
	)
	: Width(w),
	  Height(h),
	  Mip_level_count(count),
	  Texture(tex)
	{
	}

	virtual void Recreate() const =0;

	void Release()
	{
		Texture->Set_Render_Backend_Texture(0);
	}

	TextureBaseClass* Get_Texture() const { return Texture; }


protected:

	unsigned int Width;
	unsigned int Height;
	MipCountType Mip_level_count;
	TextureBaseClass *Texture;
};

class DX9TextureTrackerClass : public TextureTrackerClass
{
public:
	DX9TextureTrackerClass
	(
		unsigned int w,
		unsigned int h,
		unsigned int d,
		WW3DFormat format,
		RenderBackendTextureKind kind,
		MipCountType count,
		TextureBaseClass *tex,
		bool rt
	)
	: TextureTrackerClass(w,h,count,tex), Depth(d), Format(format), Kind(kind), RenderTarget(rt)
	{
	}

	virtual void Recreate() const override
	{
		WWASSERT(Texture->Peek_Render_Backend_Texture() == 0);

		// Let the texture object recreate its resource through the render-backend
		// abstraction.  This is important for procedural textures such as the
		// terrain atlas: allocating a new resource is not enough, because the
		// atlas contents have to be repopulated from the height map as well.
		// It also preserves the original resource kind and pool for every texture
		// type instead of silently turning default-pool textures into managed ones.
		if (!Texture->Ensure_Render_Backend_Texture() &&
			Texture->Peek_Render_Backend_Texture() == 0 && !Texture->Is_Procedural())
		{
			// A backend-owned default-pool texture which is not procedurally
			// recreatable still needs its resource restored after Reset().
			// This is only the allocation fallback; content-owning procedural
			// textures must never be replaced with an unpopulated surface here.
			IRenderBackend *backend = WW3D::Get_Render_Backend();
			RenderBackendTextureHandle resource = 0;
			switch (Kind)
			{
			case RenderBackendTextureKind::Texture2D:
				resource = backend->Create_Texture_Handle_Pooled(
					Width, Height, Format, Mip_level_count,
					RenderBackendTexturePool::Default, RenderTarget);
				break;
			case RenderBackendTextureKind::Cube:
				resource = backend->Create_Cube_Texture_Handle(
					Width, Height, Format, Mip_level_count,
					RenderBackendTexturePool::Default, RenderTarget);
				break;
			case RenderBackendTextureKind::Volume:
				resource = backend->Create_Volume_Texture_Handle(
					Width, Height, Depth, Format, Mip_level_count,
					RenderBackendTexturePool::Default);
				break;
			default:
				break;
			}
			Texture->Set_Render_Backend_Texture(resource);
		}
	}

private:
	unsigned int Depth;
	WW3DFormat Format;
	RenderBackendTextureKind Kind;
	bool RenderTarget;
};

class DX9ZTextureTrackerClass : public TextureTrackerClass
{
public:
	DX9ZTextureTrackerClass
	(
		unsigned int w,
		unsigned int h,
		WW3DZFormat zformat,
		MipCountType count,
		TextureBaseClass* tex
	)
	: TextureTrackerClass(w,h,count,tex), ZFormat(zformat)
	{
	}

	virtual void Recreate() const override
	{
		WWASSERT(Texture->Peek_Render_Backend_Texture() == 0);
		Texture->Ensure_Render_Backend_Texture();
	}


private:
	WW3DZFormat ZFormat;
};


class DX9TextureManagerClass
{
public:
	static void Shutdown();
	static void Add(TextureTrackerClass *track);
	static void Register(TextureBaseClass *texture, RenderBackendTextureKind kind,
		unsigned width, unsigned height, unsigned depth, WW3DFormat format,
		WW3DZFormat depth_format, MipCountType mip_levels, bool render_target);
	static void Remove(TextureBaseClass *tex);
	static void Release_Textures();
	static void Recreate_Textures();
private:
	static TextureTrackerList Managed_Textures;
};
