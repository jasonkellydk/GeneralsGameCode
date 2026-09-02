/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
*/

#pragma once

#include "Backend/RenderBackend.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "Texture.h"
#include "VertexBuffer.h"
#include "VertMaterial.h"
#include "WWMath/matrix4.h"

// Render state is shared by WW3D2 and the active backend.  It intentionally
// contains only WW3D and neutral backend types; native graphics structures
// belong in the backend implementation that consumes this state.
struct RenderStateStruct
{
	unsigned shader_bits;
	VertexMaterialClass * material;
	TextureBaseClass * Textures[MAX_TEXTURE_STAGES];
	RenderBackendLight Lights[4];
	bool LightEnable[4];
	Matrix4x4 world;
	Matrix4x4 view;
	unsigned vertex_buffer_types[MAX_VERTEX_STREAMS];
	unsigned index_buffer_type;
	unsigned short vba_offset;
	unsigned short vba_count;
	unsigned short iba_offset;
	VertexBufferClass * vertex_buffers[MAX_VERTEX_STREAMS];
	IndexBufferClass * index_buffer;
	unsigned short index_base_offset;

	RenderStateStruct();
	~RenderStateStruct();

	RenderStateStruct & operator=(const RenderStateStruct & source);
};
