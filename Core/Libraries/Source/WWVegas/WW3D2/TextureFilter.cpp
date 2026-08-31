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
 *                     $Archive:: ww3d2/TextureFilter.cpp												$*
 *                                                                                             *
 *                  $Org Author:: Kenny Mitchell                                              $*
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 08/05/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 1                                                          $*
 *                                                                                             *
 * 08/05/02 KM Texture filter class abstraction																			*
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "TextureFilter.h"
#include "WW3D.h"
#include "StringUtilities.h"

const char* const TextureFilterClass::TextureFilterModeString[TEXTURE_FILTER_COUNT] = {
	"None",
	"Point",
	"Bilinear",
	"Trilinear",
	"Anisotropic"
};

TextureFilterClass::TextureFilterMode TextureFilterClass::getTextureFilterMode(const char* str) {
	for (int i = 0; i < TextureFilterClass::TEXTURE_FILTER_COUNT; ++i) {
		if (WW3DString::Compare_No_Case(str, TextureFilterClass::TextureFilterModeString[i]) == 0) {
			return (TextureFilterClass::TextureFilterMode)i;
		}
	}

	return TextureFilterClass::TEXTURE_FILTER_NONE;
}

RenderBackendTextureFilter _MinTextureFilters[MAX_TEXTURE_STAGES][TextureFilterClass::FILTER_TYPE_COUNT];
RenderBackendTextureFilter _MagTextureFilters[MAX_TEXTURE_STAGES][TextureFilterClass::FILTER_TYPE_COUNT];
RenderBackendTextureFilter _MipMapFilters[MAX_TEXTURE_STAGES][TextureFilterClass::FILTER_TYPE_COUNT];

/*************************************************************************
**                             TextureFilterClass
*************************************************************************/
TextureFilterClass::TextureFilterClass(MipCountType mip_level_count)
:	TextureMinFilter(FILTER_TYPE_DEFAULT),
	TextureMagFilter(FILTER_TYPE_DEFAULT),
	UAddressMode(TEXTURE_ADDRESS_REPEAT),
	VAddressMode(TEXTURE_ADDRESS_REPEAT)
{
	if (mip_level_count!=MIP_LEVELS_1)
	{
		MipMapFilter=FILTER_TYPE_DEFAULT;
	}
	else
	{
		MipMapFilter=FILTER_TYPE_NONE;
	}
}

//**********************************************************************************************
//! Apply filters (legacy)
/*!
*/
void TextureFilterClass::Apply(unsigned int stage)
{
	WW3D::Get_Render_Backend()->Set_Texture_Filter(stage,
		RenderBackendTextureFilterType::Minification,
		_MinTextureFilters[stage][TextureMinFilter]);
	WW3D::Get_Render_Backend()->Set_Texture_Filter(stage,
		RenderBackendTextureFilterType::Magnification,
		_MagTextureFilters[stage][TextureMagFilter]);
	WW3D::Get_Render_Backend()->Set_Texture_Filter(stage,
		RenderBackendTextureFilterType::MipMap,
		_MipMapFilters[stage][MipMapFilter]);

	switch (Get_U_Addr_Mode())
	{
	case TEXTURE_ADDRESS_REPEAT:
		WW3D::Get_Render_Backend()->Set_Texture_Address_Mode(stage, true, RenderBackendTextureAddressMode::Wrap);
		break;

	case TEXTURE_ADDRESS_CLAMP:
		WW3D::Get_Render_Backend()->Set_Texture_Address_Mode(stage, true, RenderBackendTextureAddressMode::Clamp);
		break;
	}

	switch (Get_V_Addr_Mode())
	{
	case TEXTURE_ADDRESS_REPEAT:
		WW3D::Get_Render_Backend()->Set_Texture_Address_Mode(stage, false, RenderBackendTextureAddressMode::Wrap);
		break;

	case TEXTURE_ADDRESS_CLAMP:
		WW3D::Get_Render_Backend()->Set_Texture_Address_Mode(stage, false, RenderBackendTextureAddressMode::Clamp);
		break;
	}
}

