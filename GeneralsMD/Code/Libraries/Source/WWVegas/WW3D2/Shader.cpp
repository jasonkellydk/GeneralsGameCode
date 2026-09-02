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
 *                     $Archive:: /Commando/Code/ww3d2/Shader.cpp                             $*
 *                                                                                             *
 *                       Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                     $Modtime:: 12/11/01 5:15p                                              $*
 *                                                                                             *
 *                    $Revision:: 21                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   ShaderClass::Init_From_Material3 -- Initialize a shader from a Material3 structure        *
 *   ShaderClass::Apply -- Apply the renderstates for this shader                              *
 *   ShaderClass::Invert_Backface_Culling -- Globally invert the sense of all backface culling *
 *   ShaderClass::Is_Backface_Culling_Inverted -- Is backface culling inverted?                *
 *   ShaderClass::Get_SS_Category -- Helper function for static sort system                    *
 *   ShaderClass::Guess_Sort_Level -- Guess the static sort level                              *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "Shader.h"
#include "W3DFile.h"
#include "WWDebug/wwdebug.h"
#include "WW3D.h"
#include "WW3D2/Backend/RenderBackend.h"


bool ShaderClass::ShaderDirty=true;
unsigned long ShaderClass::CurrentShader=0;
bool _BackfaceCullingInverted = false;

namespace
{
	void Set_Backend_Texture_Operation(unsigned stage,
		RenderBackendTextureComponent component, RenderBackendTextureOperation operation)
	{
		WW3D::Get_Render_Backend()->Set_Texture_Operation(stage, component, operation);
	}

	void Set_Backend_Texture_Argument(unsigned stage,
		RenderBackendTextureComponent component, unsigned argument_index, RenderBackendTextureArgument argument)
	{
		WW3D::Get_Render_Backend()->Set_Texture_Argument(stage, component, argument_index, argument);
	}
}


/*
** Definitions of the preset shaders:
*/

// Texturing, zbuffer, primary gradient, no blending

#define SC_OPAQUE ( SHADE_CNST(PASS_LEQUAL, DEPTH_WRITE_ENABLE, COLOR_WRITE_ENABLE, SRCBLEND_ONE, \
	DSTBLEND_ZERO, FOG_DISABLE, GRADIENT_MODULATE, SECONDARY_GRADIENT_DISABLE, TEXTURING_ENABLE, \
	ALPHATEST_DISABLE, CULL_MODE_ENABLE, \
	DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetOpaqueShader(SC_OPAQUE);

// Texturing, zbuffer, disabled zbuffer write, primary gradient, additive blending
#define SC_ADDITIVE ( SHADE_CNST(PASS_LEQUAL, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, SRCBLEND_ONE, \
	DSTBLEND_ONE, FOG_DISABLE, GRADIENT_MODULATE, SECONDARY_GRADIENT_DISABLE, TEXTURING_ENABLE, \
	ALPHATEST_DISABLE, CULL_MODE_ENABLE, \
	DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetAdditiveShader(SC_ADDITIVE);

// Texturing, zbuffer, disabled zbuffer write, primary gradient, additive blending, bumpenvmap
#define SC_BUMPENVMAP ( SHADE_CNST(PASS_LEQUAL, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, SRCBLEND_ONE, \
	DSTBLEND_ONE, FOG_DISABLE, GRADIENT_BUMPENVMAP, SECONDARY_GRADIENT_DISABLE, TEXTURING_ENABLE, \
	ALPHATEST_DISABLE, CULL_MODE_ENABLE, \
	DETAILCOLOR_ADD, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetBumpenvmapShader(SC_BUMPENVMAP);

// Texturing, zbuffer, disabled zbuffer write, primary gradient, alpha blending
#define SC_ALPHA ( SHADE_CNST(PASS_LEQUAL, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, SRCBLEND_SRC_ALPHA, \
	DSTBLEND_ONE_MINUS_SRC_ALPHA, FOG_DISABLE, GRADIENT_MODULATE, SECONDARY_GRADIENT_DISABLE, TEXTURING_ENABLE, \
	ALPHATEST_DISABLE, CULL_MODE_ENABLE, \
	DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetAlphaShader(SC_ALPHA);

// Texturing, zbuffer, disabled zbuffer write, primary gradient, multiplicative blending
#define SC_MULTIPLICATIVE ( SHADE_CNST(PASS_LEQUAL, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, SRCBLEND_ZERO, \
	DSTBLEND_SRC_COLOR, FOG_DISABLE, GRADIENT_MODULATE, SECONDARY_GRADIENT_DISABLE, TEXTURING_ENABLE, \
	ALPHATEST_DISABLE, CULL_MODE_ENABLE, \
	DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetMultiplicativeShader(SC_MULTIPLICATIVE);

// Texturing, no zbuffer reading/writing, no gradients, no blending, no
// fogging - mostly for opaque 2D objects.
#define SC_OP_2D ( SHADE_CNST(PASS_ALWAYS, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, SRCBLEND_ONE, \
	DSTBLEND_ZERO, FOG_DISABLE, GRADIENT_DISABLE, SECONDARY_GRADIENT_DISABLE, TEXTURING_ENABLE, \
	ALPHATEST_DISABLE, CULL_MODE_ENABLE, \
	DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetOpaque2DShader(SC_OP_2D);

// Texturing, default zbuffer reading, no zbuffer writing, no gradients, no blending, no
// fogging - mostly for opaque sprite objects.
#define SC_OP_SPRITE ( SHADE_CNST(PASS_LEQUAL, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, SRCBLEND_ONE, \
	DSTBLEND_ZERO, FOG_DISABLE, GRADIENT_DISABLE, SECONDARY_GRADIENT_DISABLE, TEXTURING_ENABLE, \
	ALPHATEST_DISABLE, CULL_MODE_ENABLE, \
	DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetOpaqueSpriteShader(SC_OP_SPRITE);


// Texturing, no zbuffer reading/writing, no gradients, additive blending,
// no fogging - mostly for additive 2D objects.
#define SC_ADD_2D ( SHADE_CNST(PASS_ALWAYS, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, SRCBLEND_ONE, \
	DSTBLEND_ONE, FOG_DISABLE, GRADIENT_DISABLE, SECONDARY_GRADIENT_DISABLE, TEXTURING_ENABLE, \
	ALPHATEST_DISABLE, CULL_MODE_ENABLE, \
	DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetAdditive2DShader(SC_ADD_2D);

// Texturing, no zbuffer reading/writing, no gradients, alpha blending, no
// fogging - mostly for alpha-blended 2D objects.
#define SC_ALPHA_2D ( SHADE_CNST(PASS_ALWAYS, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, \
	SRCBLEND_SRC_ALPHA, DSTBLEND_ONE_MINUS_SRC_ALPHA, FOG_DISABLE, GRADIENT_DISABLE, \
	SECONDARY_GRADIENT_DISABLE, TEXTURING_ENABLE, \
	ALPHATEST_DISABLE, CULL_MODE_ENABLE, DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetAlpha2DShader(SC_ALPHA_2D);

// Texturing, default zbuffer reading, no zbuffer writing, no gradients,
// additive blending, no fogging - mostly for use in additive sprite
// objects.
#define SC_ADD_SPRITE ( SHADE_CNST(PASS_LEQUAL, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, \
	SRCBLEND_ONE, DSTBLEND_ONE, FOG_DISABLE, GRADIENT_DISABLE, SECONDARY_GRADIENT_DISABLE, \
	TEXTURING_ENABLE, ALPHATEST_DISABLE, CULL_MODE_ENABLE, \
	DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetAdditiveSpriteShader(SC_ADD_SPRITE);

