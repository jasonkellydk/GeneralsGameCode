/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#pragma once

// Buffer kinds describe how WW3D owns and submits a buffer. They deliberately
// do not identify a graphics API.
enum RenderBufferType : unsigned
{
	BUFFER_TYPE_RENDER,
	BUFFER_TYPE_SORTING,
	BUFFER_TYPE_DYNAMIC_RENDER,
	BUFFER_TYPE_DYNAMIC_SORTING,
	BUFFER_TYPE_INVALID
};

enum RenderBufferUsage : unsigned
{
	BUFFER_USAGE_DEFAULT = 0,
	BUFFER_USAGE_DYNAMIC = 1u << 0,
	BUFFER_USAGE_SOFTWARE_PROCESSING = 1u << 1,
	BUFFER_USAGE_NPATCHES = 1u << 2
};

