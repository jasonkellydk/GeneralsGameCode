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
 *                 Project Name : ww3d                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/meshrenderer.h                          $*
 *                                                                                             *
 *              Original Author:: Jani Penttinen                                               *
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 06/27/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 29                                                          $*
 *                                                                                             *
 * 06/27/02 KM Changes to max texture stage caps																*
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "WWLib/always.h"
#include "WWLib/wwstring.h"
#include "WWLib/simplevec.h"
#include "WWLib/Vector.h"
#include "MeshRenderLists.h"
#include "WW3D2/Shader.h"
#include "WW3D2/MeshMatDesc.h"
#include "WW3D2/MeshMdl.h"

class IndexBufferClass;
class VertexBufferClass;
class MeshRenderTypeArrayClass;
class MeshClass;
class MeshModelClass;
class MeshPolygonRendererClass;
class MeshModelRenderData;
class Vertex_Split_Table;
class MeshFVFCategoryContainer;
class DecalMeshClass;
class MaterialPassClass;
class MatPassTaskClass;
class PolyRenderTaskClass;
class TextureClass;
class VertexMaterialClass;
class CameraClass;

/**
** MeshTextureCategoryClass
** This class is used for each Material-Texture-Shader combination that is encountered during rendering.
** Each polygon_renderer that uses the same 'TextureCategory' will be linked to the 'TextureCategory' object.
** Then, all polygons will be rendered in 'TextureCategory' batches to reduce the number of stage changes
** (and most importantly, texture changes) that we cause in Mesh.
*/
class MeshTextureCategoryClass : public MultiListObjectClass
{
	int												pass;
	TextureClass *									textures[MeshMatDescClass::MAX_TEX_STAGES];
	ShaderClass										shader;
	VertexMaterialClass *						material;
	MeshPolygonRendererList						PolygonRendererList;
	MeshFVFCategoryContainer*					container;

	PolyRenderTaskClass *						render_task_head;			// polygon renderers queued for rendering
	static bool											m_gForceMultiply;  // Forces opaque materials to use the multiply blend - pseudo transparent effect.  jba.

public:

	MeshTextureCategoryClass(MeshFVFCategoryContainer* container,TextureClass** textures, ShaderClass shd, VertexMaterialClass* mat,int pass);
	virtual ~MeshTextureCategoryClass() override;

	void									Add_Render_Task(MeshPolygonRendererClass * p_renderer,MeshClass * p_mesh);

	void									Render();
	bool									Anything_To_Render() { return (render_task_head != nullptr); }
	void									Clear_Render_List();

	TextureClass *						Peek_Texture(int stage)	{ return textures[stage]; }
	const VertexMaterialClass *	Peek_Material() { return material; }
	ShaderClass							Get_Shader() { return shader; }

	MeshPolygonRendererList&			Get_Polygon_Renderer_List() { return PolygonRendererList; }

	unsigned Add_Mesh(
		Vertex_Split_Table& split_buffer,
		unsigned vertex_offset,
		unsigned index_offset,
		IndexBufferClass* index_buffer,
		unsigned pass);
	void Log(bool only_visible);

	void Remove_Polygon_Renderer(MeshPolygonRendererClass* p_renderer);
	void Add_Polygon_Renderer(MeshPolygonRendererClass* p_renderer,MeshPolygonRendererClass* add_after_this=nullptr);


	MeshFVFCategoryContainer * Get_Container() { return container; }

	// Force multiply blend on all objects inserted from now on. (Doesn't affect the objects that are already in the lists)
	static void						SetForceMultiply(bool multiply) { m_gForceMultiply=multiply; }

};

// Concrete storage for the opaque per-mesh data exposed by IRenderBackend.
class MeshModelRenderData : public RenderBackendMeshData
{
public:
	MeshPolygonRendererList PolygonRendererList;
};

inline MeshModelRenderData *Get_Mesh_Model_Render_Data(MeshModelClass * mesh)
{
	return mesh == nullptr ? nullptr : static_cast<MeshModelRenderData *>(mesh->Get_Render_Backend_Data());
}

inline const MeshModelRenderData *Get_Mesh_Model_Render_Data(const MeshModelClass * mesh)
{
	return mesh == nullptr ? nullptr : static_cast<const MeshModelRenderData *>(mesh->Get_Render_Backend_Data());
}

// ----------------------------------------------------------------------------

/**
** MeshFVFCategoryContainer
*/