// Texturing, default zbuffer reading, no zbuffer writing, no gradients,
// alpha blending, no fogging - mostly for use in alpha-blended sprite
// objects.
#define SC_ALPHA_SPRITE ( SHADE_CNST(PASS_LEQUAL, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, \
	SRCBLEND_SRC_ALPHA, DSTBLEND_ONE_MINUS_SRC_ALPHA, FOG_DISABLE, GRADIENT_DISABLE, \
	SECONDARY_GRADIENT_DISABLE, TEXTURING_ENABLE, \
	ALPHATEST_DISABLE, CULL_MODE_ENABLE, DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetAlphaSpriteShader(SC_ALPHA_SPRITE);

// No texturing, default zbuffer reading/writing, primary gradient, no
// blending, no fogging - mostly for use in solid-colored opaque objects.
#define SC_OPAQUE_SOLID ( SHADE_CNST(PASS_LEQUAL, DEPTH_WRITE_ENABLE, COLOR_WRITE_ENABLE, \
	SRCBLEND_ONE, DSTBLEND_ZERO, FOG_DISABLE, GRADIENT_MODULATE, SECONDARY_GRADIENT_DISABLE, \
	TEXTURING_DISABLE, ALPHATEST_DISABLE, CULL_MODE_ENABLE, \
	DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetOpaqueSolidShader(SC_OPAQUE_SOLID);

// No texturing, default zbuffer reading, no zbuffer writing, primary
// gradient, additive blending, no fogging - mostly for use in solid-colored
// additive objects.
#define SC_ADDITIVE_SOLID ( SHADE_CNST(PASS_LEQUAL, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, \
	SRCBLEND_ONE, DSTBLEND_ONE, FOG_DISABLE, GRADIENT_MODULATE, SECONDARY_GRADIENT_DISABLE, \
	TEXTURING_DISABLE, ALPHATEST_DISABLE, CULL_MODE_ENABLE, \
	DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetAdditiveSolidShader(SC_ADDITIVE_SOLID);

// No texturing, default zbuffer reading, no zbuffer writing, primary
// gradient, alpha blending, no fogging - mostly for use in solid-colored
// alpha objects.
#define SC_ALPHA_SOLID ( SHADE_CNST(PASS_LEQUAL, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, \
	SRCBLEND_SRC_ALPHA, DSTBLEND_ONE_MINUS_SRC_ALPHA, FOG_DISABLE, GRADIENT_MODULATE, \
	SECONDARY_GRADIENT_DISABLE, TEXTURING_DISABLE, \
	ALPHATEST_DISABLE, CULL_MODE_ENABLE, DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetAlphaSolidShader(SC_ALPHA_SOLID);

// Texturing, no zbuffer reading/writing, no gradients, no blending, alpha
// testing, no fogging - mostly for "pure" alpha-tested 2D objects.
#define SC_ATEST_2D ( SHADE_CNST(PASS_ALWAYS, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, \
	SRCBLEND_ONE, DSTBLEND_ZERO, FOG_DISABLE, GRADIENT_DISABLE, SECONDARY_GRADIENT_DISABLE, \
	TEXTURING_ENABLE, ALPHATEST_ENABLE, CULL_MODE_ENABLE, \
	DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetATest2DShader(SC_ATEST_2D);

// Texturing, default zbuffer reading and writing, no gradients, no
// blending, alpha testing, no fogging - mostly for "pure" alpha-tested
// sprite objects.
#define SC_ATEST_SPRITE ( SHADE_CNST(PASS_LEQUAL, DEPTH_WRITE_ENABLE, COLOR_WRITE_ENABLE, \
	SRCBLEND_ONE, DSTBLEND_ZERO, FOG_DISABLE, GRADIENT_DISABLE, SECONDARY_GRADIENT_DISABLE, \
	TEXTURING_ENABLE, ALPHATEST_ENABLE, CULL_MODE_ENABLE, \
	DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetATestSpriteShader(SC_ATEST_SPRITE);

// Texturing, no zbuffer reading/writing, no gradients, alpha blending AND
// alpha testing, no fogging - mostly for alpha-tested and blended 2D
// objects.
#define SC_ATESTBLEND_2D ( SHADE_CNST(PASS_ALWAYS, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, \
	SRCBLEND_SRC_ALPHA, DSTBLEND_ONE_MINUS_SRC_ALPHA, FOG_DISABLE, GRADIENT_DISABLE, \
	SECONDARY_GRADIENT_DISABLE, TEXTURING_ENABLE, \
	ALPHATEST_ENABLE, CULL_MODE_ENABLE, DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetATestBlend2DShader(SC_ATESTBLEND_2D);

// Texturing, default zbuffer reading and writing, no gradients, alpha
// blending AND alpha testing, no fogging - mostly for use in alpha-tested
// and blended sprite objects.
#define SC_ATESTBLEND_SPRITE ( SHADE_CNST(PASS_LEQUAL, DEPTH_WRITE_ENABLE, COLOR_WRITE_ENABLE, \
	SRCBLEND_SRC_ALPHA, DSTBLEND_ONE_MINUS_SRC_ALPHA, FOG_DISABLE, GRADIENT_DISABLE, \
	SECONDARY_GRADIENT_DISABLE, TEXTURING_ENABLE, \
	ALPHATEST_ENABLE, CULL_MODE_ENABLE, DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetATestBlendSpriteShader(SC_ATESTBLEND_SPRITE);

// Texturing, no zbuffer reading/writing, no gradients, screen blending,
// no fogging - mostly for screen-blended 2D objects.
#define SC_SCREEN_2D ( SHADE_CNST(PASS_ALWAYS, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, SRCBLEND_ONE, \
	DSTBLEND_ONE_MINUS_SRC_COLOR, FOG_DISABLE, GRADIENT_DISABLE, SECONDARY_GRADIENT_DISABLE, \
	TEXTURING_ENABLE, ALPHATEST_DISABLE, CULL_MODE_ENABLE, \
	DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetScreen2DShader(SC_SCREEN_2D);

// Texturing, default zbuffer reading, no zbuffer writing, no gradients,
// screen blending, no fogging - mostly for use in screen-blended sprite
// objects.
#define SC_SCREEN_SPRITE ( SHADE_CNST(PASS_LEQUAL, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, \
	SRCBLEND_ONE, DSTBLEND_ONE_MINUS_SRC_COLOR, FOG_DISABLE, GRADIENT_DISABLE, SECONDARY_GRADIENT_DISABLE, \
	TEXTURING_ENABLE, ALPHATEST_DISABLE, CULL_MODE_ENABLE, \
	DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetScreenSpriteShader(SC_SCREEN_SPRITE);

// Texturing, no zbuffer reading/writing, no gradients, multiplicative
// blending, no fogging - mostly for multiplicatively blended 2D objects.
#define SC_MUL_2D ( SHADE_CNST(PASS_ALWAYS, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, SRCBLEND_ZERO, \
	DSTBLEND_SRC_COLOR, FOG_DISABLE, GRADIENT_DISABLE, SECONDARY_GRADIENT_DISABLE, TEXTURING_ENABLE, \
	ALPHATEST_DISABLE, CULL_MODE_ENABLE, \
	DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetMultiplicative2DShader(SC_MUL_2D);

