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
 *                 Project Name : ww3d                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/meshlist.h                              $*
 *                                                                                             *
 *              Original Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 06/27/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 5                                                           $*
 *                                                                                             *
 * 06/27/02 KM Texture class abstraction																			*
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "WWLib/always.h"
#include "WWLib/multilist.h"


/*
** Here we're just typedefing some multi-lists so we don't have to write the
** long template names.
*/
class MeshTextureCategoryClass;
typedef MultiListClass<MeshTextureCategoryClass>			TextureCategoryList;
typedef MultiListIterator<MeshTextureCategoryClass>		TextureCategoryListIterator;

class MeshFVFCategoryContainer;
typedef MultiListClass<MeshFVFCategoryContainer>			FVFCategoryList;
typedef MultiListIterator<MeshFVFCategoryContainer>		FVFCategoryListIterator;

class MeshPolygonRendererClass;
typedef MultiListClass<MeshPolygonRendererClass>			MeshPolygonRendererList;
typedef MultiListIterator<MeshPolygonRendererClass>		MeshPolygonRendererListIterator;

class TextureTrackerClass;
typedef MultiListClass<TextureTrackerClass>				TextureTrackerList;
typedef MultiListIterator<TextureTrackerClass>			TextureTrackerListIterator;
