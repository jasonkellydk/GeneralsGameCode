/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "VertexFormat.h"

#include "WWDebug/wwdebug.h"
#include "WWLib/wwstring.h"

RenderBackendVertexLayout::RenderBackendVertexLayout() :
	format(RenderBackendVertexFormat::Unknown),
	transformed(false),
	has_blend(false),
	has_normal(false),
	has_diffuse(false),
	has_specular(false),
	texture_count(0)
{
	for (unsigned &dimension : texture_dimensions)
	{
		dimension = 0;
	}
}

RenderBackendVertexLayout RenderBackend_Vertex_Layout(RenderBackendVertexFormat format)
{
	RenderBackendVertexLayout layout;
	layout.format = format;

	switch (format)
	{
		case RenderBackendVertexFormat::Position:
			break;
		case RenderBackendVertexFormat::PositionDiffuse:
			layout.has_diffuse = true;
			break;
		case RenderBackendVertexFormat::PositionTexture:
			layout.texture_count = 1;
			layout.texture_dimensions[0] = 2;
			break;
		case RenderBackendVertexFormat::PositionDiffuseTexture:
			layout.has_diffuse = true;
			layout.texture_count = 1;
			layout.texture_dimensions[0] = 2;
			break;
		case RenderBackendVertexFormat::PositionTexture2:
			layout.texture_count = 2;
			layout.texture_dimensions[0] = 2;
			layout.texture_dimensions[1] = 2;
			break;
		case RenderBackendVertexFormat::PositionDiffuseTexture2:
			layout.has_diffuse = true;
			layout.texture_count = 2;
			layout.texture_dimensions[0] = 2;
			layout.texture_dimensions[1] = 2;
			break;
		case RenderBackendVertexFormat::PositionNormal:
			layout.has_normal = true;
			break;
		case RenderBackendVertexFormat::PositionNormalDiffuse:
			layout.has_normal = true;
			layout.has_diffuse = true;
			break;
		case RenderBackendVertexFormat::PositionNormalTexture:
			layout.has_normal = true;
			layout.texture_count = 1;
			layout.texture_dimensions[0] = 2;
			break;
		case RenderBackendVertexFormat::PositionNormalDiffuseTexture:
			layout.has_normal = true;
			layout.has_diffuse = true;
			layout.texture_count = 1;
			layout.texture_dimensions[0] = 2;
			break;
		case RenderBackendVertexFormat::PositionNormalTexture2:
			layout.has_normal = true;
			layout.texture_count = 2;
			layout.texture_dimensions[0] = 2;
			layout.texture_dimensions[1] = 2;
			break;
		case RenderBackendVertexFormat::PositionNormalDiffuseTexture2:
			layout.has_normal = true;
			layout.has_diffuse = true;
			layout.texture_count = 2;
			layout.texture_dimensions[0] = 2;
			layout.texture_dimensions[1] = 2;
			break;
		case RenderBackendVertexFormat::TransformedPosition:
			layout.transformed = true;
			break;
		case RenderBackendVertexFormat::TransformedPositionDiffuse:
			layout.transformed = true;
			layout.has_diffuse = true;
			break;
		case RenderBackendVertexFormat::TransformedPositionTexture:
			layout.transformed = true;
			layout.texture_count = 1;
			layout.texture_dimensions[0] = 2;
			break;
		case RenderBackendVertexFormat::TransformedPositionDiffuseTexture:
			layout.transformed = true;
			layout.has_diffuse = true;
			layout.texture_count = 1;
			layout.texture_dimensions[0] = 2;
			break;
		case RenderBackendVertexFormat::TransformedPositionTexture2:
			layout.transformed = true;
			layout.texture_count = 2;
			layout.texture_dimensions[0] = 2;
			layout.texture_dimensions[1] = 2;
			break;
		case RenderBackendVertexFormat::TransformedPositionDiffuseTexture2:
			layout.transformed = true;
			layout.has_diffuse = true;
			layout.texture_count = 2;
			layout.texture_dimensions[0] = 2;
			layout.texture_dimensions[1] = 2;
			break;
		case RenderBackendVertexFormat::PositionNormalDiffuseTexture4TangentBasis:
			layout.has_normal = true;
			layout.has_diffuse = true;
			layout.texture_count = 4;
			layout.texture_dimensions[0] = 2;
			layout.texture_dimensions[1] = 3;
			layout.texture_dimensions[2] = 3;
			layout.texture_dimensions[3] = 3;
			break;
		case RenderBackendVertexFormat::PositionNormalTextureDisplacement:
			layout.has_normal = true;
			layout.texture_count = 3;
			layout.texture_dimensions[0] = 1;
			layout.texture_dimensions[1] = 4;
			layout.texture_dimensions[2] = 2;
			break;
		case RenderBackendVertexFormat::PositionNormalDiffuseCube:
			layout.has_normal = true;
			layout.has_diffuse = true;
			break;
		default:
			break;
	}

	return layout;
}

