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

// FILE: W3DSnow.h /////////////////////////////////////////////////////////

#pragma once

#include <span>

#include "GameClient/Snow.h"

class IndexBufferClass;
class RenderInfoClass;
class TextureClass;
class RenderBackendVertexBuffer;

class W3DSnowManager : public SnowManager
{
  public :

	W3DSnowManager();
	virtual ~W3DSnowManager() override;

	virtual void init() override;
	virtual void reset() override;
	virtual void update () override;
	virtual void updateIniSettings() override;

	void	render(RenderInfoClass &rinfo);
	void	renderAsQuads(RenderInfoClass &rinfo, Int cubeOriginX, Int cubeOriginY, Int cubeDimX, Int cubeDimY);
	void	renderSubBox(RenderInfoClass &rinfo, Int originX, Int originY, Int cubeDimX, Int cubeDimY );
	std::size_t Build_Modern_Particles(float camera_x, float camera_y, float camera_z,
		std::span<float> position_x, std::span<float> position_y, std::span<float> position_z,
		std::span<float> sizes) const noexcept;
	float Modern_Cull_Radius() const noexcept;
	bool Modern_Uses_Point_Sprites() const noexcept;
	float Modern_Point_Sprite_Size() const noexcept;
	void	ReleaseResources();
	Bool	ReAcquireResources();

 private:
	IndexBufferClass	*m_indexBuffer;
	TextureClass *m_snowTexture;
	RenderBackendVertexBuffer* m_vertexBuffer;
	Int m_dwBase;	///<index to beginning of unused vertex buffer space.
    Int m_dwFlush;	///<maximum amount of vertices to sumbit before rendering.
	Int m_dwDiscard;	///<maximum index allowed before needing to discard the buffer.
	Int m_leafDim;		///<horizontal dimensions of leaf nodes that are always rendered without visibility checks.
	Real m_snowCeiling;	///<height at the top of the cube with camera at center.
	Real m_heightTraveled;	///<height that snow flake traveled this frame.
	Int m_totalRendered;	///<total number of snow particles rendered this frame - only for profiling.
	Real m_cullOverscan;	///<how much extra padding to put on the sides of AABoxes when view culling.
};
