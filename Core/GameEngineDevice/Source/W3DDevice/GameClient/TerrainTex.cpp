#include "WW3D2/WW3D.h"
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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: TerrainTex.cpp ////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:   RTS3
//
// File name: TerrainTex.cpp
//
// Created:   John Ahlquist, April 2001
//
// Desc:      TextureClass overrides to perform custom texturing for the terrain.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//         Includes
//-----------------------------------------------------------------------------
#include <stdlib.h>

#include "W3DDevice/GameClient/TerrainTex.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
#include "W3DDevice/GameClient/TileData.h"
#include "Common/GlobalData.h"
#include "WW3D2/Backend/IRenderBackend.h"

/******************************************************************************
						TerrainTextureClass
******************************************************************************/
//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

//=============================================================================
// TerrainTextureClass::TerrainTextureClass
//=============================================================================
/** Constructor. Calls parent constructor to create a 16 bit per pixel
texture of the desired height and mip level. */
//=============================================================================
TerrainTextureClass::TerrainTextureClass(int height) :
	TextureClass(TERRAIN_TEXTURE_WIDTH, height,
		WW3D_FORMAT_A1R5G5B5, MIP_LEVELS_3 ),
	m_sourceHeightMap(nullptr),
	m_isFlatTexture(false),
	m_flatXCell(0),
	m_flatYCell(0),
	m_flatCellWidth(0),
	m_flatPixelsPerCell(0)
{
}

//=============================================================================
// TerrainTextureClass::TerrainTextureClass
//=============================================================================
/** Constructor. Calls parent constructor to create a 16 bit per pixel
texture of the desired height and mip level. */
//=============================================================================
TerrainTextureClass::TerrainTextureClass(int height, int width) :
	TextureClass(width, height,
		WW3D_FORMAT_A1R5G5B5, MIP_LEVELS_ALL ),
	m_sourceHeightMap(nullptr),
	m_isFlatTexture(true),
	m_flatXCell(0),
	m_flatYCell(0),
	m_flatCellWidth(0),
	m_flatPixelsPerCell(0)
{
}

bool TerrainTextureClass::Recreate_Procedural_Texture()
{
	if (!TextureClass::Recreate_Procedural_Texture()) {
		return false;
	}

	const bool populated = m_isFlatTexture
		? updateFlat(m_sourceHeightMap, m_flatXCell, m_flatYCell, m_flatCellWidth, m_flatPixelsPerCell) != 0
		: update(m_sourceHeightMap) != 0;
	if (!populated) {
		Set_Render_Backend_Texture(0);
	}
	return populated;
}


//=============================================================================
// TerrainTextureClass::update
//=============================================================================
/** Sets the tile bitmap data into the texture.  The tiles are placed with 4
	pixel borders around them, so that when the tiles are scaled and bilinearly
	interpolated, you don't get seams between the tiles.  */
//=============================================================================
int TerrainTextureClass::update(WorldHeightMap *htMap)
{
	if (htMap == nullptr) {
		return 0;
	}
	m_sourceHeightMap = htMap;
	m_isFlatTexture = false;

	IRenderBackend *backend = WW3D::Get_Render_Backend();
	const RenderBackendTextureHandle texture = Peek_Render_Backend_Texture();
	RenderBackendTextureDescription texture_description;
	RenderBackendTextureLock locked_texture;
	if (texture == 0 ||
		!backend->Get_Texture_Description(texture, 0, texture_description) ||
		texture_description.width < TERRAIN_TEXTURE_WIDTH ||
		!backend->Lock_Texture(texture, 0, locked_texture, false)) {
		return 0;
	}

	Int tilePixelExtent = TERRAIN_TILE_PIXEL_EXTENT;
	Int tilesPerRow = texture_description.width/(2*TERRAIN_TILE_PIXEL_EXTENT+TERRAIN_TILE_OFFSET);
	tilesPerRow *= 2;
//	Int numRows = surface_desc.Height/(tilePixelExtent+TILE_OFFSET);
#ifdef RTS_DEBUG
	//DEBUG_ASSERTCRASH(tilesPerRow*numRows >= htMap->m_numBitmapTiles, ("Too many tiles."));
	DEBUG_ASSERTCRASH((Int)texture_description.width >= tilePixelExtent*tilesPerRow, ("Bitmap too small."));
#endif
	if (texture_description.format == WW3D_FORMAT_A1R5G5B5) {
		Int tileNdx;
		Int pixelBytes = 2;
		for (tileNdx=0; tileNdx < htMap->m_numBitmapTiles; tileNdx++) {
			TileData *pTile = htMap->getSourceTile(tileNdx);
			if (!pTile) continue;
			ICoord2D position = pTile->m_tileLocationInTexture;
			if (position.x<=0) continue; // all real tile offsets start at 2.  jba.

			Int i,j;
			for (j=0; j<tilePixelExtent; j++) {
				UnsignedByte *pBGR = pTile->getRGBDataForWidth(tilePixelExtent);
				pBGR += (tilePixelExtent-1-j)*TILE_BYTES_PER_PIXEL*tilePixelExtent; // invert to match.
				Int row = position.y+j;
				UnsignedByte *pBGRX = ((UnsignedByte*)locked_texture.bits) +
							(row)*locked_texture.row_pitch;

				Int column = position.x;
				pBGRX += column*pixelBytes;
				for (i=0; i<tilePixelExtent; i++) {
					const unsigned packed_pixel = 0x8000u + ((pBGR[2]>>3)<<10) + ((pBGR[1]>>3)<<5) + (pBGR[0]>>3);
					*((unsigned short*)pBGRX) = static_cast<unsigned short>(packed_pixel);
					pBGRX +=pixelBytes;
					pBGR +=TILE_BYTES_PER_PIXEL;
				}
			}
		}
		// Now draw the 4 pixel border around each tile class.
		Int texClass;
		for (texClass=0; texClass<htMap->m_numTextureClasses; texClass++) {
			Int width = htMap->m_textureClasses[texClass].width;
			ICoord2D origin = htMap->m_textureClasses[texClass].positionInTexture;
			if (origin.x<=0) continue;
			width *= TERRAIN_TILE_PIXEL_EXTENT;
			const Int border = TERRAIN_TILE_OFFSET/2;
			// Duplicate the border columns before and after each class.
			Int j;
			for (j=0; j<width; j++) {
				Int row = origin.y+j;
			UnsignedByte *pBGRX = ((UnsignedByte*)locked_texture.bits) +
						(row)*locked_texture.row_pitch;

				Int column = origin.x;
				pBGRX += column*pixelBytes;
				// copy before
				memcpy(pBGRX-border*pixelBytes, pBGRX+(width-border)*pixelBytes, border*pixelBytes);
				// copy after
				memcpy(pBGRX+(width*pixelBytes), pBGRX, border*pixelBytes);
			}

			// Duplicate the border rows before and after each class.
			for (j=0; j<border; j++) {
				// copy before.
				Int row = origin.y-j-1;
				UnsignedByte *pBGRX = ((UnsignedByte*)locked_texture.bits) +
							(row)*locked_texture.row_pitch;
				UnsignedByte *target = pBGRX+(origin.x-border)*pixelBytes;
				memcpy(target, target+width*locked_texture.row_pitch, (width+2*border)*pixelBytes);
				// copy after.
				row = origin.y+j;
				pBGRX = ((UnsignedByte*)locked_texture.bits) +
							(row)*locked_texture.row_pitch;
				target = pBGRX+(origin.x-border)*pixelBytes;
				memcpy(target+width*locked_texture.row_pitch, target, (width+2*border)*pixelBytes);
			}

		}

	}
	backend->Unlock_Texture(texture, 0);
	backend->Generate_Texture_Mipmaps(texture);
	if (WW3D::Get_Texture_Reduction()) {
		backend->Set_Texture_LOD(texture, WW3D::Get_Texture_Reduction());
	}
	return(static_cast<int>(texture_description.height));
}

