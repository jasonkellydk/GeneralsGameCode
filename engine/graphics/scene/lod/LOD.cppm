module;

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

export module Graphics.Scene.LOD;

export import Graphics.Resources.Meshes.Mesh;
export import Graphics.Scene.RenderScene;
export import Graphics.Scene.Visibility;
export import Graphics.Scene.Views.View;

namespace Graphics
{

export struct LODSelection final
{
	InstanceHandle instance{};
	MeshHandle mesh{};
	std::uint32_t lod_index = 0;
};

export class LODSet;

export bool Build_LOD_Set(const RenderScene &scene, const MeshPool &meshes, const VisibleSet &visible_set, const View &view, LODSet &lod_set) noexcept;

export class LODSet final
{
public:
	explicit LODSet(std::span<LODSelection> storage) noexcept
		: m_storage(storage)
	{
	}

	void Clear() noexcept
	{
		m_count = 0;
	}

	std::size_t Size() const noexcept
	{
		return m_count;
	}

	std::span<const LODSelection> Selections() const noexcept
	{
		return {m_storage.data(), m_count};
	}

private:
	friend bool Build_LOD_Set(const RenderScene &scene, const MeshPool &meshes, const VisibleSet &visible_set, const View &view, LODSet &lod_set) noexcept;

	bool Try_Append(LODSelection selection) noexcept
	{
		if (m_count >= m_storage.size())
			return false;

		m_storage[m_count++] = selection;
		return true;
	}

	std::span<LODSelection> m_storage;
	std::size_t m_count = 0;
};

namespace
{
bool Project_Y(const Matrix4x4 &projection_matrix, float view_x, float view_y, float view_z, float &projected_y) noexcept
{
	const float clip_y = projection_matrix(1, 0) * view_x
		+ projection_matrix(1, 1) * view_y
		+ projection_matrix(1, 2) * view_z
		+ projection_matrix(1, 3);
	const float clip_w = projection_matrix(3, 0) * view_x
		+ projection_matrix(3, 1) * view_y
		+ projection_matrix(3, 2) * view_z
		+ projection_matrix(3, 3);
	if (clip_w == 0.0f)
		return false;

	projected_y = clip_y / clip_w;
	return true;
}

float Projected_Screen_Size(const View &view, const RenderWorldBoundsData &bounds, std::size_t dense_index) noexcept
{
	const float world_x = bounds.center_x[dense_index];
	const float world_y = bounds.center_y[dense_index];
	const float world_z = bounds.center_z[dense_index];
	const float world_radius = bounds.radii[dense_index];
	const float view_x = view.view_matrix(0, 0) * world_x
		+ view.view_matrix(0, 1) * world_y
		+ view.view_matrix(0, 2) * world_z
		+ view.view_matrix(0, 3);
	const float view_y = view.view_matrix(1, 0) * world_x
		+ view.view_matrix(1, 1) * world_y
		+ view.view_matrix(1, 2) * world_z
		+ view.view_matrix(1, 3);
	const float view_z = view.view_matrix(2, 0) * world_x
		+ view.view_matrix(2, 1) * world_y
		+ view.view_matrix(2, 2) * world_z
		+ view.view_matrix(2, 3);

	float projected_top = 0.0f;
	float projected_bottom = 0.0f;
	if (!Project_Y(view.projection_matrix, view_x, view_y + world_radius, view_z, projected_top)
		|| !Project_Y(view.projection_matrix, view_x, view_y - world_radius, view_z, projected_bottom))
		return std::numeric_limits<float>::max();

	return std::abs(projected_top - projected_bottom);
}

LODSelection Select_LOD(InstanceHandle instance_handle, MeshHandle instance_mesh, const MeshPool &meshes, const Mesh &mesh, float screen_size) noexcept
{
	LODSelection selection{instance_handle, instance_mesh, 0};

	const std::uint32_t lod_count = mesh.lod_count < Mesh::MaxLodCount ? mesh.lod_count : Mesh::MaxLodCount;
	for (std::uint32_t lod_index = 0; lod_index < lod_count; ++lod_index) {
		const MeshLod &lod = mesh.lods[lod_index];
		if (screen_size > lod.max_screen_size)
			break;
		if (lod.mesh.Is_Valid() && meshes.Resolve(lod.mesh) != nullptr) {
			selection.mesh = lod.mesh;
			selection.lod_index = lod_index + 1;
		}
	}

	return selection;
}
}

export bool Build_LOD_Set(const RenderScene &scene, const MeshPool &meshes, const VisibleSet &visible_set, const View &view, LODSet &lod_set) noexcept
{
	lod_set.Clear();
	const RenderSceneData scene_data = scene.Data();

	for (const InstanceHandle instance_handle : visible_set.Handles()) {
		const std::uint32_t dense_index = scene.Dense_Index(instance_handle);
		if (dense_index == Invalid_Render_Scene_Index || dense_index >= scene_data.Size())
			continue;

		const MeshHandle instance_mesh = scene_data.meshes[dense_index];
		const Mesh *mesh = meshes.Resolve(instance_mesh);
		if (mesh == nullptr)
			continue;

		const float screen_size = Projected_Screen_Size(view, scene_data.world_bounds, dense_index);
		if (!lod_set.Try_Append(Select_LOD(instance_handle, instance_mesh, meshes, *mesh, screen_size))) {
			lod_set.Clear();
			return false;
		}
	}

	return true;
}

}