// Texturing, default zbuffer reading, no zbuffer writing, no gradients,
// multiplicative blending, no fogging - mostly for use in multiplicatively
// blended sprite objects.
#define SC_MUL_SPRITE ( SHADE_CNST(PASS_LEQUAL, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, \
	SRCBLEND_ZERO, DSTBLEND_SRC_COLOR, FOG_DISABLE, GRADIENT_DISABLE, SECONDARY_GRADIENT_DISABLE, \
	TEXTURING_ENABLE, ALPHATEST_DISABLE, CULL_MODE_ENABLE, \
	DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetMultiplicativeSpriteShader(SC_MUL_SPRITE);


/***********************************************************************************************
 * ShaderClass::Init_From_Material3 -- Initialize a shader from a Material3 structure          *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   4/24/2001  gth : Created.                                                                 *
 *=============================================================================================*/
void ShaderClass::Init_From_Material3(const W3dMaterial3Struct & mat3)
{
	if ( mat3.Attributes & W3DMATERIAL_USE_ALPHA ) {
		Set_Depth_Mask( DEPTH_WRITE_DISABLE );
		Set_Dst_Blend_Func( DSTBLEND_ONE_MINUS_SRC_ALPHA );
		Set_Src_Blend_Func( SRCBLEND_SRC_ALPHA );
	}
}


/***********************************************************************************************
 * ShaderClass::Enable_Fog -- Turn on fog for this shader.                                     *
 *                                                                                             *
 * Enable most appropriate fog mode (FOG_ENABLE, FOG_SCALE_FRAGMENT or FOG_WHITE) for given	  *
 *	source and destination blends.																				  *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:																												  *
 *		05/02/00    IML : Created.																					  *
 *=============================================================================================*/

void ShaderClass::Enable_Fog (const char *source)
{
	// Enable the appropriate fog mode based on shader's source and destination blends.
	switch (Get_Src_Blend_Func()) {

		case ShaderClass::SRCBLEND_ZERO:
			if (Get_Dst_Blend_Func() == ShaderClass::DSTBLEND_SRC_COLOR) {
				Set_Fog_Func (ShaderClass::FOG_WHITE);
			} else {
				Report_Unable_To_Fog (source);
			}
			break;

		case ShaderClass::SRCBLEND_ONE:
			switch (Get_Dst_Blend_Func()) {

				case ShaderClass::DSTBLEND_ZERO:							// Opaque.
					Set_Fog_Func (ShaderClass::FOG_ENABLE);
					break;

				case ShaderClass::DSTBLEND_ONE:							// Additive.
				case ShaderClass::DSTBLEND_ONE_MINUS_SRC_COLOR:		// Screen.
					Set_Fog_Func (ShaderClass::FOG_SCALE_FRAGMENT);
					break;

				default:
					Report_Unable_To_Fog (source);
					break;
			}
			break;

		case ShaderClass::SRCBLEND_SRC_ALPHA:
			if (Get_Dst_Blend_Func() == ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA) {
				Set_Fog_Func (ShaderClass::FOG_ENABLE);
			} else {
				Report_Unable_To_Fog (source);
			}
			break;

		case ShaderClass::SRCBLEND_ONE_MINUS_SRC_ALPHA:
			if (Get_Dst_Blend_Func() == ShaderClass::DSTBLEND_SRC_ALPHA) {
				Set_Fog_Func (ShaderClass::FOG_ENABLE);
			} else {
				Report_Unable_To_Fog (source);
			}
			break;
	}
}


/***********************************************************************************************
 * ShaderClass::Report_Unable_To_Fog --																		  *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:																												  *
 *		10/04/00    IML : Created.																					  *
 *=============================================================================================*/
void ShaderClass::Report_Unable_To_Fog (const char *source)
{
	#ifdef WWDEBUG
	static unsigned _reportcount = 0;

	const char		*unabletofogtext		= "WARNING: Unable to fog shader in %s with given blending mode.";
	const char		*unabletofogmoretext = "WARNING: Unable to fog additional shaders (further warnings will be suppressed).";
	const unsigned  maxreportcount		= 10;

	// Limit the no. of warning messages to some practical maximum. Suppress all subsequent warnings.
	if (_reportcount < maxreportcount) {
		WWDEBUG_SAY ((unabletofogtext, source));
		_reportcount++;
	} else {
		if (_reportcount == maxreportcount) {
			WWDEBUG_SAY ((unabletofogmoretext));
			_reportcount++;
		}
	}
	#endif
}

class Blend
{
public:

	Blend(RenderBackendBlendFactor f, bool ab)
	{
		func = f;
		useAlpha = ab;
	}

	RenderBackendBlendFactor	func;
	bool		useAlpha;
};

const Blend srcBlendLUT[ShaderClass::SRCBLEND_MAX] =
{
	Blend(RenderBackendBlendFactor::Zero, false),
	Blend(RenderBackendBlendFactor::One, false),
	Blend(RenderBackendBlendFactor::SourceAlpha, true),
 	Blend(RenderBackendBlendFactor::DestinationColor, true)
};

const Blend dstBlendLUT[ShaderClass::DSTBLEND_MAX] =
{
	Blend(RenderBackendBlendFactor::Zero, false),
	Blend(RenderBackendBlendFactor::One, false),
 	Blend(RenderBackendBlendFactor::SourceColor, false),
 	Blend(RenderBackendBlendFactor::InverseSourceColor, false),
 	Blend(RenderBackendBlendFactor::SourceAlpha, true),
 	Blend(RenderBackendBlendFactor::InverseSourceAlpha, true)
};


/***********************************************************************************************
 * ShaderClass::Apply -- Apply the renderstates for this shader                                *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   4/24/2001  gth : Created.                                                                 *
 *=============================================================================================*/