//=============================================================================
// TerrainTextureClass::setLOD
//=============================================================================
/** Sets the lod of the texture to be loaded into the video card.  */
//=============================================================================
void TerrainTextureClass::setLOD(Int LOD)
{
	const RenderBackendTextureHandle texture = Peek_Render_Backend_Texture();
	if (texture != 0) {
		WW3D::Get_Render_Backend()->Set_Texture_LOD(texture, static_cast<unsigned>(LOD));
	}
}
//=============================================================================
// TerrainTextureClass::update
//=============================================================================
/** Sets the tile bitmap data into the texture.  The tiles are placed with 4
	pixel borders around them, so that when the tiles are scaled and bilinearly
	interpolated, you don't get seams between the tiles.  */
//=============================================================================
Bool TerrainTextureClass::updateFlat(WorldHeightMap *htMap, Int xCell, Int yCell, Int cellWidth, Int pixelsPerCell)
{
	if (htMap == nullptr) {
		return false;
	}
	m_sourceHeightMap = htMap;
	m_isFlatTexture = true;
	m_flatXCell = xCell;
	m_flatYCell = yCell;
	m_flatCellWidth = cellWidth;
	m_flatPixelsPerCell = pixelsPerCell;

	IRenderBackend *backend = WW3D::Get_Render_Backend();
	const RenderBackendTextureHandle texture = Peek_Render_Backend_Texture();
	RenderBackendTextureDescription texture_description;
	RenderBackendTextureLock locked_texture;
	if (texture == 0 ||
		!backend->Get_Texture_Description(texture, 0, texture_description)) {
		return false;
	}
	DEBUG_ASSERTCRASH((Int)texture_description.width == cellWidth*pixelsPerCell, ("Bitmap too small."));
	DEBUG_ASSERTCRASH((Int)texture_description.height == cellWidth*pixelsPerCell, ("Bitmap too small."));
	if (texture_description.width != static_cast<unsigned>(cellWidth*pixelsPerCell) ||
		texture_description.height != static_cast<unsigned>(cellWidth*pixelsPerCell)) {
		return false;
	}
	if (!backend->Lock_Texture(texture, 0, locked_texture, false)) {
		return false;
	}

	if (texture_description.format == WW3D_FORMAT_A1R5G5B5) {

		Int pixelBytes = 2;
		Int cellX, cellY;
		for (cellX = 0; cellX < cellWidth; cellX++) {
			for (cellY = 0; cellY < cellWidth; cellY++) {
				UnsignedByte *pBGRX_data = ((UnsignedByte*)locked_texture.bits);
				UnsignedByte *pBGR = htMap->getPointerToTileData(xCell+cellX, yCell+cellY, pixelsPerCell);
				if (pBGR == nullptr) continue; // past end of defined terrain. [3/24/2003]
				Int k, l;
				for (k=pixelsPerCell-1; k>=0; k--) {
					UnsignedByte *pBGRX = pBGRX_data + (pixelsPerCell*(cellWidth-cellY-1)+k)*locked_texture.row_pitch +
						cellX*pixelsPerCell*pixelBytes;
					for (l=0; l<pixelsPerCell; l++) {
						*((Short*)pBGRX) = 0x8000 + ((pBGR[2]>>3)<<10) + ((pBGR[1]>>3)<<5) + (pBGR[0]>>3);
						pBGRX +=pixelBytes;
						pBGR +=TILE_BYTES_PER_PIXEL;
					}
				}
			}
		}
	}

	backend->Unlock_Texture(texture, 0);
	backend->Generate_Texture_Mipmaps(texture);
	return(static_cast<int>(texture_description.height));
}