VertexFormatInfoClass::VertexFormatInfoClass(RenderBackendVertexFormat format_) :
	VertexFormatInfoClass(RenderBackend_Vertex_Layout(format_))
{
}

VertexFormatInfoClass::VertexFormatInfoClass(const RenderBackendVertexLayout &layout_) :
	layout(layout_),
	format(layout_.format),
	vertex_size(0),
	location_offset(0),
	normal_offset(0),
	blend_offset(0),
	diffuse_offset(0),
	specular_offset(0)
{
	for (unsigned &offset : texcoord_offset)
	{
		offset = 0;
	}

	const unsigned position_size = layout.transformed ? 16 : 12;
	unsigned next_offset = position_size;

	location_offset = 0;
	blend_offset = next_offset;
	if (layout.has_blend)
	{
		next_offset += 16;
	}

	normal_offset = next_offset;
	if (layout.has_normal)
	{
		next_offset += 12;
	}

	diffuse_offset = next_offset;
	if (layout.has_diffuse)
	{
		next_offset += 4;
	}

	specular_offset = next_offset;
	if (layout.has_specular)
	{
		next_offset += 4;
	}

	for (unsigned index = 0; index < layout.texture_count &&
		index < RENDER_BACKEND_MAX_TEXTURE_COORDINATES; ++index)
	{
		texcoord_offset[index] = next_offset;
		const unsigned dimension = layout.texture_dimensions[index] == 0 ?
			2 : layout.texture_dimensions[index];
		next_offset += dimension * sizeof(float);
	}

	vertex_size = next_offset;
}

unsigned VertexFormatInfoClass::Get_Tex_Offset(unsigned index) const
{
	WWASSERT(index < RENDER_BACKEND_MAX_TEXTURE_COORDINATES);
	return index < RENDER_BACKEND_MAX_TEXTURE_COORDINATES ? texcoord_offset[index] : 0;
}

void VertexFormatInfoClass::Get_Format_Name(StringClass &name) const
{
	switch (format)
	{
		case RenderBackendVertexFormat::Position: name = "Position"; break;
		case RenderBackendVertexFormat::PositionDiffuse: name = "PositionDiffuse"; break;
		case RenderBackendVertexFormat::PositionTexture: name = "PositionTexture"; break;
		case RenderBackendVertexFormat::PositionDiffuseTexture: name = "PositionDiffuseTexture"; break;
		case RenderBackendVertexFormat::PositionTexture2: name = "PositionTexture2"; break;
		case RenderBackendVertexFormat::PositionDiffuseTexture2: name = "PositionDiffuseTexture2"; break;
		case RenderBackendVertexFormat::PositionNormal: name = "PositionNormal"; break;
		case RenderBackendVertexFormat::PositionNormalDiffuse: name = "PositionNormalDiffuse"; break;
		case RenderBackendVertexFormat::PositionNormalTexture: name = "PositionNormalTexture"; break;
		case RenderBackendVertexFormat::PositionNormalDiffuseTexture: name = "PositionNormalDiffuseTexture"; break;
		case RenderBackendVertexFormat::PositionNormalTexture2: name = "PositionNormalTexture2"; break;
		case RenderBackendVertexFormat::PositionNormalDiffuseTexture2: name = "PositionNormalDiffuseTexture2"; break;
		case RenderBackendVertexFormat::TransformedPosition: name = "TransformedPosition"; break;
		case RenderBackendVertexFormat::TransformedPositionDiffuse: name = "TransformedPositionDiffuse"; break;
		case RenderBackendVertexFormat::TransformedPositionTexture: name = "TransformedPositionTexture"; break;
		case RenderBackendVertexFormat::TransformedPositionDiffuseTexture: name = "TransformedPositionDiffuseTexture"; break;
		case RenderBackendVertexFormat::TransformedPositionTexture2: name = "TransformedPositionTexture2"; break;
		case RenderBackendVertexFormat::TransformedPositionDiffuseTexture2: name = "TransformedPositionDiffuseTexture2"; break;
		case RenderBackendVertexFormat::PositionNormalDiffuseTexture4TangentBasis: name = "PositionNormalDiffuseTexture4TangentBasis"; break;
		case RenderBackendVertexFormat::PositionNormalTextureDisplacement: name = "PositionNormalTextureDisplacement"; break;
		case RenderBackendVertexFormat::PositionNormalDiffuseCube: name = "PositionNormalDiffuseCube"; break;
		default: name = "Generic"; break;
	}
}
