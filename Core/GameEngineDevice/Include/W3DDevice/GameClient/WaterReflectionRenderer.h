/*
** Command & Conquer Generals Zero Hour(tm)
**
** Neutral scene-submission contract used by the modern water renderer.
*/

#pragma once

#include "WW3D2/Backend/RenderBackendTypes.h"

class CameraClass;

class WaterReflectionRenderer
{
public:
	virtual ~WaterReflectionRenderer() = default;

	virtual void Render_Water_Reflection(CameraClass *camera,
		const RenderBackendViewport &viewport) = 0;
};