void ShaderClass::Apply()
{
	unsigned long diff;

	if (ShaderDirty)
	{
		diff=0xffffffff;
	}
	else
	{
		diff=CurrentShader^ShaderBits;
	}

	if(!diff) return;


	CurrentShader=ShaderBits;
	ShaderDirty=false;
	// COLOR MASK

	if(diff & (ShaderClass::MASK_COLORMASK | ShaderClass::MASK_SRCBLEND | ShaderClass::MASK_DSTBLEND | ShaderClass::MASK_ALPHATEST))
	{
		unsigned long planeMask = 0xffffff;

		if(Get_Color_Mask() != ShaderClass::COLOR_WRITE_ENABLE)
			planeMask = 0;

		RenderBackendBlendFactor	sf;
		RenderBackendBlendFactor	df;

		if(!planeMask)
		{
			sf = RenderBackendBlendFactor::Zero;
			df = RenderBackendBlendFactor::One;
		}
		else
		{
			sf = srcBlendLUT[ int(Get_Src_Blend_Func()) ].func;
			df = dstBlendLUT[ int(Get_Dst_Blend_Func()) ].func;
		}

		bool blendOn = false;

		if(sf != RenderBackendBlendFactor::One || df != RenderBackendBlendFactor::Zero)
		{
			WW3D::Get_Render_Backend()->Set_Blend_Factors(sf, df);
			blendOn = true;
		}
		WW3D::Get_Render_Backend()->Set_Alpha_Blend_Enabled(blendOn);

		bool alphaTest = false;

		if(Get_Alpha_Test() == ShaderClass::ALPHATEST_ENABLE)
		{
			unsigned char alphareference = 0x60;	// Alpha reference value that produces best results with mip-mapped textures.

			if(sf == RenderBackendBlendFactor::InverseSourceAlpha)
			{
				WW3D::Get_Render_Backend()->Set_Alpha_Test_Reference(0xff - alphareference);
				WW3D::Get_Render_Backend()->Set_Alpha_Test_Function(RenderBackendCompareFunction::LessEqual);
			}
			else
			{
				WW3D::Get_Render_Backend()->Set_Alpha_Test_Reference(alphareference);
				WW3D::Get_Render_Backend()->Set_Alpha_Test_Function(RenderBackendCompareFunction::GreaterEqual);
			}
			alphaTest = true;
		}
		WW3D::Get_Render_Backend()->Set_Alpha_Test_Enabled(alphaTest);

		diff &= ~(ShaderClass::MASK_COLORMASK | ShaderClass::MASK_SRCBLEND | ShaderClass::MASK_DSTBLEND | ShaderClass::MASK_ALPHATEST);
		if(!diff)
			return;
	}

	if(diff & (ShaderClass::MASK_FOG))
	{
		// Whenever fog is enabled or disabled, the entire shader is invalidated. This is why we
		// can defer the "fog enabled" check inside the "fog settings changed" check.
		if (WW3D::Get_Render_Backend()->Is_Fog_Allowed() && WW3D::Get_Render_Backend()->Is_Fog_Enabled()) {

			bool fm = false;
			unsigned fogColor = WW3D::Get_Render_Backend()->Get_Fog_Color();

			switch(Get_Fog_Func())
			{
			case ShaderClass::FOG_ENABLE:
				fm = true;
				break;
			case ShaderClass::FOG_SCALE_FRAGMENT:
				fogColor = 0;
				fm = true;
				break;
			case ShaderClass::FOG_WHITE:
				fogColor = 0xffffff;
				fm = true;
				break;
			case ShaderClass::FOG_DISABLE:
				fm = false;
				break;
			}

			WW3D::Get_Render_Backend()->Set_Fog_Enabled(fm);

			if(fm)
			{
				WW3D::Get_Render_Backend()->Set_Fog_Color(fogColor);
			}

		} else {
			WW3D::Get_Render_Backend()->Set_Fog_Enabled(false);
		}

		diff &= ~(ShaderClass::MASK_FOG);
		if(!diff)
			return;
	}

	// Defaults

	RenderBackendTextureOperation	PricOp	= RenderBackendTextureOperation::SelectArgument1;
	RenderBackendTextureArgument				PricArg1 = RenderBackendTextureArgument::Diffuse;
	RenderBackendTextureArgument				PricArg2 = RenderBackendTextureArgument::Diffuse;

	RenderBackendTextureOperation	PriaOp	 = RenderBackendTextureOperation::SelectArgument1;
	RenderBackendTextureArgument			PriaArg1 = RenderBackendTextureArgument::Diffuse;
	RenderBackendTextureArgument			PriaArg2 = RenderBackendTextureArgument::Diffuse;

	RenderBackendTextureOperation	SeccOp	 = RenderBackendTextureOperation::Disable;
	RenderBackendTextureArgument			SeccArg1 = RenderBackendTextureArgument::Texture;
	RenderBackendTextureArgument			SeccArg2 = RenderBackendTextureArgument::Current;

	RenderBackendTextureOperation	SecaOp	 = RenderBackendTextureOperation::Disable;
	RenderBackendTextureArgument			SecaArg1 = RenderBackendTextureArgument::Texture;
	RenderBackendTextureArgument			SecaArg2 = RenderBackendTextureArgument::Current;

	bool voodoo3=WW3D::Get_Render_Backend()->Is_3DFX_Voodoo3();
	int pri_mask=ShaderClass::MASK_PRIGRADIENT|ShaderClass::MASK_TEXTURING;
	int sec_mask=ShaderClass::MASK_POSTDETAILALPHAFUNC|ShaderClass::MASK_POSTDETAILCOLORFUNC|ShaderClass::MASK_TEXTURING;

	// Voodoo3s need to keep track of any changes in any of the above
	// because it shuffles the stages around
	if (voodoo3) {
		pri_mask|=sec_mask;
		sec_mask=pri_mask;
	}

	if(diff & pri_mask)
	{
		if(Get_Texturing() == ShaderClass::TEXTURING_ENABLE)
		{
			switch(Get_Primary_Gradient())
			{
			case ShaderClass::GRADIENT_DISABLE:
				//Decal
				PricOp = RenderBackendTextureOperation::SelectArgument1;
				PricArg1 = RenderBackendTextureArgument::Texture;
				PricArg2 = RenderBackendTextureArgument::Current;
				PriaOp = RenderBackendTextureOperation::SelectArgument1;
				PriaArg1 = RenderBackendTextureArgument::Texture;
				PriaArg2 = RenderBackendTextureArgument::Current;
				break;
			default:
			case ShaderClass::GRADIENT_MODULATE:
				PricOp = RenderBackendTextureOperation::Modulate;
				PricArg1 = RenderBackendTextureArgument::Texture;
				PricArg2 = RenderBackendTextureArgument::Diffuse;
				PriaOp = RenderBackendTextureOperation::Modulate;
				PriaArg1 = RenderBackendTextureArgument::Texture;
				PriaArg2 = RenderBackendTextureArgument::Diffuse;
				break;
			case ShaderClass::GRADIENT_ADD:
				//Modulate Alpha
				if(!WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::Add))
					PricOp = RenderBackendTextureOperation::Modulate;
				else
					PricOp = RenderBackendTextureOperation::Add;
				PricArg1 = RenderBackendTextureArgument::Texture;
				PricArg2 = RenderBackendTextureArgument::Diffuse;
				PriaOp = RenderBackendTextureOperation::Modulate;
				PriaArg1 = RenderBackendTextureArgument::Texture;
				PriaArg2 = RenderBackendTextureArgument::Diffuse;
				break;

			// Bump map is a hack currently as we only have two stages in use!
			case ShaderClass::GRADIENT_BUMPENVMAP:
				if(WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::BumpEnvironmentMap))
				{
					PricOp=RenderBackendTextureOperation::BumpEnvironmentMap;
					PricArg1=RenderBackendTextureArgument::Texture;
					PricArg2=RenderBackendTextureArgument::Diffuse;
					PriaOp = RenderBackendTextureOperation::Disable;
					PriaArg1 = RenderBackendTextureArgument::Texture;
					PriaArg2 = RenderBackendTextureArgument::Current;
				} else {
					PricOp = RenderBackendTextureOperation::SelectArgument1;
					PricArg1 = RenderBackendTextureArgument::Diffuse;
					PricArg2 = RenderBackendTextureArgument::Diffuse;
					PriaOp = RenderBackendTextureOperation::SelectArgument1;
					PriaArg1 = RenderBackendTextureArgument::Diffuse;
					PriaArg2 = RenderBackendTextureArgument::Diffuse;
				}
				break;

			// Bump map is a hack currently as we only have two stages in use!
			case ShaderClass::GRADIENT_BUMPENVMAPLUMINANCE:
				if(WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::BumpEnvironmentMapLuminance))
				{
					PricOp=RenderBackendTextureOperation::BumpEnvironmentMapLuminance;
					PricArg1=RenderBackendTextureArgument::Texture;
					PricArg2=RenderBackendTextureArgument::Diffuse;
					PriaOp = RenderBackendTextureOperation::Disable;
					PriaArg1 = RenderBackendTextureArgument::Texture;
					PriaArg2 = RenderBackendTextureArgument::Current;
				} else {
					PricOp = RenderBackendTextureOperation::SelectArgument1;
					PricArg1 = RenderBackendTextureArgument::Diffuse;
					PricArg2 = RenderBackendTextureArgument::Diffuse;
					PriaOp = RenderBackendTextureOperation::SelectArgument1;
					PriaArg1 = RenderBackendTextureArgument::Diffuse;
					PriaArg2 = RenderBackendTextureArgument::Diffuse;
				}
				break;

			case ShaderClass::GRADIENT_MODULATE2X:
				//Modulate Alpha
				if(!WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::Modulate2X))
					PricOp = RenderBackendTextureOperation::Modulate;
				else
					PricOp = RenderBackendTextureOperation::Modulate2X;
				PricArg1 = RenderBackendTextureArgument::Texture;
				PricArg2 = RenderBackendTextureArgument::Diffuse;
				PriaOp = RenderBackendTextureOperation::Modulate;
				PriaArg1 = RenderBackendTextureArgument::Texture;
				PriaArg2 = RenderBackendTextureArgument::Diffuse;
				break;
			}

		}
		else
		{
			switch(Get_Primary_Gradient())
			{
			case ShaderClass::GRADIENT_DISABLE:
				PricOp = RenderBackendTextureOperation::Disable;
				PricArg1 = RenderBackendTextureArgument::Texture;
				PricArg2 = RenderBackendTextureArgument::Current;
				PriaOp = RenderBackendTextureOperation::Disable;
				PriaArg1 = RenderBackendTextureArgument::Texture;
				PriaArg2 = RenderBackendTextureArgument::Current;
				break;
			default:
			case ShaderClass::GRADIENT_MODULATE:
				PricOp = RenderBackendTextureOperation::SelectArgument2;
				PricArg1 = RenderBackendTextureArgument::Texture;
				PricArg2 = RenderBackendTextureArgument::Diffuse;
				PriaOp = RenderBackendTextureOperation::SelectArgument2;
				PriaArg1 = RenderBackendTextureArgument::Texture;
				PriaArg2 = RenderBackendTextureArgument::Diffuse;
				break;
			case ShaderClass::GRADIENT_ADD:
				PricOp = RenderBackendTextureOperation::SelectArgument2;
				PricArg1 = RenderBackendTextureArgument::Texture;
				PricArg2 = RenderBackendTextureArgument::Diffuse;
				PriaOp = RenderBackendTextureOperation::SelectArgument2;
				PriaArg1 = RenderBackendTextureArgument::Texture;
				PriaArg2 = RenderBackendTextureArgument::Diffuse;
				break;
			}
		}
	}

	if(diff & sec_mask)
	{
		if(Get_Texturing()== ShaderClass::TEXTURING_ENABLE)
		{
			switch(Get_Post_Detail_Color_Func())
			{
			default:
			case ShaderClass::DETAILCOLOR_DISABLE:
				break;

			case ShaderClass::DETAILCOLOR_DETAIL:
				if(WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::SelectArgument1))
				{
					SeccOp = RenderBackendTextureOperation::SelectArgument1;
					SeccArg1 = RenderBackendTextureArgument::Texture;
					SeccArg2 = RenderBackendTextureArgument::Current;
				}
				else {
					SNAPSHOT_SAY(("Warning: Using unsupported texture op: SELECTARG1"));
				}
				break;

			case ShaderClass::DETAILCOLOR_SCALE:
				if(WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::Modulate))
				{
					SeccOp = RenderBackendTextureOperation::Modulate;
					SeccArg1 = RenderBackendTextureArgument::Texture;
					SeccArg2 = RenderBackendTextureArgument::Current;
				}
				else {
					SNAPSHOT_SAY(("Warning: Using unsupported texture op: MODULATE"));
				}
				break;

			case ShaderClass::DETAILCOLOR_INVSCALE:
				if(WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::AddSmooth))
				{
					SeccOp = RenderBackendTextureOperation::AddSmooth;
					SeccArg1 = RenderBackendTextureArgument::Texture;
					SeccArg2 = RenderBackendTextureArgument::Current;
				} else if(WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::Add)) {
					SeccOp = RenderBackendTextureOperation::Add;
					SeccArg1 = RenderBackendTextureArgument::Texture;
					SeccArg2 = RenderBackendTextureArgument::Current;
				}
				else {
					SNAPSHOT_SAY(("Warning: Using unsupported texture op: ADDSMOOTH"));
				}
				break;

			case ShaderClass::DETAILCOLOR_ADD:
				if(WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::Add))
				{
					SeccOp = RenderBackendTextureOperation::Add;
					SeccArg1 = RenderBackendTextureArgument::Texture;
					SeccArg2 = RenderBackendTextureArgument::Current;
				}
				else {
					SNAPSHOT_SAY(("Warning: Using unsupported texture op: ADD"));
				}
				break;

			case ShaderClass::DETAILCOLOR_SUB:
				if(WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::Subtract))
				{
					SeccOp = RenderBackendTextureOperation::Subtract;
					SeccArg1 = RenderBackendTextureArgument::Texture;
					SeccArg2 = RenderBackendTextureArgument::Current;
				}
				else {
					SNAPSHOT_SAY(("Warning: Using unsupported texture op: SUBTRACT"));
				}
				break;

			case ShaderClass::DETAILCOLOR_SUBR:
				if(WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::Subtract))
				{
					SeccOp = RenderBackendTextureOperation::Subtract;
					SeccArg1 = RenderBackendTextureArgument::Current;
					SeccArg2 = RenderBackendTextureArgument::Texture;
				}
				else {
					SNAPSHOT_SAY(("Warning: Using unsupported texture op: SUBTRACT"));
				}
				break;

			case ShaderClass::DETAILCOLOR_BLEND:
				if(WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::BlendTextureAlpha))
				{
					SeccOp = RenderBackendTextureOperation::BlendTextureAlpha;
					SeccArg1 = RenderBackendTextureArgument::Texture;
					SeccArg2 = RenderBackendTextureArgument::Current;
				}
				else {
					SNAPSHOT_SAY(("Warning: Using unsupported texture op: BLENDTEXTUREALPHA"));
				}
				break;

			case ShaderClass::DETAILCOLOR_DETAILBLEND:
				if(WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::BlendCurrentAlpha))
				{
					SeccOp = RenderBackendTextureOperation::BlendCurrentAlpha;
					SeccArg1 = RenderBackendTextureArgument::Texture;
					SeccArg2 = RenderBackendTextureArgument::Current;
				}
				else {
					SNAPSHOT_SAY(("Warning: Using unsupported texture op: BLENDCURRENTALPHA"));
				}
				break;

			case ShaderClass::DETAILCOLOR_ADDSIGNED:
				if (WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::AddSigned)) {
					SeccOp = RenderBackendTextureOperation::AddSigned;
					SeccArg1 = RenderBackendTextureArgument::Texture;
					SeccArg2 = RenderBackendTextureArgument::Current;
				}  else if (WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::Add)) {
					SeccOp = RenderBackendTextureOperation::Add;
					SeccArg1 = RenderBackendTextureArgument::Texture;
					SeccArg2 = RenderBackendTextureArgument::Current;
				} else {
					SNAPSHOT_SAY(("Warning: Using unsupported texture op: ADDSIGNED"));
				}
				break;

			case ShaderClass::DETAILCOLOR_ADDSIGNED2X:
				if (WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::AddSigned2X)) {
					SeccOp = RenderBackendTextureOperation::AddSigned2X;
					SeccArg1 = RenderBackendTextureArgument::Texture;
					SeccArg2 = RenderBackendTextureArgument::Current;
				} else if (WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::AddSigned)) {
					SeccOp = RenderBackendTextureOperation::AddSigned;
					SeccArg1 = RenderBackendTextureArgument::Texture;
					SeccArg2 = RenderBackendTextureArgument::Current;
				}  else if (WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::Add)) {
					SeccOp = RenderBackendTextureOperation::Add;
					SeccArg1 = RenderBackendTextureArgument::Texture;
					SeccArg2 = RenderBackendTextureArgument::Current;
				} else {
					SNAPSHOT_SAY(("Warning: Using unsupported texture op: ADDSIGNED2X"));
				}
				break;

			case ShaderClass::DETAILCOLOR_SCALE2X:
				if(WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::Modulate2X)) {
					SeccOp = RenderBackendTextureOperation::Modulate2X;
					SeccArg1 = RenderBackendTextureArgument::Texture;
					SeccArg2 = RenderBackendTextureArgument::Current;
				} else if(WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::Modulate)) {
					SeccOp = RenderBackendTextureOperation::Modulate;
					SeccArg1 = RenderBackendTextureArgument::Texture;
					SeccArg2 = RenderBackendTextureArgument::Current;
				}
				else {
					SNAPSHOT_SAY(("Warning: Using unsupported texture op: MODULATE2X"));
				}
				break;

			case ShaderClass::DETAILCOLOR_MODALPHAADDCOLOR:
				if (WW3D::Get_Render_Backend()->Supports_Modulate_Alpha_Add_Color()) {
					SeccOp = RenderBackendTextureOperation::ModulateAlphaAddColor;
					SeccArg1 = RenderBackendTextureArgument::Current;
					SeccArg2 = RenderBackendTextureArgument::Texture;
				} else if (WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::Add)) {
					SeccOp = RenderBackendTextureOperation::Add;
					SeccArg1 = RenderBackendTextureArgument::Texture;
					SeccArg2 = RenderBackendTextureArgument::Current;
				} else {
					SNAPSHOT_SAY(("Warning: Using unsupported texture op: MODULATEALPHA_ADDCOLOR"));
				}
				break;
			}

			switch(Get_Post_Detail_Alpha_Func())
			{
			default:
			case ShaderClass::DETAILALPHA_DISABLE:
				break;

			case ShaderClass::DETAILALPHA_DETAIL:
				if(WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::SelectArgument1))
				{
					SecaOp = RenderBackendTextureOperation::SelectArgument1;
					SecaArg1 = RenderBackendTextureArgument::Texture;
					SecaArg2 = RenderBackendTextureArgument::Current;
				}
				else {
					SNAPSHOT_SAY(("Warning: Using unsupported texture op: SELECTARG1"));
				}
				break;

			case ShaderClass::DETAILALPHA_SCALE:
				if(WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::Modulate))
				{
					SecaOp = RenderBackendTextureOperation::Modulate;
					SecaArg1 = RenderBackendTextureArgument::Texture;
					SecaArg2 = RenderBackendTextureArgument::Current;
				}
				else {
					SNAPSHOT_SAY(("Warning: Using unsupported texture op: MODULATE"));
				}
				break;

			case ShaderClass::DETAILALPHA_INVSCALE:
				if(WW3D::Get_Render_Backend()->Supports_Texture_Operation(RenderBackendTextureOperation::AddSmooth))
				{
					SecaOp = RenderBackendTextureOperation::AddSmooth;
					SecaArg1 = RenderBackendTextureArgument::Texture;
					SecaArg2 = RenderBackendTextureArgument::Current;
				}
				else {
					SNAPSHOT_SAY(("Warning: Using unsupported texture op: ADDSMOOTH"));
				}
				break;
			}

			// if color is enabled and alpha is disabled set to pass alpha through
			if ((SeccOp!=RenderBackendTextureOperation::Disable) && (SecaOp==RenderBackendTextureOperation::Disable)) {
				SecaOp = RenderBackendTextureOperation::SelectArgument2;
				SecaArg2 = RenderBackendTextureArgument::Current;
			} else if ((SeccOp==RenderBackendTextureOperation::Disable) && (SecaOp!=RenderBackendTextureOperation::Disable)) {
				SeccOp = RenderBackendTextureOperation::SelectArgument2;
				SeccArg2 = RenderBackendTextureArgument::Current;
			}
		}
	}

	bool kill_stage_2=false;

	// Apply the stage settings
	if (diff & pri_mask) {
		// for voodoo3 supported blend modes, the stage 0 color and alpha are both diffuse
		// or both not, so we can check for color diffuse only
		if ( voodoo3 && (PricArg2==RenderBackendTextureArgument::Diffuse) &&
			  ( (SecaOp!=RenderBackendTextureOperation::Disable) || (SeccOp!=RenderBackendTextureOperation::Disable) )
			) {
			// Special Voodoo3 code
			// If stage 0 has a diffuse input
			// and stage 1 has an input put the diffuse in stage 2

			RenderBackendTextureArgument tex_arg=RenderBackendTextureArgument::Current;
			if(Get_Texturing() == ShaderClass::TEXTURING_ENABLE) {
				tex_arg=RenderBackendTextureArgument::Texture;
			}

			// this is for the bad case of using
			// stage 0 for diffuse only
			if ((PricOp==RenderBackendTextureOperation::SelectArgument1)&&(PricArg1==RenderBackendTextureArgument::Diffuse)) {
				WWDEBUG_SAY(("Wasted Stage 0 in shader-vertex diffuse only"));
				// set stage 0 to disable
				Set_Backend_Texture_Operation(0,RenderBackendTextureComponent::Color,RenderBackendTextureOperation::Disable);
				Set_Backend_Texture_Operation(0,RenderBackendTextureComponent::Alpha,RenderBackendTextureOperation::Disable);
				// set stage 1 to accept diffuse
				if (SeccArg2==RenderBackendTextureArgument::Current) SeccArg2=RenderBackendTextureArgument::Diffuse;
				if (SecaArg2==RenderBackendTextureArgument::Current) SecaArg2=RenderBackendTextureArgument::Diffuse;
				// and nuke stage 2
				kill_stage_2=true;
			} else {
				// set stage 0 to pass through what it needs
				Set_Backend_Texture_Operation(0,RenderBackendTextureComponent::Color,RenderBackendTextureOperation::SelectArgument1);
				Set_Backend_Texture_Argument(0,RenderBackendTextureComponent::Color,1,tex_arg);
				Set_Backend_Texture_Operation(0,RenderBackendTextureComponent::Alpha,RenderBackendTextureOperation::SelectArgument1);
				Set_Backend_Texture_Argument(0,RenderBackendTextureComponent::Alpha,1,tex_arg);

				// set stage 2 to do the diffuse op
				// bypass the wrapper since it only supports 2 texture stages
				Set_Backend_Texture_Operation(2,RenderBackendTextureComponent::Color,PricOp);
				Set_Backend_Texture_Argument(2,RenderBackendTextureComponent::Color,1,RenderBackendTextureArgument::Current);
				Set_Backend_Texture_Argument(2,RenderBackendTextureComponent::Color,2,RenderBackendTextureArgument::Diffuse);
				Set_Backend_Texture_Operation(2,RenderBackendTextureComponent::Alpha,PriaOp);
				Set_Backend_Texture_Argument(2,RenderBackendTextureComponent::Alpha,1,RenderBackendTextureArgument::Current);
				Set_Backend_Texture_Argument(2,RenderBackendTextureComponent::Alpha,2,RenderBackendTextureArgument::Diffuse);
				WW3D::Get_Render_Backend()->Set_Texture_Coordinate_Source(2,RenderBackendTextureCoordinateSource::PassThrough);
				WW3D::Get_Render_Backend()->Set_Texture(2,nullptr);
				kill_stage_2=false;
				ShaderDirty=true;
			}
		} else {

#pragma message("(gth) Generals added a feature here WW3D::Is_Coloring_Enabled() which needs to be merged properly")
#if 0
			if (WW3D::Is_Coloring_Enabled())
			{
				cArg2=aArg2=RenderBackendTextureArgument::TextureFactor;
				cOp=aOp=RenderBackendTextureOperation::SelectArgument2;
			}
#endif
			Set_Backend_Texture_Operation(0,RenderBackendTextureComponent::Color,PricOp);
			Set_Backend_Texture_Argument(0,RenderBackendTextureComponent::Color,1,PricArg1);
			Set_Backend_Texture_Argument(0,RenderBackendTextureComponent::Color,2,PricArg2);
			Set_Backend_Texture_Operation(0,RenderBackendTextureComponent::Alpha,PriaOp);
			Set_Backend_Texture_Argument(0,RenderBackendTextureComponent::Alpha,1,PriaArg1);
			Set_Backend_Texture_Argument(0,RenderBackendTextureComponent::Alpha,2,PriaArg2);
			kill_stage_2=true;
		}
		diff &= ~(ShaderClass::MASK_PRIGRADIENT);
	}

	if (diff & sec_mask) {
		Set_Backend_Texture_Operation(1,RenderBackendTextureComponent::Color,SeccOp);
		Set_Backend_Texture_Argument(1,RenderBackendTextureComponent::Color,1,SeccArg1);
		Set_Backend_Texture_Argument(1,RenderBackendTextureComponent::Color,2,SeccArg2);
		Set_Backend_Texture_Operation(1,RenderBackendTextureComponent::Alpha,SecaOp);
		Set_Backend_Texture_Argument(1,RenderBackendTextureComponent::Alpha,1,SecaArg1);
		Set_Backend_Texture_Argument(1,RenderBackendTextureComponent::Alpha,2,SecaArg2);
		diff &= ~(ShaderClass::MASK_POSTDETAILCOLORFUNC);
		diff &= ~(ShaderClass::MASK_POSTDETAILALPHAFUNC);
		diff &= ~(ShaderClass::MASK_TEXTURING);
	}

	// Make sure to disable stage 2 for voodoos since we don't have state tracking for
	// stage 2
	// bypass the wrapper since it only supports 2 texture stages
	if (voodoo3 && kill_stage_2) {
		if ((SeccOp!=RenderBackendTextureOperation::Disable)&&(SecaOp!=RenderBackendTextureOperation::Disable)) {
			Set_Backend_Texture_Operation(2,RenderBackendTextureComponent::Color,RenderBackendTextureOperation::SelectArgument1);
			Set_Backend_Texture_Argument(2,RenderBackendTextureComponent::Color,1,RenderBackendTextureArgument::Current);
			Set_Backend_Texture_Operation(2,RenderBackendTextureComponent::Alpha,RenderBackendTextureOperation::SelectArgument1);
			Set_Backend_Texture_Argument(2,RenderBackendTextureComponent::Alpha,1,RenderBackendTextureArgument::Current);
		} else {
			Set_Backend_Texture_Operation(2,RenderBackendTextureComponent::Color,RenderBackendTextureOperation::Disable);
			Set_Backend_Texture_Operation(2,RenderBackendTextureComponent::Alpha,RenderBackendTextureOperation::Disable);
		}
		WW3D::Get_Render_Backend()->Set_Texture_Coordinate_Source(2,RenderBackendTextureCoordinateSource::PassThrough);
		WW3D::Get_Render_Backend()->Set_Texture(2,nullptr);
	}

	if(!diff)
		return;

	WW3D::Get_Render_Backend()->Set_Specular_Enabled(Get_Secondary_Gradient() != 0);

	// DEPTH COMPARE FUNCTION
	WW3D::Get_Render_Backend()->Set_Depth_Function(
		static_cast<RenderBackendCompareFunction>(Get_Depth_Compare()));

	// DEPTH MASK
	WW3D::Get_Render_Backend()->Set_Depth_Write_Enabled(Get_Depth_Mask() != 0);

	// DITHERING

	// CULLMODE
	RenderBackendCullMode cull_mode = RenderBackendCullMode::None;
	if (Get_Cull_Mode())
	{
		cull_mode = _BackfaceCullingInverted
			? RenderBackendCullMode::CounterClockwise
			: RenderBackendCullMode::Clockwise;
	}
	WW3D::Get_Render_Backend()->Set_Cull_Mode(cull_mode);

	// NPATCHES
	if (diff&ShaderClass::MASK_NPATCHENABLE) {
		float level=1.0f;
		if (Get_NPatch_Enable()) level=float(WW3D::Get_NPatches_Level());
		WW3D::Get_Render_Backend()->Set_NPatch_Segments(level);
	}

	// Enable/disable alpha test
	WW3D::Get_Render_Backend()->Set_Alpha_Test_Enabled(Get_Alpha_Test() != 0);

	// Enable/disable stencil test
	// Not supported yet
}


