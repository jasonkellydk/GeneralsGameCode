/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
*/

#pragma once

#include <windows.h>
#include <d3d9types.h>

#include "WW3D2/WW3DFormat.h"

D3DFORMAT DX9_Format_From_WW3D(WW3DFormat format);
WW3DFormat WW3D_Format_From_DX9(D3DFORMAT format);
D3DFORMAT DX9_ZFormat_From_WW3D(WW3DZFormat format);
WW3DZFormat WW3DZ_Format_From_DX9(D3DFORMAT format);
