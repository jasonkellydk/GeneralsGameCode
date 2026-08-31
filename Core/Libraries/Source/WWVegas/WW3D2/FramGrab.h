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
 *                     $Archive:: /Commando/Code/ww3d2/FramGrab.h                             $*
 *                                                                                             *
 *                       Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                     $Modtime:: 1/08/01 10:04a                                              $*
 *                                                                                             *
 *                    $Revision:: 1                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

// FramGrab.h: interface for the FrameGrabClass class.
//
//////////////////////////////////////////////////////////////////////

class FrameGrabClass
{
public:
	enum MODE {
		RAW,
		AVI
	};

	// depending on which mode you select, it will produce either frames or an AVI.
	FrameGrabClass(const char *filename, MODE mode, int width, int height, int bitdepth, float framerate );

	virtual ~FrameGrabClass();

	void ConvertGrab(void *BitmapPointer);
	void Grab(void *BitmapPointer);

	long * GetBuffer()			{ return reinterpret_cast<long *> (BitmapStorage.data ()); }
	float	GetFrameRate()			{ return FrameRate; }

protected:
	std::string Filename;
	float			FrameRate;

	MODE Mode;
	std::uint32_t Counter; // number of frames written.
	int Width;
	int Height;
	int BitDepth;
	std::uint32_t FrameSize;
	std::uint32_t RiffSizeOffset;
	std::uint32_t AviHeaderFrameCountOffset;
	std::uint32_t StreamHeaderFrameCountOffset;
	std::uint32_t MovieListSizeOffset;
	std::uint32_t MovieDataOffset;
	std::fstream Output;
	std::vector<std::uint32_t> FrameOffsets;
	std::vector<std::uint8_t> BitmapStorage;

	void GrabAVI(void *BitmapPointer);
	void GrabRawFrame(void *BitmapPointer);

	// general purpose cleanup routine
	void CleanupAVI();

	// convert the SR image into AVI byte ordering
	void ConvertFrame(void *BitmapPointer);

};