/***********************************************************************************************
 * ShaderClass::Invert_Backface_Culling -- Globally invert the sense of all backface culling   *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   4/24/2001  gth : Created.                                                                 *
 *=============================================================================================*/
void ShaderClass::Invert_Backface_Culling(bool onoff)
{
	if (onoff == true) {
		_BackfaceCullingInverted = true;
	} else {
		_BackfaceCullingInverted = false;
	}
	Invalidate();
}


/***********************************************************************************************
 * ShaderClass::Get_SS_Category -- Helper function for static sort system                      *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   7/13/2001  hy : Created.                                                                  *
 *=============================================================================================*/
ShaderClass::StaticSortCategoryType ShaderClass::Get_SS_Category() const
{
	// category: Opaque
	if ( (ALPHATEST_DISABLE==Get_Alpha_Test()) && (DSTBLEND_ZERO==Get_Dst_Blend_Func()) )
		return SSCAT_OPAQUE;
	// category: Alpha Test
	if (ALPHATEST_ENABLE==Get_Alpha_Test())	{
		if (DSTBLEND_ZERO==Get_Dst_Blend_Func()) return SSCAT_ALPHA_TEST;
		if ( (SRCBLEND_SRC_ALPHA==Get_Src_Blend_Func()) &&
			  (DSTBLEND_ONE_MINUS_SRC_ALPHA==Get_Dst_Blend_Func()) )
			  return SSCAT_ALPHA_TEST;
	}
	// category: Additive
	if ( (SRCBLEND_ONE==Get_Src_Blend_Func()) && (DSTBLEND_ONE==Get_Dst_Blend_Func()) )
		return SSCAT_ADDITIVE;
	// category: Screen
	if ( (SRCBLEND_ONE==Get_Src_Blend_Func()) && (DSTBLEND_ONE_MINUS_SRC_COLOR==Get_Dst_Blend_Func()) )
		return SSCAT_SCREEN;
	// category: Everything else
	return SSCAT_OTHER;
}