class MeshFVFCategoryContainer : public MultiListObjectClass
{
public:
	enum {
		MAX_PASSES=4
	};
protected:

	TextureCategoryList									texture_category_list[MAX_PASSES];
	TextureCategoryList									visible_texture_category_list[MAX_PASSES];

	MatPassTaskClass *									visible_matpass_head;
	MatPassTaskClass *									visible_matpass_tail;

	IndexBufferClass *									index_buffer;
	int														used_indices;
	unsigned													FVF;
	unsigned													passes;
	unsigned													uv_coordinate_channels;
	bool														sorting;
	bool														AnythingToRender;
	bool														AnyDelayedPassesToRender;

	void Generate_Texture_Categories(Vertex_Split_Table& split_table,unsigned vertex_offset);
	void Insert_To_Texture_Category(
		Vertex_Split_Table& split_table,
		TextureClass** textures,
		VertexMaterialClass* mat,
		ShaderClass shader,
		int pass,
		unsigned vertex_offset);

	bool Anything_To_Render()					{ return AnythingToRender; }
	bool Any_Delayed_Passes_To_Render()	{ return AnyDelayedPassesToRender; }

	void Render_Procedural_Material_Passes();

	MeshTextureCategoryClass* Find_Matching_Texture_Category(
		TextureClass* texture,
		unsigned pass,
		unsigned stage,
		MeshTextureCategoryClass* ref_category);

	MeshTextureCategoryClass* Find_Matching_Texture_Category(
		VertexMaterialClass* vmat,
		unsigned pass,
		MeshTextureCategoryClass* ref_category);

public:

	MeshFVFCategoryContainer(unsigned FVF,bool sorting);
	virtual ~MeshFVFCategoryContainer() override;

	static unsigned Define_FVF(MeshModelClass* mmc,bool enable_lighting);
	bool Is_Sorting() const { return sorting; }

	void Change_Polygon_Renderer_Texture(
		MeshPolygonRendererList& polygon_renderer_list,
		TextureClass* texture,
		TextureClass* new_texture,
		unsigned pass,
		unsigned stage);

	void Change_Polygon_Renderer_Material(
		MeshPolygonRendererList& polygon_renderer_list,
		VertexMaterialClass* vmat,
		VertexMaterialClass* new_vmat,
		unsigned pass);

	void Remove_Texture_Category(MeshTextureCategoryClass* tex_category);

	virtual void Render()=0;
	virtual void Add_Mesh(MeshModelClass* mmc)=0;
	virtual void Log(bool only_visible)=0;
	virtual bool Check_If_Mesh_Fits(MeshModelClass* mmc)=0;

	unsigned Get_FVF() const { return FVF; }

	void Add_Visible_Texture_Category(MeshTextureCategoryClass * tex_category,int pass)
	{
		WWASSERT(pass<MAX_PASSES);
		WWASSERT(tex_category != nullptr);
		WWASSERT(texture_category_list[pass].Contains(tex_category));
		visible_texture_category_list[pass].Add(tex_category);
		AnythingToRender=true;
	}

	/*
	** Material pass rendering.  The following two functions allow procedural material passes
	** to be applied to meshes in this FVF category.  In certain cases, the game will *only* render
	** the procedural pass and not the base materials for the mesh.  When this happens there can
	** be rendering errors unless these procedural passes are rendered after all of the meshes in
	** the scene.  The virtual method Add_Delayed_Material_Pass is used in this case.
	*/
	void Add_Visible_Material_Pass(MaterialPassClass * pass,MeshClass * mesh);
	virtual void Add_Delayed_Visible_Material_Pass(MaterialPassClass * pass, MeshClass * mesh) = 0;
	virtual void Render_Delayed_Procedural_Material_Passes() = 0;
};


/**
** MeshRigidFVFCategoryContainer
** This is an FVFCategoryContainer for rigid (non-skin) meshes
*/
class MeshRigidFVFCategoryContainer : public MeshFVFCategoryContainer
{
public:
	MeshRigidFVFCategoryContainer(unsigned FVF,bool sorting);
	virtual ~MeshRigidFVFCategoryContainer() override;

	virtual void Add_Mesh(MeshModelClass* mmc) override;
	virtual void Log(bool only_visible) override;
	virtual bool Check_If_Mesh_Fits(MeshModelClass* mmc) override;

	virtual void Render() override;	// Generic render function