//=============================================================================
// TerrainTextureClass::Apply
//=============================================================================
/** Sets the texture as the current D3D texture, and does some custom setup
(standard D3D setup, but beyond the scope of W3D).  */
//=============================================================================
void TerrainTextureClass::Apply(unsigned int stage)
{
	// Do the base apply.
	TextureClass::Apply(stage);
}

/******************************************************************************
						AlphaTerrainTextureClass
******************************************************************************/
//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

//=============================================================================
// AlphaTerrainTextureClass::AlphaTerrainTextureClass
//=============================================================================
/** Constructor. Calls parent constructor to creat a throw away 8x8 texture,
then shares the base texture. This way the base tiles pass, drawn
using TerrainTextureClass shares the same texture with the blended edges pass,
saving lots of texture memory, and preventing seams between blended tiles. */
//=============================================================================
AlphaTerrainTextureClass::AlphaTerrainTextureClass( TextureClass *pBaseTex ):
	TextureClass(8, 8,
		WW3D_FORMAT_A1R5G5B5, MIP_LEVELS_1, TextureClass::POOL_DEFAULT ),
	m_baseTexture(nullptr)
{
	// The parent constructor creates and registers a temporary default-pool
	// texture.  This object must instead be tracked as an alias of the terrain
	// atlas, so remove the temporary tracker and release its resource before
	// attaching the shared atlas below.
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	backend->Unregister_Texture(this);
	Set_Render_Backend_Texture(0);

	REF_PTR_SET(m_baseTexture, pBaseTex);
	if (m_baseTexture != nullptr) {
		m_baseTexture->Ensure_Render_Backend_Texture();
	}

	// Share the base texture's backend resource.
	Set_Render_Backend_Texture(
		backend->Add_Texture_Reference(
			m_baseTexture != nullptr ? m_baseTexture->Peek_Render_Backend_Texture() : 0));
	Initialized = Peek_Render_Backend_Texture() != 0;
	backend->Register_Texture(this, RenderBackendTextureKind::Texture2D,
		8, 8, 1, WW3D_FORMAT_A1R5G5B5, WW3D_ZFORMAT_UNKNOWN,
		MIP_LEVELS_1, false);
}

AlphaTerrainTextureClass::~AlphaTerrainTextureClass()
{
	REF_PTR_RELEASE(m_baseTexture);
}

bool AlphaTerrainTextureClass::Recreate_Procedural_Texture()
{
	if (m_baseTexture == nullptr || !m_baseTexture->Ensure_Render_Backend_Texture()) {
		return false;
	}

	const RenderBackendTextureHandle base_texture = m_baseTexture->Peek_Render_Backend_Texture();
	if (base_texture == 0) {
		return false;
	}

	Set_Render_Backend_Texture(WW3D::Get_Render_Backend()->Add_Texture_Reference(base_texture));
	return Peek_Render_Backend_Texture() != 0;
}