/***********************************************************************************************
 * ShaderClass::Guess_Sort_Level -- Guess the static sort level                                *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   8/27/2001  hy : Created.                                                                  *
 *=============================================================================================*/
int ShaderClass::Guess_Sort_Level() const
{
	int sort_level;
	StaticSortCategoryType scat=Get_SS_Category();

	switch (scat)
	{
	case ShaderClass::SSCAT_OPAQUE:
	case ShaderClass::SSCAT_ALPHA_TEST:
		sort_level=SORT_LEVEL_NONE;
		break;
	case ShaderClass::SSCAT_SCREEN:
		sort_level=SORT_LEVEL_BIN2;
		break;
	case ShaderClass::SSCAT_ADDITIVE:
		sort_level=SORT_LEVEL_BIN3;
		break;
	default:
		sort_level=SORT_LEVEL_BIN1;
		break;
	}
	return sort_level;
}

/***********************************************************************************************
 * ShaderClass::Is_Backface_Culling_Inverted -- Is backface culling inverted?                  *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   4/24/2001  gth : Created.                                                                 *
 *=============================================================================================*/
bool ShaderClass::Is_Backface_Culling_Inverted()
{
	return _BackfaceCullingInverted;
}

const StringClass& ShaderClass::Get_Description(StringClass& str) const
{
	str="";
	switch (Get_Depth_Compare()) {
	case PASS_NEVER: str+="DEPTH_COMPARE:PASS_NEVER | "; break;
	case PASS_LESS: str+="DEPTH_COMPARE:PASS_LESS | "; break;
	case PASS_EQUAL: str+="DEPTH_COMPARE:PASS_EQUAL | "; break;
	case PASS_LEQUAL: str+="DEPTH_COMPARE:PASS_LEQUAL | "; break;
	case PASS_GREATER: str+="DEPTH_COMPARE:PASS_GREATER | "; break;
	case PASS_NOTEQUAL: str+="DEPTH_COMPARE:PASS_NOTEQUAL | "; break;
	case PASS_GEQUAL: str+="DEPTH_COMPARE:PASS_GEQUAL | "; break;
	case PASS_ALWAYS: str+="DEPTH_COMPARE:PASS_ALWAYS | "; break;
	}

	switch (Get_Depth_Mask()) {
	case DEPTH_WRITE_DISABLE: str+="DEPTH_WRITE_DISABLE | "; break;
	case DEPTH_WRITE_ENABLE: str+="DEPTH_WRITE_ENABLE | "; break;
	}

	switch (Get_Color_Mask()) {
	case COLOR_WRITE_DISABLE: str+="COLOR_WRITE_DISABLE | "; break;
	case COLOR_WRITE_ENABLE: str+="COLOR_WRITE_ENABLE | "; break;
	}

	switch (Get_Dst_Blend_Func()) {
	case DSTBLEND_ZERO: str+="DSTBLEND_ZERO | "; break;
  	case DSTBLEND_ONE: str+="DSTBLEND_ONE | "; break;
 	case DSTBLEND_SRC_COLOR: str+="DSTBLEND_SRC_COLOR | "; break;
 	case DSTBLEND_ONE_MINUS_SRC_COLOR: str+="DSTBLEND_ONE_MINUS_SRC_COLOR | "; break;
 	case DSTBLEND_SRC_ALPHA: str+="DSTBLEND_SRC_ALPHA | "; break;
 	case DSTBLEND_ONE_MINUS_SRC_ALPHA: str+="DSTBLEND_ONE_MINUS_SRC_ALPHA | "; break;
	}

	switch (Get_Fog_Func()) {
	case FOG_DISABLE: str+="FOG_DISABLE | "; break;
 	case FOG_ENABLE: str+="FOG_ENABLE | "; break;
 	case FOG_SCALE_FRAGMENT: str+="FOG_SCALE_FRAGMENT | "; break;
 	case FOG_WHITE: str+="FOG_WHITE | "; break;
	}

	switch (Get_Primary_Gradient()) {
 	case GRADIENT_DISABLE: str+="GRADIENT_DISABLE | "; break;
	case GRADIENT_MODULATE: str+="GRADIENT_MODULATE | "; break;
	case GRADIENT_ADD: str+="GRADIENT_ADD | "; break;
	case GRADIENT_BUMPENVMAP: str+="GRADIENT_BUMPENVMAP | "; break;
	case GRADIENT_BUMPENVMAPLUMINANCE: str+="GRADIENT_BUMPENVMAPLUMINANCE | "; break;
	case GRADIENT_MODULATE2X: str+="GRADIENT_MODULATE2X | "; break;
	}

	switch (Get_Secondary_Gradient()) {
	case SECONDARY_GRADIENT_DISABLE: str+="SECONDARY_GRADIENT_DISABLE | "; break;
	case SECONDARY_GRADIENT_ENABLE: str+="SECONDARY_GRADIENT_ENABLE | "; break;
	}

	switch (Get_Src_Blend_Func()) {
  	case SRCBLEND_ZERO: str+="SRCBLEND_ZERO | "; break;
  	case SRCBLEND_ONE: str+="SRCBLEND_ONE | "; break;
 	case SRCBLEND_SRC_ALPHA: str+="SRCBLEND_SRC_ALPHA | "; break;
 	case SRCBLEND_ONE_MINUS_SRC_ALPHA: str+="SRCBLEND_ONE_MINUS_SRC_ALPHA | "; break;
	}

	switch (Get_Texturing()) {
	case TEXTURING_DISABLE: str+="TEXTURING_DISABLE | "; break;
	case TEXTURING_ENABLE: str+="TEXTURING_ENABLE | "; break;
	}

	switch (Get_NPatch_Enable()) {
	case NPATCH_DISABLE: str+="NPATCH_DISABLE | "; break;
	case NPATCH_ENABLE: str+="NPATCH_ENABLE | "; break;
	}

	switch (Get_Alpha_Test()) {
	case ALPHATEST_DISABLE: str+="ALPHATEST_DISABLE | "; break;
	case ALPHATEST_ENABLE: str+="ALPHATEST_ENABLE | "; break;
	}

	switch (Get_Cull_Mode()) {
	case CULL_MODE_DISABLE: str+="CULL_MODE_DISABLE | "; break;
	case CULL_MODE_ENABLE: str+="CULL_MODE_ENABLE | "; break;
	}

	switch (Get_Post_Detail_Color_Func()) {
	case DETAILCOLOR_DISABLE: str+="DETAILCOLOR_DISABLE"; break;
	case DETAILCOLOR_DETAIL: str+="DETAILCOLOR_DETAIL"; break;
	case DETAILCOLOR_SCALE: str+="DETAILCOLOR_SCALE"; break;
	case DETAILCOLOR_INVSCALE: str+="DETAILCOLOR_INVSCALE"; break;
	case DETAILCOLOR_ADD: str+="DETAILCOLOR_ADD"; break;
	case DETAILCOLOR_SUB: str+="DETAILCOLOR_SUB"; break;
	case DETAILCOLOR_SUBR: str+="DETAILCOLOR_SUBR"; break;
	case DETAILCOLOR_BLEND: str+="DETAILCOLOR_BLEND"; break;
	case DETAILCOLOR_DETAILBLEND: str+="DETAILCOLOR_DETAILBLEND"; break;
	case DETAILCOLOR_ADDSIGNED: str+="DETAILCOLOR_ADDSIGNED"; break;
	case DETAILCOLOR_ADDSIGNED2X: str+="DETAILCOLOR_ADDSIGNED2X"; break;
	case DETAILCOLOR_SCALE2X: str+="DETAILCOLOR_SCALE2X"; break;
	case DETAILCOLOR_MODALPHAADDCOLOR: str+="DETAILCOLOR_MODALPHAADDCOLOR"; break;
	}
	return str;
}

