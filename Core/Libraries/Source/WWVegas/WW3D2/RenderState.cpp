/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
*/

#include "RenderState.h"

RenderStateStruct::RenderStateStruct() :
	shader_bits(0),
	material(nullptr),
	index_buffer(nullptr),
	index_buffer_type(BUFFER_TYPE_INVALID),
	vba_offset(0),
	vba_count(0),
	iba_offset(0),
	index_base_offset(0)
{
	world.Make_Identity();
	view.Make_Identity();
	for (unsigned i = 0; i < MAX_VERTEX_STREAMS; ++i)
	{
		vertex_buffers[i] = nullptr;
		vertex_buffer_types[i] = BUFFER_TYPE_INVALID;
	}
	for (unsigned i = 0; i < MAX_TEXTURE_STAGES; ++i)
	{
		Textures[i] = nullptr;
	}
	for (unsigned i = 0; i < 4; ++i)
	{
		LightEnable[i] = false;
	}
}

RenderStateStruct::~RenderStateStruct()
{
	REF_PTR_RELEASE(material);
	for (unsigned i = 0; i < MAX_VERTEX_STREAMS; ++i)
	{
		REF_PTR_RELEASE(vertex_buffers[i]);
	}
	REF_PTR_RELEASE(index_buffer);
	for (unsigned i = 0; i < MAX_TEXTURE_STAGES; ++i)
	{
		REF_PTR_RELEASE(Textures[i]);
	}
}

RenderStateStruct & RenderStateStruct::operator=(const RenderStateStruct & source)
{
	if (this == &source)
	{
		return *this;
	}

	REF_PTR_SET(material, source.material);
	for (unsigned i = 0; i < MAX_VERTEX_STREAMS; ++i)
	{
		REF_PTR_SET(vertex_buffers[i], source.vertex_buffers[i]);
		vertex_buffer_types[i] = source.vertex_buffer_types[i];
	}
	REF_PTR_SET(index_buffer, source.index_buffer);
	for (unsigned i = 0; i < MAX_TEXTURE_STAGES; ++i)
	{
		REF_PTR_SET(Textures[i], source.Textures[i]);
	}

	shader_bits = source.shader_bits;
	for (unsigned i = 0; i < 4; ++i)
	{
		Lights[i] = source.Lights[i];
		LightEnable[i] = source.LightEnable[i];
	}
	world = source.world;
	view = source.view;
	index_buffer_type = source.index_buffer_type;
	vba_offset = source.vba_offset;
	vba_count = source.vba_count;
	iba_offset = source.iba_offset;
	index_base_offset = source.index_base_offset;

	return *this;
}