//=============================================================================
// AlphaTerrainTextureClass::Apply
//=============================================================================
/** Sets the texture as current and does some custom setup.
This may be applied in either single pass, as the second texture in the pipe,
or multipass.  If stage==0, we are doing multipass and we set up the pipe
for a single texture.  If stage==1, then we are doing a single pass, and we
set up the pipe so that we blend onto the base texture in stage 0.
(standard setup, but beyond the scope of W3D). */
//=============================================================================
void AlphaTerrainTextureClass::Apply(unsigned int stage)
{
	// Do the base apply.
	TextureClass::Apply(stage);

	// Set the bilinear or trilinear filtering.
	if (TheGlobalData && (TheGlobalData->m_bilinearTerrainTex || TheGlobalData->m_trilinearTerrainTex)) {
		WW3D::Get_Render_Backend()->Set_Texture_Filter(stage, RenderBackendTextureFilterType::Minification, RenderBackendTextureFilter::Linear);
		WW3D::Get_Render_Backend()->Set_Texture_Filter(stage, RenderBackendTextureFilterType::Magnification, RenderBackendTextureFilter::Linear);
	} else {
		WW3D::Get_Render_Backend()->Set_Texture_Filter(stage, RenderBackendTextureFilterType::Minification, RenderBackendTextureFilter::Point);
		WW3D::Get_Render_Backend()->Set_Texture_Filter(stage, RenderBackendTextureFilterType::Magnification, RenderBackendTextureFilter::Point);
	}
	if (TheGlobalData && TheGlobalData->m_trilinearTerrainTex) {
		WW3D::Get_Render_Backend()->Set_Texture_Filter(stage, RenderBackendTextureFilterType::MipMap, RenderBackendTextureFilter::Linear);
	} else {
		WW3D::Get_Render_Backend()->Set_Texture_Filter(stage, RenderBackendTextureFilterType::MipMap, RenderBackendTextureFilter::Point);
	}
	// Since we are using multiple distinct tiles, the textures doesn't wrap, so clamp it.
	WW3D::Get_Render_Backend()->Set_Texture_Address_Mode(0, true, RenderBackendTextureAddressMode::Clamp);
	WW3D::Get_Render_Backend()->Set_Texture_Address_Mode(0, false, RenderBackendTextureAddressMode::Clamp);
	// Now setup the texture pipeline.
	if (stage==0) {
		// Modulate the diffuse color with the texture as lighting comes from diffuse.
		WW3D::Get_Render_Backend()->Set_Texture_Argument(0, RenderBackendTextureComponent::Color, 1, RenderBackendTextureArgument::Texture, RenderBackendTextureArgumentModifiers::None);
		WW3D::Get_Render_Backend()->Set_Texture_Argument(0, RenderBackendTextureComponent::Color, 2, RenderBackendTextureArgument::Diffuse, RenderBackendTextureArgumentModifiers::None);
		WW3D::Get_Render_Backend()->Set_Texture_Operation(0, RenderBackendTextureComponent::Color, RenderBackendTextureOperation::Modulate);
		WW3D::Get_Render_Backend()->Set_Texture_Operation(0, RenderBackendTextureComponent::Alpha, RenderBackendTextureOperation::Modulate);
		WW3D::Get_Render_Backend()->Set_Texture_Coordinate_Source(0, RenderBackendTextureCoordinateSource::PassThrough, 1);
		// Blend the result using the alpha. (came from diffuse mod texture)
		WW3D::Get_Render_Backend()->Set_Alpha_Blend_Enabled(true);
		WW3D::Get_Render_Backend()->Set_Source_Blend_Factor(RenderBackendBlendFactor::SourceAlpha);
		WW3D::Get_Render_Backend()->Set_Destination_Blend_Factor(RenderBackendBlendFactor::InverseSourceAlpha);
		// Disable stage 2.
		WW3D::Get_Render_Backend()->Set_Texture_Operation(1, RenderBackendTextureComponent::Color, RenderBackendTextureOperation::Disable);
		WW3D::Get_Render_Backend()->Set_Texture_Operation(1, RenderBackendTextureComponent::Alpha, RenderBackendTextureOperation::Disable);
	}	else if (stage==1) {

		if (TheGlobalData && !TheGlobalData->m_multiPassTerrain)
		{
			///@todo: Remove 8-Stage Nvidia hack after drivers are fixed.
			//This method is a backdoor specific to Nvidia based cards.  It will fail on
			//other hardware.  Allows single pass blend of 2 textures and post modulate diffuse.
			WW3D::Get_Render_Backend()->Set_Texture_Operation(0, RenderBackendTextureComponent::Color, RenderBackendTextureOperation::Modulate);
			WW3D::Get_Render_Backend()->Set_Texture_Coordinate_Source(0, RenderBackendTextureCoordinateSource::PassThrough, 0);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(0, RenderBackendTextureComponent::Color, 1, RenderBackendTextureArgument::Texture, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(0, RenderBackendTextureComponent::Color, 2, RenderBackendTextureArgument::Diffuse, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(0, RenderBackendTextureComponent::Alpha, RenderBackendTextureOperation::Modulate);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(0, RenderBackendTextureComponent::Alpha, 1, RenderBackendTextureArgument::Texture, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(0, RenderBackendTextureComponent::Alpha, 2, RenderBackendTextureArgument::Diffuse, RenderBackendTextureArgumentModifiers::None);

			WW3D::Get_Render_Backend()->Set_Texture_Operation(1, RenderBackendTextureComponent::Color, RenderBackendTextureOperation::Add);
			WW3D::Get_Render_Backend()->Set_Texture_Coordinate_Source(1, RenderBackendTextureCoordinateSource::PassThrough, 1);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(1, RenderBackendTextureComponent::Color, 1, RenderBackendTextureArgument::Diffuse, RenderBackendTextureArgumentModifiers::Complement | RenderBackendTextureArgumentModifiers::AlphaReplicate);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(1, RenderBackendTextureComponent::Color, 2, RenderBackendTextureArgument::Diffuse, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(1, RenderBackendTextureComponent::Alpha, RenderBackendTextureOperation::Add);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(1, RenderBackendTextureComponent::Alpha, 1, RenderBackendTextureArgument::TextureFactor, RenderBackendTextureArgumentModifiers::Complement);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(1, RenderBackendTextureComponent::Alpha, 2, RenderBackendTextureArgument::TextureFactor, RenderBackendTextureArgumentModifiers::None);

			WW3D::Get_Render_Backend()->Set_Texture_Resource(2, nullptr);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(2, RenderBackendTextureComponent::Color, RenderBackendTextureOperation::Modulate);
			WW3D::Get_Render_Backend()->Set_Texture_Coordinate_Source(2, RenderBackendTextureCoordinateSource::PassThrough, 2);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(2, RenderBackendTextureComponent::Color, 1, RenderBackendTextureArgument::Texture, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(2, RenderBackendTextureComponent::Color, 2, RenderBackendTextureArgument::Texture, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(2, RenderBackendTextureComponent::Alpha, RenderBackendTextureOperation::Modulate);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(2, RenderBackendTextureComponent::Alpha, 1, RenderBackendTextureArgument::TextureFactor, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(2, RenderBackendTextureComponent::Alpha, 2, RenderBackendTextureArgument::TextureFactor, RenderBackendTextureArgumentModifiers::None);

			WW3D::Get_Render_Backend()->Set_Texture_Resource(3, nullptr);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(3, RenderBackendTextureComponent::Color, RenderBackendTextureOperation::SelectArgument1);
			WW3D::Get_Render_Backend()->Set_Texture_Coordinate_Source(3, RenderBackendTextureCoordinateSource::PassThrough, 3);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(3, RenderBackendTextureComponent::Color, 1, RenderBackendTextureArgument::Diffuse, RenderBackendTextureArgumentModifiers::AlphaReplicate);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(3, RenderBackendTextureComponent::Color, 2, RenderBackendTextureArgument::Diffuse, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(3, RenderBackendTextureComponent::Alpha, RenderBackendTextureOperation::SelectArgument1);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(3, RenderBackendTextureComponent::Alpha, 1, RenderBackendTextureArgument::TextureFactor, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(3, RenderBackendTextureComponent::Alpha, 2, RenderBackendTextureArgument::TextureFactor, RenderBackendTextureArgumentModifiers::None);

			WW3D::Get_Render_Backend()->Set_Texture_Resource(4, nullptr);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(4, RenderBackendTextureComponent::Color, RenderBackendTextureOperation::Modulate);
			WW3D::Get_Render_Backend()->Set_Texture_Coordinate_Source(4, RenderBackendTextureCoordinateSource::PassThrough, 4);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(4, RenderBackendTextureComponent::Color, 1, RenderBackendTextureArgument::Current, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(4, RenderBackendTextureComponent::Color, 2, RenderBackendTextureArgument::Diffuse, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(4, RenderBackendTextureComponent::Alpha, RenderBackendTextureOperation::Modulate);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(4, RenderBackendTextureComponent::Alpha, 1, RenderBackendTextureArgument::Current, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(4, RenderBackendTextureComponent::Alpha, 2, RenderBackendTextureArgument::Diffuse, RenderBackendTextureArgumentModifiers::None);

			WW3D::Get_Render_Backend()->Set_Texture_Resource(5, nullptr);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(5, RenderBackendTextureComponent::Color, RenderBackendTextureOperation::Add);
			WW3D::Get_Render_Backend()->Set_Texture_Coordinate_Source(5, RenderBackendTextureCoordinateSource::PassThrough, 5);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(5, RenderBackendTextureComponent::Color, 1, RenderBackendTextureArgument::Diffuse, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(5, RenderBackendTextureComponent::Color, 2, RenderBackendTextureArgument::Diffuse, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(5, RenderBackendTextureComponent::Alpha, RenderBackendTextureOperation::Add);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(5, RenderBackendTextureComponent::Alpha, 1, RenderBackendTextureArgument::TextureFactor, RenderBackendTextureArgumentModifiers::Complement);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(5, RenderBackendTextureComponent::Alpha, 2, RenderBackendTextureArgument::TextureFactor, RenderBackendTextureArgumentModifiers::None);

			WW3D::Get_Render_Backend()->Set_Texture_Resource(6, nullptr);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(6, RenderBackendTextureComponent::Color, RenderBackendTextureOperation::Modulate);
			WW3D::Get_Render_Backend()->Set_Texture_Coordinate_Source(6, RenderBackendTextureCoordinateSource::PassThrough, 6);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(6, RenderBackendTextureComponent::Color, 1, RenderBackendTextureArgument::TextureFactor, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(6, RenderBackendTextureComponent::Color, 2, RenderBackendTextureArgument::TextureFactor, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(6, RenderBackendTextureComponent::Alpha, RenderBackendTextureOperation::Modulate);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(6, RenderBackendTextureComponent::Alpha, 1, RenderBackendTextureArgument::TextureFactor, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(6, RenderBackendTextureComponent::Alpha, 2, RenderBackendTextureArgument::TextureFactor, RenderBackendTextureArgumentModifiers::None);

			WW3D::Get_Render_Backend()->Set_Texture_Resource(7, nullptr);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(7, RenderBackendTextureComponent::Color, RenderBackendTextureOperation::SelectArgument1);
			WW3D::Get_Render_Backend()->Set_Texture_Coordinate_Source(7, RenderBackendTextureCoordinateSource::PassThrough, 7);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(7, RenderBackendTextureComponent::Color, 1, RenderBackendTextureArgument::TextureFactor, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(7, RenderBackendTextureComponent::Color, 2, RenderBackendTextureArgument::TextureFactor, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(7, RenderBackendTextureComponent::Alpha, RenderBackendTextureOperation::SelectArgument1);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(7, RenderBackendTextureComponent::Alpha, 1, RenderBackendTextureArgument::TextureFactor, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(7, RenderBackendTextureComponent::Alpha, 2, RenderBackendTextureArgument::TextureFactor, RenderBackendTextureArgumentModifiers::None);
		}
		else
		{
			WW3D::Get_Render_Backend()->Set_Texture_Argument(0, RenderBackendTextureComponent::Color, 1, RenderBackendTextureArgument::Texture, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(0, RenderBackendTextureComponent::Color, RenderBackendTextureOperation::SelectArgument1);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(0, RenderBackendTextureComponent::Alpha, 1, RenderBackendTextureArgument::Texture, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(0, RenderBackendTextureComponent::Alpha, RenderBackendTextureOperation::SelectArgument1);

			WW3D::Get_Render_Backend()->Set_Texture_Argument(1, RenderBackendTextureComponent::Color, 1, RenderBackendTextureArgument::Texture, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(1, RenderBackendTextureComponent::Color, RenderBackendTextureOperation::Modulate);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(1, RenderBackendTextureComponent::Color, 2, RenderBackendTextureArgument::Current, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(1, RenderBackendTextureComponent::Alpha, 1, RenderBackendTextureArgument::Texture, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(1, RenderBackendTextureComponent::Alpha, RenderBackendTextureOperation::SelectArgument1);
		}
	}
}


/******************************************************************************
						LightMapTerrainTextureClass
******************************************************************************/
//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

//=============================================================================
// LightMapTerrainTextureClass::LightMapTerrainTextureClass
//=============================================================================
/** Constructor. Calls parent constructor to load the .tga texture. */
//=============================================================================
LightMapTerrainTextureClass::LightMapTerrainTextureClass(AsciiString name, MipCountType mipLevelCount) :
TextureClass(name.isEmpty()?"TSNoiseUrb.tga":name.str(),name.isEmpty()?"TSNoiseUrb.tga":name.str(), mipLevelCount )
{
	Get_Filter().Set_Min_Filter(TextureFilterClass::FILTER_TYPE_BEST);
	Get_Filter().Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_BEST);
	Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_REPEAT);
	Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_REPEAT);
}

#define STRETCH_FACTOR ((float)(1/(63.0*MAP_XY_FACTOR/2))) /* covers 63/2 tiles */

//=============================================================================
// LightMapTerrainTextureClass::Apply
//=============================================================================
/** Sets the texture as the current D3D texture, and does some custom setup.
The LightMapTerrainTextureClass may be applied by itself, or with the
CloudMapTerrainTextureClass.  This may be applied in either single pass,
as the second texture in the pipe,
or multipass.  If stage==0, we are doing multipass and we set up the pipe
for a single texture.  If stage==1, then we are doing a single pass, and we
set up the pipe so that we blend onto the cloud map texture in stage 0.
Also, texture is mapped using the x/y coordinates of the map, saving us
yet another set of uv coordinates.
(standard D3D setup, but beyond the scope of W3D). */
//=============================================================================
void LightMapTerrainTextureClass::Apply(unsigned int stage)
{
	TextureClass::Apply(stage);
}









/******************************************************************************
						AlphaEdgeTextureClass
******************************************************************************/
//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

/**
* AlphaEdgeTextureClass - Generates the alpha edge blending for terrain.
*
*/
AlphaEdgeTextureClass::AlphaEdgeTextureClass( int height, MipCountType mipLevelCount) :
//	TextureClass("EdgingTemplate.tga","EdgingTemplate.tga", mipLevelCount )
	TextureClass(TERRAIN_TEXTURE_WIDTH, height, WW3D_FORMAT_A8R8G8B8, mipLevelCount ),
	m_sourceHeightMap(nullptr)
{

}

int AlphaEdgeTextureClass::update256(WorldHeightMap *htMap)
{
	return 1;
}

int AlphaEdgeTextureClass::update(WorldHeightMap *htMap)
{
	if (htMap == nullptr) {
		return 0;
	}
	m_sourceHeightMap = htMap;

	IRenderBackend *backend = WW3D::Get_Render_Backend();
	const RenderBackendTextureHandle texture = Peek_Render_Backend_Texture();
	RenderBackendTextureDescription texture_description;
	RenderBackendTextureLock locked_texture;
	if (texture == 0 ||
		!backend->Get_Texture_Description(texture, 0, texture_description) ||
		!backend->Lock_Texture(texture, 0, locked_texture, false)) {
		return 0;
	}

	Int tilePixelExtent = TERRAIN_TILE_PIXEL_EXTENT;
//	Int tilesPerRow = surface_desc.Width / (tilePixelExtent+8);

//	Int numRows = surface_desc.Height/(tilePixelExtent+8);

	if (texture_description.format == WW3D_FORMAT_A8R8G8B8) {
#if 1
#if 1
		Int cellX, cellY;
		for (cellX = 0; (UnsignedInt)cellX < texture_description.width; cellX++) {
			for (cellY = 0; cellY < texture_description.height; cellY++) {
				UnsignedByte *pBGR = ((UnsignedByte *)locked_texture.bits)+cellY*locked_texture.row_pitch+cellX*4;
				pBGR[2] = 255-cellY/2;
				pBGR[0] = cellX/2;
				pBGR[3] = cellX/2;  // alpha.
				pBGR[3] = 128;  // alpha.
			}
		}
#endif
#if 1
		Int tileNdx;
		Int pixelBytes = 4;
		for (tileNdx=0; tileNdx < htMap->m_numEdgeTiles; tileNdx++) {
			TileData *pTile = htMap->getEdgeTile(tileNdx);
			if (!pTile) continue;
			ICoord2D position = pTile->m_tileLocationInTexture;
			if (position.x<=0) continue; // all real edge offsets start at 4.  jba.
			Int i,j;
			Int column = position.x;
			for (j=0; j<tilePixelExtent; j++) {
				Int row = position.y+j;
				UnsignedByte *pBGR = htMap->getEdgeTile(tileNdx)->getRGBDataForWidth(tilePixelExtent);
				pBGR += (tilePixelExtent-1-j)*TILE_BYTES_PER_PIXEL*tilePixelExtent; // invert to match.
				UnsignedByte *pBGRX = ((UnsignedByte*)locked_texture.bits) +
							(row)*locked_texture.row_pitch;
				pBGRX += column*pixelBytes;

				for (i=0; i<tilePixelExtent; i++) {
					pBGRX[0] = pBGR[0];  //r
					pBGRX[1] = pBGR[1];	//g
					pBGRX[2] = pBGR[2];	//b
					if (pBGR[0]==0 && pBGR[1]==0 && pBGR[2]==0) {
						pBGRX[3] = 0x80;
					} else if (pBGR[0]==0xff && pBGR[1]==0xff && pBGR[2]==0xff) {
						pBGRX[3] = 0x00;
					}	else {
						pBGRX[3] = 0xff;
					}

					pBGRX += pixelBytes;
					pBGR += TILE_BYTES_PER_PIXEL;
				}
			}
		}
#endif
#endif
	}
	backend->Unlock_Texture(texture, 0);
	backend->Generate_Texture_Mipmaps(texture);
	return(static_cast<int>(texture_description.height));
}

bool AlphaEdgeTextureClass::Recreate_Procedural_Texture()
{
	if (!TextureClass::Recreate_Procedural_Texture()) {
		return false;
	}

	if (update(m_sourceHeightMap) == 0) {
		Set_Render_Backend_Texture(0);
		return false;
	}
	return true;
}

void AlphaEdgeTextureClass::Apply(unsigned int stage)
{
	// Do the base apply.
	TextureClass::Apply(stage);
}


/******************************************************************************
						CloudMapTerrainTextureClass
******************************************************************************/
//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

//=============================================================================
// CloudMapTerrainTextureClass::CloudMapTerrainTextureClass
//=============================================================================
/** Constructor. Calls parent constructor to load the .tga texture, and sets
up the "sliding" parameters for the clouds to slide over the terrain. */
//=============================================================================
//@todo - Allow adjustment of the cloud slide rate, and lose the hard coded "cloudmap.tga"
CloudMapTerrainTextureClass::CloudMapTerrainTextureClass(MipCountType mipLevelCount) :
	TextureClass("TSCloudMed.tga","TSCloudMed.tga", mipLevelCount )
{
	Get_Filter().Set_Mip_Mapping( TextureFilterClass::FILTER_TYPE_FAST );
	m_xSlidePerSecond = -0.02f;
	m_ySlidePerSecond =  1.50f * m_xSlidePerSecond;
	m_curTick = 0;
	m_xOffset = 0;
	m_yOffset = 0;

}

//=============================================================================
// CloudMapTerrainTextureClass::Apply
//=============================================================================
/** Sets the texture as the current D3D texture, and does some custom setup.
The CloudMapTerrainTextureClass may be applied by itself, or with the
LightMapTerrainTexture.  This may be applied in either single pass,
as the first texture in the pipe with LightMapTerrainTextureClass as the
second stage of the pape, or multipass.  We setup for stage 0, assuming that
we are the only texture, as LightMapTerrainTexture will adjust for multitexture
if it is applied to stage 1.
Also, texture is mapped using the x/y coordinates of the map, saving us
yet another set of uv coordinates.
(standard D3D setup, but beyond the scope of W3D). */
//=============================================================================
void CloudMapTerrainTextureClass::Apply(unsigned int stage)
{


	// Do the base apply.
	TextureClass::Apply(stage);
}

//=============================================================================
// CloudMapTerrainTextureClass::restore
//=============================================================================
/** Cleans up any custom settings to the texturing pipeline that may not be
understood by w3d. */
//=============================================================================
void CloudMapTerrainTextureClass::restore()
{
	WW3D::Get_Render_Backend()->Set_Texture_Argument(0, RenderBackendTextureComponent::Color, 1, RenderBackendTextureArgument::Texture, RenderBackendTextureArgumentModifiers::None);
	WW3D::Get_Render_Backend()->Set_Texture_Argument(0, RenderBackendTextureComponent::Color, 2, RenderBackendTextureArgument::Diffuse, RenderBackendTextureArgumentModifiers::None);
	WW3D::Get_Render_Backend()->Set_Texture_Operation(0, RenderBackendTextureComponent::Color, RenderBackendTextureOperation::Modulate);
	WW3D::Get_Render_Backend()->Set_Texture_Operation(0, RenderBackendTextureComponent::Alpha, RenderBackendTextureOperation::Disable);

	WW3D::Get_Render_Backend()->Set_Texture_Address_Mode(0, true, RenderBackendTextureAddressMode::Wrap);
	WW3D::Get_Render_Backend()->Set_Texture_Address_Mode(0, false, RenderBackendTextureAddressMode::Wrap);
	WW3D::Get_Render_Backend()->Set_Texture_Coordinate_Source(0, RenderBackendTextureCoordinateSource::PassThrough, 0);
	WW3D::Get_Render_Backend()->Set_Texture_Transform_Flags(0, RenderBackendTextureTransformFlags::Disabled);

	WW3D::Get_Render_Backend()->Set_Texture_Argument(1, RenderBackendTextureComponent::Color, 1, RenderBackendTextureArgument::Texture, RenderBackendTextureArgumentModifiers::None);
	WW3D::Get_Render_Backend()->Set_Texture_Argument(1, RenderBackendTextureComponent::Color, 2, RenderBackendTextureArgument::Diffuse, RenderBackendTextureArgumentModifiers::None);
	WW3D::Get_Render_Backend()->Set_Texture_Operation(1, RenderBackendTextureComponent::Color, RenderBackendTextureOperation::Modulate);
	WW3D::Get_Render_Backend()->Set_Texture_Operation(1, RenderBackendTextureComponent::Alpha, RenderBackendTextureOperation::Disable);

	WW3D::Get_Render_Backend()->Set_Texture_Address_Mode(1, true, RenderBackendTextureAddressMode::Wrap);
	WW3D::Get_Render_Backend()->Set_Texture_Address_Mode(1, false, RenderBackendTextureAddressMode::Wrap);
	WW3D::Get_Render_Backend()->Set_Texture_Coordinate_Source(1, RenderBackendTextureCoordinateSource::PassThrough, 0);
	WW3D::Get_Render_Backend()->Set_Texture_Transform_Flags(1, RenderBackendTextureTransformFlags::Disabled);
	WW3D::Get_Render_Backend()->Set_Alpha_Blend_Enabled(false);
	WW3D::Get_Render_Backend()->Set_Source_Blend_Factor(RenderBackendBlendFactor::SourceAlpha);
	WW3D::Get_Render_Backend()->Set_Destination_Blend_Factor(RenderBackendBlendFactor::InverseSourceAlpha);


	if (TheGlobalData && !TheGlobalData->m_multiPassTerrain)
	{
		///@todo: Remove 8-Stage Nvidia hack after drivers are fixed.
		//This method is a backdoor specific to Nvidia based cards.  It will fail on
		//other hardware.  Allows single pass blend of 2 textures and post modulate diffuse.
		Int i;
		for (i=0; i<8; i++) {
			WW3D::Get_Render_Backend()->Set_Texture_Operation(i, RenderBackendTextureComponent::Color, RenderBackendTextureOperation::Disable);
			WW3D::Get_Render_Backend()->Set_Texture_Coordinate_Source(i, RenderBackendTextureCoordinateSource::PassThrough, i);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(i, RenderBackendTextureComponent::Color, 1, RenderBackendTextureArgument::Texture, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(i, RenderBackendTextureComponent::Color, 2, RenderBackendTextureArgument::Diffuse, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Operation(i, RenderBackendTextureComponent::Alpha, RenderBackendTextureOperation::Disable);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(i, RenderBackendTextureComponent::Alpha, 1, RenderBackendTextureArgument::Texture, RenderBackendTextureArgumentModifiers::None);
			WW3D::Get_Render_Backend()->Set_Texture_Argument(i, RenderBackendTextureComponent::Alpha, 2, RenderBackendTextureArgument::Diffuse, RenderBackendTextureArgumentModifiers::None);

			WW3D::Get_Render_Backend()->Set_Texture_Resource(i, nullptr);
		}
	}
}

/******************************************************************************
						ScorchTextureClass
******************************************************************************/
//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

//=============================================================================
// ScorchTextureClass::ScorchTextureClass
//=============================================================================
/** Constructor. Calls parent constructor to load the .tga texture. */
//=============================================================================
/// @todo - get "EXScorch01.tga" from not hard coded location.
ScorchTextureClass::ScorchTextureClass(MipCountType mipLevelCount) :
	TextureClass("EXScorch01.tga","EXScorch01.tga", mipLevelCount )
// Hack to disable texture reduction.
//	TextureClass("EXScorch01.tga","EXScorch01.tga", mipLevelCount,WW3D_FORMAT_UNKNOWN,true,false)
{
}

//=============================================================================
// ScorchTextureClass::Apply
//=============================================================================
/** Sets the texture as the current D3D texture, and does some custom setup.
The ScorchTextureClass is applied by iteself, as it's mesh is a subset of the
terrain mesh.
(standard D3D setup, but beyond the scope of W3D). */
//=============================================================================
void ScorchTextureClass::Apply(unsigned int stage)
{
	// Do the base apply.
	TextureClass::Apply(stage);
	// Setup bilinear or trilinear filtering as specified in global data.
	if (TheGlobalData && (TheGlobalData->m_bilinearTerrainTex || TheGlobalData->m_trilinearTerrainTex)) {
		WW3D::Get_Render_Backend()->Set_Texture_Filter(stage, RenderBackendTextureFilterType::Minification, RenderBackendTextureFilter::Linear);
		WW3D::Get_Render_Backend()->Set_Texture_Filter(stage, RenderBackendTextureFilterType::Magnification, RenderBackendTextureFilter::Linear);
	} else {
		WW3D::Get_Render_Backend()->Set_Texture_Filter(stage, RenderBackendTextureFilterType::Minification, RenderBackendTextureFilter::Point);
		WW3D::Get_Render_Backend()->Set_Texture_Filter(stage, RenderBackendTextureFilterType::Magnification, RenderBackendTextureFilter::Point);
	}
	if (TheGlobalData && TheGlobalData->m_trilinearTerrainTex) {
		WW3D::Get_Render_Backend()->Set_Texture_Filter(stage, RenderBackendTextureFilterType::MipMap, RenderBackendTextureFilter::Linear);
	} else {
		WW3D::Get_Render_Backend()->Set_Texture_Filter(stage, RenderBackendTextureFilterType::MipMap, RenderBackendTextureFilter::Point);
	}

	WW3D::Get_Render_Backend()->Set_Texture_Transform_Flags(0, RenderBackendTextureTransformFlags::Disabled);
	WW3D::Get_Render_Backend()->Set_Texture_Address_Mode(0, true, RenderBackendTextureAddressMode::Clamp);
	WW3D::Get_Render_Backend()->Set_Texture_Address_Mode(0, false, RenderBackendTextureAddressMode::Clamp);
	// Now setup the texture pipeline.

	WW3D::Get_Render_Backend()->Set_Texture_Argument(0, RenderBackendTextureComponent::Color, 1, RenderBackendTextureArgument::Texture, RenderBackendTextureArgumentModifiers::None);
	WW3D::Get_Render_Backend()->Set_Texture_Argument(0, RenderBackendTextureComponent::Color, 2, RenderBackendTextureArgument::Diffuse, RenderBackendTextureArgumentModifiers::None);
	WW3D::Get_Render_Backend()->Set_Texture_Operation(0, RenderBackendTextureComponent::Color, RenderBackendTextureOperation::Modulate);
	WW3D::Get_Render_Backend()->Set_Texture_Operation(0, RenderBackendTextureComponent::Alpha, RenderBackendTextureOperation::SelectArgument1);
	WW3D::Get_Render_Backend()->Set_Texture_Coordinate_Source(0, RenderBackendTextureCoordinateSource::PassThrough, 0);
	WW3D::Get_Render_Backend()->Set_Alpha_Blend_Enabled(true);
	WW3D::Get_Render_Backend()->Set_Source_Blend_Factor(RenderBackendBlendFactor::SourceAlpha);
	WW3D::Get_Render_Backend()->Set_Destination_Blend_Factor(RenderBackendBlendFactor::InverseSourceAlpha);

	WW3D::Get_Render_Backend()->Set_Texture_Operation(1, RenderBackendTextureComponent::Color, RenderBackendTextureOperation::Disable);
	WW3D::Get_Render_Backend()->Set_Texture_Operation(1, RenderBackendTextureComponent::Alpha, RenderBackendTextureOperation::Disable);
}