//**********************************************************************************************
//! Init filters (legacy)
/*!
*/
void TextureFilterClass::_Init_Filters(TextureFilterMode texture_filter, AnisotropicFilterMode anisotropy_level)
{
	IRenderBackend *backend = WW3D::Get_Render_Backend();

	// TheSuperHackers @info Init zero stage filter defaults, point filtering is the lowest type for non mip filtering
	_MinTextureFilters[0][FILTER_TYPE_NONE]=RenderBackendTextureFilter::Point;
	_MagTextureFilters[0][FILTER_TYPE_NONE]=RenderBackendTextureFilter::Point;
	_MipMapFilters[0][FILTER_TYPE_NONE]=RenderBackendTextureFilter::None;

	// Bilinear
	_MinTextureFilters[0][FILTER_TYPE_FAST]=RenderBackendTextureFilter::Linear;
	_MagTextureFilters[0][FILTER_TYPE_FAST]=RenderBackendTextureFilter::Linear;
	_MipMapFilters[0][FILTER_TYPE_FAST]=RenderBackendTextureFilter::Point;

	// Anisotropic - MipMap interlayer filtering only goes up to linear
	_MinTextureFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Anisotropic;
	_MagTextureFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Anisotropic;
	_MipMapFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Linear;

	// TheSuperHackers @feature Mauller 08/03/2026 Add full support for all texture filtering modes;
	// None, Point, Bilinear, Trilinear, Anisotropic.
	bool FilterSupported = false;
	switch (texture_filter) {

	default:
		// TheSuperHackers @info if we have an invalid filter_type, set the filtering to none
		DEBUG_CRASH(("Invalid filter type passed into TextureFilterClass::_Init_Filters()"));
		FALLTHROUGH;

	case TEXTURE_FILTER_NONE:

		_MinTextureFilters[0][FILTER_TYPE_FAST]=RenderBackendTextureFilter::Point;
		_MagTextureFilters[0][FILTER_TYPE_FAST]=RenderBackendTextureFilter::Point;
		_MipMapFilters[0][FILTER_TYPE_FAST]=RenderBackendTextureFilter::None;

		_MinTextureFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Point;
		_MagTextureFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Point;
		_MipMapFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::None;
		break;

	case TEXTURE_FILTER_POINT:

		_MinTextureFilters[0][FILTER_TYPE_FAST]=RenderBackendTextureFilter::Point;
		_MagTextureFilters[0][FILTER_TYPE_FAST]=RenderBackendTextureFilter::Point;
		_MipMapFilters[0][FILTER_TYPE_FAST]=RenderBackendTextureFilter::Point;

		_MinTextureFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Point;
		_MagTextureFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Point;
		_MipMapFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Point;
		break;

	case TEXTURE_FILTER_BILINEAR:

		FilterSupported = backend->Supports_Texture_Filter(
			RenderBackendTextureFilterType::Minification, RenderBackendTextureFilter::Linear) &&
			backend->Supports_Texture_Filter(
				RenderBackendTextureFilterType::Magnification, RenderBackendTextureFilter::Linear);

		if (FilterSupported) {
			_MinTextureFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Linear;
			_MagTextureFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Linear;
		}
		else {
			_MinTextureFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Point;
			_MagTextureFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Point;
		}

		_MipMapFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Point;
		break;

	case TEXTURE_FILTER_TRILINEAR:

		FilterSupported = backend->Supports_Texture_Filter(
			RenderBackendTextureFilterType::Minification, RenderBackendTextureFilter::Linear) &&
			backend->Supports_Texture_Filter(
				RenderBackendTextureFilterType::Magnification, RenderBackendTextureFilter::Linear);

		if (FilterSupported) {
			_MinTextureFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Linear;
			_MagTextureFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Linear;
		}
		else {
			_MinTextureFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Point;
			_MagTextureFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Point;
		}

		if (backend->Supports_Texture_Filter(
			RenderBackendTextureFilterType::MipMap, RenderBackendTextureFilter::Linear)) {
			_MipMapFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Linear;
		}
		else {
			// TheSuperHackers @info if only linear mipmap filtering is unsupported,
			// Trilinear filtering becomes Bilinear filtering by default
			_MipMapFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Point;
		}
		break;

	case TEXTURE_FILTER_ANISOTROPIC:

		FilterSupported = backend->Supports_Texture_Filter(
			RenderBackendTextureFilterType::Magnification, RenderBackendTextureFilter::Anisotropic) &&
			backend->Supports_Texture_Filter(
				RenderBackendTextureFilterType::Minification, RenderBackendTextureFilter::Anisotropic);

		if (FilterSupported) {
			_MinTextureFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Anisotropic;
			_MagTextureFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Anisotropic;

			// Set the Anisotropic filtering level for all stages
			_Set_Max_Anisotropy(anisotropy_level);
		}
		else {
			_MinTextureFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Point;
			_MagTextureFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Point;
		}

		if (backend->Supports_Texture_Filter(
			RenderBackendTextureFilterType::MipMap, RenderBackendTextureFilter::Linear)) {
			_MipMapFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Linear;
		}
		else {
			_MipMapFilters[0][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Point;
		}
		break;

	}


	// For stages above zero, set best filter to the same as the stage zero
	int i=1;
	for (;i<MAX_TEXTURE_STAGES;++i) {
		_MinTextureFilters[i][FILTER_TYPE_NONE]=_MinTextureFilters[0][FILTER_TYPE_NONE];
		_MagTextureFilters[i][FILTER_TYPE_NONE]=_MagTextureFilters[0][FILTER_TYPE_NONE];
		_MipMapFilters[i][FILTER_TYPE_NONE]=_MipMapFilters[0][FILTER_TYPE_NONE];

		_MinTextureFilters[i][FILTER_TYPE_FAST]=_MinTextureFilters[0][FILTER_TYPE_FAST];
		_MagTextureFilters[i][FILTER_TYPE_FAST]=_MagTextureFilters[0][FILTER_TYPE_FAST];
		_MipMapFilters[i][FILTER_TYPE_FAST]=_MipMapFilters[0][FILTER_TYPE_FAST];

		// When Anisotropic filtering is used, all stages above zero use trilinear filtering
		if (_MagTextureFilters[0][FILTER_TYPE_BEST]==RenderBackendTextureFilter::Anisotropic) {
			_MagTextureFilters[i][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Linear;
		}
		else {
			_MagTextureFilters[i][FILTER_TYPE_BEST]=_MagTextureFilters[0][FILTER_TYPE_BEST];
		}

		if (_MinTextureFilters[0][FILTER_TYPE_BEST]==RenderBackendTextureFilter::Anisotropic) {
			_MinTextureFilters[i][FILTER_TYPE_BEST]=RenderBackendTextureFilter::Linear;
		}
		else {
			_MinTextureFilters[i][FILTER_TYPE_BEST]=_MinTextureFilters[0][FILTER_TYPE_BEST];
		}
		_MipMapFilters[i][FILTER_TYPE_BEST]=_MipMapFilters[0][FILTER_TYPE_BEST];

	}

	// Set default to best. The level of best filter mode is controlled by the input parameter.
	for (i=0;i<MAX_TEXTURE_STAGES;++i) {
		_MinTextureFilters[i][FILTER_TYPE_DEFAULT]=_MinTextureFilters[i][FILTER_TYPE_BEST];
		_MagTextureFilters[i][FILTER_TYPE_DEFAULT]=_MagTextureFilters[i][FILTER_TYPE_BEST];
		_MipMapFilters[i][FILTER_TYPE_DEFAULT]=_MipMapFilters[i][FILTER_TYPE_BEST];
	}

}


//**********************************************************************************************
//! Set mip mapping filter (legacy)
/*!
*/
void TextureFilterClass::Set_Mip_Mapping(FilterType mipmap)
{
//	if (mipmap != FILTER_TYPE_NONE && Get_Mip_Level_Count() <= 1 && Is_Initialized())
//	{
//		WWASSERT_PRINT(0, "Trying to enable MipMapping on texture w/o Mip levels!");
//		return;
//	}
	MipMapFilter=mipmap;
}

//**********************************************************************************************
//! Set anisotropic filter level
/*!
*/
void TextureFilterClass::_Set_Max_Anisotropy(AnisotropicFilterMode mode)
{
	for (int stage = 0; stage < MAX_TEXTURE_STAGES; ++stage)
		WW3D::Get_Render_Backend()->Set_Texture_Max_Anisotropy(stage, mode);
}

//**********************************************************************************************
//! Set default min filter (legacy)
/*!
*/
void TextureFilterClass::_Set_Default_Min_Filter(FilterType filter)
{
	for (int i=0;i<MAX_TEXTURE_STAGES;++i)
	{
		_MinTextureFilters[i][FILTER_TYPE_DEFAULT]=_MinTextureFilters[i][filter];
	}
}


//**********************************************************************************************
//! Set default mag filter (legacy)
/*!
*/
void TextureFilterClass::_Set_Default_Mag_Filter(FilterType filter)
{
	for (int i=0;i<MAX_TEXTURE_STAGES;++i)
	{
		_MagTextureFilters[i][FILTER_TYPE_DEFAULT]=_MagTextureFilters[i][filter];
	}
}

//**********************************************************************************************
//! Set default mip filter (legacy)
/*!
*/
void TextureFilterClass::_Set_Default_Mip_Filter(FilterType filter)
{
	for (int i=0;i<MAX_TEXTURE_STAGES;++i)
	{
		_MipMapFilters[i][FILTER_TYPE_DEFAULT]=_MipMapFilters[i][filter];
	}
}
