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

#include "WWLib/always.h"

class StringClass;

constexpr unsigned RENDER_BACKEND_MAX_TEXTURE_COORDINATES = 8;

// These values describe the vertex data consumed by WW3D. They deliberately
// contain no graphics API encoding. A backend translates them to its own
// vertex declaration or fixed-function format.
enum class RenderBackendVertexFormat
{
	Unknown,
	Position,
	PositionDiffuse,
	PositionTexture,
	PositionDiffuseTexture,
	PositionTexture2,
	PositionDiffuseTexture2,
	PositionNormal,
	PositionNormalDiffuse,
	PositionNormalTexture,
	PositionNormalDiffuseTexture,
	PositionNormalTexture2,
	PositionNormalDiffuseTexture2,
	TransformedPosition,
	TransformedPositionDiffuse,
	TransformedPositionTexture,
	TransformedPositionDiffuseTexture,
	TransformedPositionTexture2,
	TransformedPositionDiffuseTexture2,
	PositionNormalDiffuseTexture4TangentBasis,
	PositionNormalTextureDisplacement,
	PositionNormalDiffuseCube
};

struct RenderBackendVertexLayout
{
	RenderBackendVertexFormat format;
	bool transformed;
	bool has_blend;
	bool has_normal;
	bool has_diffuse;
	bool has_specular;
	unsigned texture_count;
	unsigned texture_dimensions[RENDER_BACKEND_MAX_TEXTURE_COORDINATES];

	RenderBackendVertexLayout();
};

RenderBackendVertexLayout RenderBackend_Vertex_Layout(RenderBackendVertexFormat format);

inline unsigned RenderBackend_Vertex_Format_Stride(RenderBackendVertexFormat format)
{
	switch (format)
	{
		case RenderBackendVertexFormat::Position: return 12;
		case RenderBackendVertexFormat::PositionDiffuse: return 16;
		case RenderBackendVertexFormat::PositionTexture: return 20;
		case RenderBackendVertexFormat::PositionDiffuseTexture: return 24;
		case RenderBackendVertexFormat::PositionTexture2: return 28;
		case RenderBackendVertexFormat::PositionDiffuseTexture2: return 32;
		case RenderBackendVertexFormat::PositionNormal: return 24;
		case RenderBackendVertexFormat::PositionNormalDiffuse: return 28;
		case RenderBackendVertexFormat::PositionNormalTexture: return 32;
		case RenderBackendVertexFormat::PositionNormalDiffuseTexture: return 36;
		case RenderBackendVertexFormat::PositionNormalTexture2: return 40;
		case RenderBackendVertexFormat::PositionNormalDiffuseTexture2: return 44;
		case RenderBackendVertexFormat::TransformedPosition: return 16;
		case RenderBackendVertexFormat::TransformedPositionDiffuse: return 20;
		case RenderBackendVertexFormat::TransformedPositionTexture: return 24;
		case RenderBackendVertexFormat::TransformedPositionDiffuseTexture: return 28;
		case RenderBackendVertexFormat::TransformedPositionTexture2: return 32;
		case RenderBackendVertexFormat::TransformedPositionDiffuseTexture2: return 36;
		case RenderBackendVertexFormat::PositionNormalDiffuseTexture4TangentBasis: return 72;
		case RenderBackendVertexFormat::PositionNormalTextureDisplacement: return 52;
		case RenderBackendVertexFormat::PositionNormalDiffuseCube: return 28;
		default: return 0;
	}
}

inline constexpr RenderBackendVertexFormat RenderBackend_Dynamic_Vertex_Format =
	RenderBackendVertexFormat::PositionNormalDiffuseTexture2;

// WW3D vertex memory layouts. These structures are engine data, not backend
// resources, and are intentionally available to code that fills a buffer.
struct VertexFormatXYZ
{
	float x;
	float y;
	float z;
};

struct VertexFormatXYZNUV1
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	float u1;
	float v1;
};

struct VertexFormatXYZNUV2
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	float u1;
	float v1;
	float u2;
	float v2;
};

struct VertexFormatXYZN
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
};

struct VertexFormatXYZNDUV1
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	unsigned diffuse;
	float u1;
	float v1;
};

struct VertexFormatXYZNDUV2
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	unsigned diffuse;
	float u1;
	float v1;
	float u2;
	float v2;
};

struct VertexFormatXYZDUV1
{
	float x;
	float y;
	float z;
	unsigned diffuse;
	float u1;
	float v1;
};

struct VertexFormatXYZDUV2
{
	float x;
	float y;
	float z;
	unsigned diffuse;
	float u1;
	float v1;
	float u2;
	float v2;
};

struct VertexFormatXYZUV1
{
	float x;
	float y;
	float z;
	float u1;
	float v1;
};

struct VertexFormatXYZUV2
{
	float x;
	float y;
	float z;
	float u1;
	float v1;
	float u2;
	float v2;
};

struct VertexFormatXYZNDUV1TG3
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	unsigned diffuse;
	float u1;
	float v1;
	float Sx;
	float Sy;
	float Sz;
	float Tx;
	float Ty;
	float Tz;
	float SxTx;
	float SxTy;
	float SxTz;
};

struct VertexFormatXYZNUV2DMAP
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	float T1x;
	float T1y;
	float T1z;
	float T1w;
	float T2x;
	float T2y;
};

struct VertexFormatXYZNDCUBEMAP
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	unsigned diffuse;
};

// Describes offsets in a WW3D vertex layout. It does not contain a native
// vertex-format bitmask; native conversion is a backend implementation detail.
class VertexFormatInfoClass
{
	W3DMPO_CODE(VertexFormatInfoClass)

	RenderBackendVertexLayout layout;
	RenderBackendVertexFormat format;
	unsigned vertex_size;
	unsigned location_offset;
	unsigned normal_offset;
	unsigned blend_offset;
	unsigned texcoord_offset[8];
	unsigned diffuse_offset;
	unsigned specular_offset;

public:
	explicit VertexFormatInfoClass(RenderBackendVertexFormat format);
	explicit VertexFormatInfoClass(const RenderBackendVertexLayout &layout);

	unsigned Get_Location_Offset() const { return location_offset; }
	unsigned Get_Normal_Offset() const { return normal_offset; }
	unsigned Get_Tex_Offset(unsigned index) const;
	unsigned Get_Diffuse_Offset() const { return diffuse_offset; }
	unsigned Get_Specular_Offset() const { return specular_offset; }
	unsigned Get_Vertex_Size() const { return vertex_size; }
	RenderBackendVertexFormat Get_Format() const { return format; }
	const RenderBackendVertexLayout &Get_Layout() const { return layout; }

	void Get_Format_Name(StringClass &name) const;
};