	/*
	** This method adds a material pass which must be rendered after all of the other rendering is complete.
	** This is needed whenever a mesh turns off its base passes and renders a translucent pass on its geometry.
	*/
	virtual void Add_Delayed_Visible_Material_Pass(MaterialPassClass * pass, MeshClass * mesh) override;
	virtual void Render_Delayed_Procedural_Material_Passes() override;

protected:


	VertexBufferClass *	vertex_buffer;
	int						used_vertices;

	MatPassTaskClass *	delayed_matpass_head;
	MatPassTaskClass *	delayed_matpass_tail;

};


/**
** MeshSkinFVFCategoryContainer
** This is an FVFCategoryContainer for skin meshes
*/
class MeshSkinFVFCategoryContainer: public MeshFVFCategoryContainer
{
public:
	MeshSkinFVFCategoryContainer(bool sorting);
	virtual ~MeshSkinFVFCategoryContainer() override;

	virtual void Render() override;
	virtual void Add_Mesh(MeshModelClass* mmc) override;
	virtual void Log(bool only_visible) override;
	virtual bool Check_If_Mesh_Fits(MeshModelClass* mmc) override;

	void Add_Visible_Skin(MeshClass * mesh);

	/*
	** Since skins are already rendered after the rigid meshes, the Add_Delayed_Material_Pass function simply
	** routes into the Add_Visible_Material_Pass method and no extra overhead is added.
	*/
	virtual void Add_Delayed_Visible_Material_Pass(MaterialPassClass * pass, MeshClass * mesh) override { Add_Visible_Material_Pass(pass,mesh); }
	virtual void Render_Delayed_Procedural_Material_Passes() override { }

private:

	void Reset();
	void clearVisibleSkinList();

	unsigned int								VisibleVertexCount;
	MeshClass *									VisibleSkinHead;
	MeshClass *									VisibleSkinTail;

};



/**
** MeshRendererClass
** This object is controller for the entire Mesh mesh rendering system.  It organizes mesh
** fragments into groups based on FVF, texture, and material.  During rendering, a list of
** the visible mesh fragments is composed and rendered.  There is a global instance of this
** class called TheMeshRenderer that should be used for all mesh rendering.
*/
class MeshRendererClass
{
public:
	MeshRendererClass();
	~MeshRendererClass();

	void						Init();
	void						Shutdown();

	void						Flush();
	void						Clear_Pending_Delete_Lists();

	void						Log_Statistics_String(bool only_visible);
	static void				Request_Log_Statistics();

	void						Register_Mesh_Type(MeshModelClass* mmc);
	void						Unregister_Mesh_Type(MeshModelClass* mmc);
	bool						Has_Mesh_Renderers(const MeshModelClass * mmc) const;
	unsigned					Get_Mesh_Renderer_Vertex_Offset(const MeshModelClass * mmc) const;
	unsigned					Get_Mesh_Renderer_Count(const MeshModelClass * mmc) const;
	void						Update_Mesh_Texture(MeshModelClass * mmc, TextureClass * texture,
							TextureClass * new_texture, unsigned pass, unsigned stage);
	void						Update_Mesh_Material(MeshModelClass * mmc, VertexMaterialClass * material,
							VertexMaterialClass * new_material, unsigned pass);
	void						Add_Mesh_Render_Tasks(MeshModelClass * mmc, MeshClass * instance);
	void						Add_Mesh_Material_Pass(MeshModelClass * mmc, MaterialPassClass * pass,
							MeshClass * instance, bool delayed);
	void						Add_Mesh_Skin(MeshModelClass * mmc, MeshClass * instance);
	void						Render_Mesh_Pass(MeshModelClass * mmc, int base_vertex_offset);
	void						Set_Camera(CameraClass* cam) { camera=cam; }
	CameraClass *			Peek_Camera()	{ return camera; }
	void						Add_To_Render_List(DecalMeshClass * decalmesh);

	// Enable or disable lighting on all objects inserted from now on. (Doesn't affect the objects that are already in the lists)
	void						Enable_Lighting(bool enable) { enable_lighting=enable; }

	// This should be called at the beginning of a game or menu or after a major modifications to the scene...
	void						Invalidate(bool shutdown=false);	// Added flag so it doesn't allocate more mem when shutting down. -MW

protected:

	void Render_Decal_Meshes();

	bool													enable_lighting;
	CameraClass *										camera;

	SimpleDynVecClass<FVFCategoryList *>		texture_category_container_lists_rigid;
	FVFCategoryList *									texture_category_container_list_skin;

	DecalMeshClass *									visible_decal_meshes;


};

extern MeshRendererClass TheMeshRenderer;
