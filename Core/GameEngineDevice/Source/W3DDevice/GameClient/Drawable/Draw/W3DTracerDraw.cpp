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

// FILE: W3DTracerDraw.cpp ////////////////////////////////////////////////////////////////////////
// Author: Colin Day, December 2001
// Desc:   Tracer drawing
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include <stdlib.h>

import Graphics.Scene.Beams;

#include "Common/Thing.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameLogic/GameLogic.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/Module/W3DTracerDraw.h"
#include "WW3D2/Line3D.h"
#include "W3DDevice/GameClient/W3DScene.h"

namespace
{
Graphics::BeamDesc Make_Modern_Beam(const Vector3 &start, const Vector3 &end, Real width, const RGBColor &color, Real opacity) noexcept
{
	Graphics::BeamDesc description;
	description.start = {start.X, start.Y, start.Z};
	description.end = {end.X, end.Y, end.Z};
	description.width = width;
	description.color = {color.red, color.green, color.blue, 1.0f};
	description.opacity = opacity;
	description.flags = width > 0.0f && opacity > 0.0f ? Graphics::BeamFlags::Enabled : Graphics::BeamFlags::None;
	return description;
}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
W3DTracerDraw::W3DTracerDraw( Thing *thing, const ModuleData* moduleData ) : DrawModule( thing, moduleData )
{

	// set opacity
	m_opacity = 1.0f;
	m_length = 20.0f;
	m_width = 0.5f;
	m_color.red = 0.9f;
	m_color.green = 0.8f;
	m_color.blue = 0.7f;
	m_speedInDistPerFrame = 1.0f;
	m_theTracer = nullptr;
	m_modernBeam = {};
	m_modernTransformValid = FALSE;

}

void W3DTracerDraw::createLegacyTracer(const Matrix3D& transform)
{
	if (m_theTracer != nullptr)
		return;

	const Vector3 start(0.0f, 0.0f, 0.0f);
	const Vector3 stop(m_length, 0.0f, 0.0f);
	m_theTracer = NEW Line3DClass(
		start,
		stop,
		m_width,
		m_color.red,
		m_color.green,
		m_color.blue,
		m_opacity);
	W3DDisplay::m_3DScene->Add_Render_Object(m_theTracer);
	m_theTracer->Set_Transform(transform);
}

bool W3DTracerDraw::updateModernTracer() noexcept
{
	if (!m_modernBeam.Is_Valid() || !m_modernTransformValid)
		return false;

	const Vector3 localStart(0.0f, 0.0f, 0.0f);
	const Vector3 localEnd(m_length, 0.0f, 0.0f);
	Vector3 start;
	Vector3 end;
	Matrix3D::Transform_Vector(m_modernTransform, localStart, &start);
	Matrix3D::Transform_Vector(m_modernTransform, localEnd, &end);
	return Graphics::UpdateBeam(m_modernBeam, Make_Modern_Beam(start, end, m_width, m_color, m_opacity));
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DTracerDraw::setTracerParms(Real speed, Real length, Real width, const RGBColor& color, Real initialOpacity)
{
	m_speedInDistPerFrame = speed;
	m_length = length;
	m_width = width;
	m_color = color;
	m_opacity = initialOpacity;
	if (m_modernBeam.Is_Valid()) {
		if (!updateModernTracer()) {
			Graphics::DestroyBeam(m_modernBeam);
			m_modernBeam = {};
			createLegacyTracer(m_modernTransform);
		}
	}
	if (m_theTracer)
	{
		Vector3 start( 0.0f, 0.0f, 0.0f );
		Vector3 stop( m_length, 0.0f, 0.0f );
		m_theTracer->Reset(start, stop, m_width);
		m_theTracer->Re_Color(m_color.red, m_color.green, m_color.blue);
		m_theTracer->Set_Opacity( m_opacity );
		// these calls nuke the internal transform, so re-set it here
		m_theTracer->Set_Transform( *getDrawable()->getTransformMatrix() );
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
W3DTracerDraw::~W3DTracerDraw()
{
	if (m_modernBeam.Is_Valid())
		Graphics::DestroyBeam(m_modernBeam);

	// remove tracer from the scene and delete
	if( m_theTracer )
	{
		W3DDisplay::m_3DScene->Remove_Render_Object( m_theTracer );
		REF_PTR_RELEASE( m_theTracer );
	}
}

//-------------------------------------------------------------------------------------------------
void W3DTracerDraw::reactToTransformChange( const Matrix3D *oldMtx,
																							 const Coord3D *oldPos,
																							 Real oldAngle )
{
	if (m_modernTransformValid)
		m_modernTransform = *getDrawable()->getTransformMatrix();
	if (m_modernBeam.Is_Valid() && !updateModernTracer()) {
		Graphics::DestroyBeam(m_modernBeam);
		m_modernBeam = {};
		createLegacyTracer(m_modernTransform);
	}
	if( m_theTracer )
		m_theTracer->Set_Transform( *getDrawable()->getTransformMatrix() );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DTracerDraw::doDrawModule(const Matrix3D* transformMtx)
{

	if (m_modernTransformValid == FALSE) {
		m_modernTransform = *transformMtx;
		m_modernTransformValid = TRUE;
	}

	// create tracer
	if (m_theTracer == nullptr && !m_modernBeam.Is_Valid())
	{
		const Vector3 localStart(0.0f, 0.0f, 0.0f);
		const Vector3 localEnd(m_length, 0.0f, 0.0f);
		Vector3 start;
		Vector3 end;
		Matrix3D::Transform_Vector(m_modernTransform, localStart, &start);
		Matrix3D::Transform_Vector(m_modernTransform, localEnd, &end);
		m_modernBeam = Graphics::CreateBeam(Make_Modern_Beam(start, end, m_width, m_color, m_opacity));
		if (!m_modernBeam.Is_Valid())
			createLegacyTracer(*transformMtx);
	}

	UnsignedInt expDate = getDrawable()->getExpirationDate();
	if (expDate != 0)
	{
		Real decay = m_opacity / (expDate - TheGameLogic->getFrame());
		m_opacity -= decay;
		if (m_theTracer)
			m_theTracer->Set_Opacity( m_opacity );
	}

	// set the position for the tracer
	if (m_speedInDistPerFrame != 0.0f)
	{
		if (m_modernBeam.Is_Valid()) {
			m_modernTransform.Translate(Vector3(m_speedInDistPerFrame, 0.0f, 0.0f));
			if (!updateModernTracer()) {
				Graphics::DestroyBeam(m_modernBeam);
				m_modernBeam = {};
				createLegacyTracer(m_modernTransform);
			}
		} else if (m_theTracer) {
			Matrix3D pos = m_theTracer->Get_Transform();
			pos.Translate(Vector3(m_speedInDistPerFrame, 0.0f, 0.0f));
			m_theTracer->Set_Transform(pos);
		}
	}
	if (m_modernBeam.Is_Valid() && !updateModernTracer()) {
		Graphics::DestroyBeam(m_modernBeam);
		m_modernBeam = {};
		createLegacyTracer(m_modernTransform);
	}

}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void W3DTracerDraw::crc( Xfer *xfer )
{

	// extend base class
	DrawModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void W3DTracerDraw::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	DrawModule::xfer( xfer );

	// no data to save here, nobody will ever notice

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void W3DTracerDraw::loadPostProcess()
{

	// extend base class
	DrawModule::loadPostProcess();

}
