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

// FILE: W3DDefaultDraw.cpp ///////////////////////////////////////////////////////////////////////
// Author: Colin Day, November 2001
// Desc:   Default w3d draw module
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include "Common/FileSystem.h"	// this is only here to pull in LOAD_TEST_ASSETS
#include "Common/GlobalData.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Object.h"
#include "GameClient/Shadow.h"
#include "GameClient/FXList.h"
#include "GameLogic/TerrainLogic.h"

#include "WW3D2/HAnim.h"
#include "WW3D2/HLOD.h"
#include "WW3D2/Mesh.h"
#include "WW3D2/MeshMdl.h"
#include "WW3D2/RendObj.h"
#include "WWMath/sphere.h"
#include "W3DDevice/GameClient/Module/W3DDefaultDraw.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/W3DScene.h"
#include "W3DDevice/GameClient/W3DShadow.h"

namespace
{
Graphics::RenderTransform Make_Modern_Transform(const Matrix3D &matrix) noexcept
{
	Graphics::RenderTransform transform;
	for (std::size_t row = 0; row < 3; ++row) {
		for (std::size_t column = 0; column < 4; ++column)
			transform.matrix[row * 4 + column] = matrix[static_cast<int>(row)][static_cast<int>(column)];
	}
	transform.matrix[12] = 0.0f;
	transform.matrix[13] = 0.0f;
	transform.matrix[14] = 0.0f;
	transform.matrix[15] = 1.0f;
	return transform;
}
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
W3DDefaultDraw::W3DDefaultDraw(Thing *thing, const ModuleData* moduleData) : DrawModule(thing, moduleData)
{
#ifdef LOAD_TEST_ASSETS
	m_renderObject = nullptr;
	m_shadow = nullptr;
	m_modernMesh = {};
	m_modernMaterial = {};
	m_modernInstance = {};
	m_modernBounds = {};
	m_modernActive = false;
	m_modernShadowsEnabled = true;
	m_modernShadowObscured = false;
	if (!getDrawable()->getTemplate()->getLTAName().isEmpty())
	{
		m_renderObject = W3DDisplay::m_assetManager->Create_Render_Obj(getDrawable()->getTemplate()->getLTAName().str(), getDrawable()->getScale(), 0);

		Shadow::ShadowTypeInfo shadowInfo;
		shadowInfo.m_type=(ShadowType)SHADOW_VOLUME;
  		m_shadow = TheW3DShadowManager->addShadow(m_renderObject, &shadowInfo);


		DEBUG_ASSERTCRASH(m_renderObject, ("Test asset %s not found", getDrawable()->getTemplate()->getLTAName().str()));
		if (m_renderObject)
		{

			W3DDisplay::m_3DScene->Add_Render_Object(m_renderObject);

			m_renderObject->Set_User_Data(getDrawable()->getDrawableInfo());

			Matrix3D transform;
			///@todo: Change back to identity once we figure out why objects show up at 0,0,0
			/// OBJECT_PILE
//			transform.Set(Vector3(0,0,9999));
			transform.Set(Vector3(0,0,0));
			m_renderObject->Set_Transform(transform);
			createModernMesh();
		}
	}
#endif
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void W3DDefaultDraw::reactToTransformChange( const Matrix3D *oldMtx,
																						 const Coord3D *oldPos,
																						 Real oldAngle )
{
#ifdef LOAD_TEST_ASSETS
	if( m_renderObject )
		m_renderObject->Set_Transform( *getDrawable()->getTransformMatrix() );
	updateModernInstance(getDrawable()->getTransformMatrix());
#endif
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
W3DDefaultDraw::~W3DDefaultDraw()
{
#ifdef LOAD_TEST_ASSETS
	releaseModernMesh();
	if (TheW3DShadowManager && m_shadow)
	{
		TheW3DShadowManager->removeShadow(m_shadow);
		m_shadow = nullptr;
	}
	if (m_renderObject)
	{
		W3DDisplay::m_3DScene->Remove_Render_Object(m_renderObject);
  	REF_PTR_RELEASE(m_renderObject);
		m_renderObject = nullptr;
	}
#endif
}

//-------------------------------------------------------------------------------------------------
void W3DDefaultDraw::setShadowsEnabled(Bool enable)
{
#ifdef LOAD_TEST_ASSETS
	if (m_shadow)
		m_shadow->enableShadowRender(enable);
	m_modernShadowsEnabled = enable != 0;
	if (m_modernActive)
		updateModernInstance(getDrawable()->getTransformMatrix());
#endif
}

//-------------------------------------------------------------------------------------------------
void W3DDefaultDraw::setFullyObscuredByShroud(Bool fullyObscured)
{
#ifdef LOAD_TEST_ASSETS
	if (m_shadow)
		m_shadow->enableShadowInvisible(fullyObscured);
	m_modernShadowObscured = fullyObscured != 0;
	if (m_modernActive)
		updateModernInstance(getDrawable()->getTransformMatrix());
#endif
}

//-------------------------------------------------------------------------------------------------
void W3DDefaultDraw::doDrawModule(const Matrix3D* transformMtx)
{
#ifdef LOAD_TEST_ASSETS
	if(m_renderObject)
	{
		Matrix3D scaledTransform;
		if (getDrawable()->getInstanceScale() != 1.0f)
		{	//do custom scaling of the W3D model.
			scaledTransform=*transformMtx;
			scaledTransform.Scale(getDrawable()->getInstanceScale());
			transformMtx = &scaledTransform;
			m_renderObject->Set_ObjectScale(getDrawable()->getInstanceScale());
		}
		else
		{
			m_renderObject->Set_Transform(*transformMtx);
		}

		if (!m_modernActive)
			createModernMesh();
		updateModernInstance(transformMtx);
	}
#endif
}

#ifdef LOAD_TEST_ASSETS
bool W3DDefaultDraw::createModernMesh()
{
	Graphics::StaticMeshRenderer &renderer = Graphics::GetStaticMeshRenderer();
	if (!renderer.Is_Initialized() || m_renderObject == nullptr || m_renderObject->Class_ID() != RenderObjClass::CLASSID_MESH)
		return false;

	MeshClass *mesh = static_cast<MeshClass *>(m_renderObject);
	MeshModelClass *model = mesh->Peek_Model();
	if (model == nullptr)
		return false;

	const int vertex_count = model->Get_Vertex_Count();
	const int polygon_count = model->Get_Polygon_Count();
	if (vertex_count <= 0 || polygon_count <= 0 || vertex_count > 65535 || polygon_count > static_cast<int>(std::numeric_limits<std::uint32_t>::max() / 3))
		return false;

	std::vector<Graphics::StaticMeshVertex> vertices(static_cast<std::size_t>(vertex_count));
	const Vector3 *positions = model->Get_Vertex_Array();
	const Vector2 *uvs = model->Get_UV_Array_By_Index(0);
	for (int index = 0; index < vertex_count; ++index) {
		vertices[index].position[0] = positions[index].X;
		vertices[index].position[1] = positions[index].Y;
		vertices[index].position[2] = positions[index].Z;
		if (uvs != nullptr) {
			vertices[index].uv[0] = uvs[index].X;
			vertices[index].uv[1] = uvs[index].Y;
		}
	}

	const TriIndex *polygons = model->Get_Polygon_Array();
	std::vector<std::uint16_t> indices(static_cast<std::size_t>(polygon_count) * 3);
	for (int polygon = 0; polygon < polygon_count; ++polygon) {
		const TriIndex &triangle = polygons[polygon];
		for (int corner = 0; corner < 3; ++corner) {
			if (triangle[corner] >= vertex_count)
				return false;
			indices[static_cast<std::size_t>(polygon) * 3 + corner] = triangle[corner];
		}
	}

	SphereClass sphere;
	mesh->Get_Obj_Space_Bounding_Sphere(sphere);
	const Graphics::StaticMeshSource source{
		static_cast<std::uint32_t>(vertex_count),
		static_cast<std::uint32_t>(indices.size()),
		static_cast<std::uint32_t>(sizeof(Graphics::StaticMeshVertex)),
		Graphics::MeshIndexFormat::UInt16,
		std::as_bytes(std::span<const Graphics::StaticMeshVertex>(vertices)),
		std::as_bytes(std::span<const std::uint16_t>(indices)),
		{sphere.Center.X, sphere.Center.Y, sphere.Center.Z},
		sphere.Radius
	};
	const Graphics::MeshHandle modern_mesh = renderer.Create_Mesh(source);
	if (!modern_mesh.Is_Valid())
		return false;

	Graphics::RenderInstance instance;
	instance.mesh = modern_mesh;
	instance.material = renderer.Default_Material();
	instance.bounds = {
		{{sphere.Center.X, sphere.Center.Y, sphere.Center.Z}},
		sphere.Radius
	};
	instance.flags = Graphics::RenderInstanceFlags::CastsShadow | Graphics::RenderInstanceFlags::ReceivesShadow;
	const Graphics::InstanceHandle modern_instance = renderer.Create_Instance(instance);
	if (!modern_instance.Is_Valid()) {
		renderer.Destroy_Mesh(modern_mesh);
		return false;
	}

	m_modernMesh = modern_mesh;
	m_modernMaterial = instance.material;
	m_modernInstance = modern_instance;
	m_modernBounds = instance.bounds;
	m_modernActive = true;
	m_renderObject->Set_Hidden(true);
	return true;
}

void W3DDefaultDraw::updateModernInstance(const Matrix3D *transformMtx)
{
	if (transformMtx == nullptr || !m_modernActive)
		return;

	Matrix3D scaled_transform;
	const Matrix3D *effective_transform = transformMtx;
	if (getDrawable()->getInstanceScale() != 1.0f) {
		scaled_transform = *transformMtx;
		scaled_transform.Scale(getDrawable()->getInstanceScale());
		effective_transform = &scaled_transform;
	}

	Graphics::RenderInstance instance;
	instance.transform = Make_Modern_Transform(*effective_transform);
	instance.bounds = m_modernBounds;
	instance.mesh = m_modernMesh;
	instance.material = m_modernMaterial;
	instance.flags = Graphics::RenderInstanceFlags::CastsShadow | Graphics::RenderInstanceFlags::ReceivesShadow;
	if (!m_modernShadowsEnabled || m_modernShadowObscured)
		instance.flags = Graphics::RenderInstanceFlags::ReceivesShadow;

	if (!Graphics::GetStaticMeshRenderer().Update_Instance(m_modernInstance, instance)) {
		releaseModernMesh();
		if (m_renderObject != nullptr)
			m_renderObject->Set_Hidden(false);
		createModernMesh();
	}
}

void W3DDefaultDraw::releaseModernMesh() noexcept
{
	Graphics::StaticMeshRenderer &renderer = Graphics::GetStaticMeshRenderer();
	if (renderer.Is_Initialized()) {
		if (m_modernInstance.Is_Valid())
			renderer.Destroy_Instance(m_modernInstance);
		if (m_modernMesh.Is_Valid())
			renderer.Destroy_Mesh(m_modernMesh);
	}
	m_modernMesh = {};
	m_modernMaterial = {};
	m_modernInstance = {};
	m_modernBounds = {};
	m_modernActive = false;
}
#endif

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void W3DDefaultDraw::crc( Xfer *xfer )
{

	// extend base class
	DrawModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void W3DDefaultDraw::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	DrawModule::xfer( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void W3DDefaultDraw::loadPostProcess()
{

	// extend base class
	DrawModule::loadPostProcess();

}
