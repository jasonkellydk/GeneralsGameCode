module;

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <span>

export module Graphics.Scene.Transparency;

export import Graphics.Resources.Materials.Material;
export import Graphics.Scene.DrawGeneration;
export import Graphics.Scene.LOD;
export import Graphics.Scene.RenderScene;
export import Graphics.Scene.Views.View;

export constexpr bool Is_Transparent_Material(MaterialFlags flags) noexcept
{
	return Has_Material_Flag(flags, MaterialFlags::Transparent);
}

export constexpr bool Is_Transparent_Material(const Material &material) noexcept
{
	return Is_Transparent_Material(material.flags);
}

export PipelineDesc Make_Transparent_Pipeline(PipelineDesc description) noexcept
{
	description.depth_test = true;
	description.depth_write = false;
	description.blend_mode = RHIBlendMode::Alpha;
	return description;
}

export class TransparentDrawSet final
{
public:
	explicit TransparentDrawSet(std::span<DrawData> storage) noexcept
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

	std::span<const DrawData> Records() const noexcept
	{
		return {m_storage.data(), m_count};
	}

private:
	friend bool Build_Transparent_Draw_Data(const RenderScene &, const MaterialPool &, const LODSet &, const View &, const GPUScene &, DrawPass, TransparentDrawSet &) noexcept;

	bool Try_Append(DrawData data) noexcept
	{
		if (m_count >= m_storage.size())
			return false;

		m_storage[m_count++] = data;
		return true;
	}

	void Sort() noexcept
	{
		std::sort(m_storage.begin(), m_storage.begin() + m_count, [](const DrawData &left, const DrawData &right) noexcept {
			if (left.sort_key != right.sort_key)
				return left.sort_key < right.sort_key;
			if (left.pipeline.Get_Index() != right.pipeline.Get_Index())
				return left.pipeline.Get_Index() < right.pipeline.Get_Index();
			if (left.pipeline.Get_Generation() != right.pipeline.Get_Generation())
				return left.pipeline.Get_Generation() < right.pipeline.Get_Generation();
			if (left.material_index != right.material_index)
				return left.material_index < right.material_index;
			if (left.mesh_index != right.mesh_index)
				return left.mesh_index < right.mesh_index;
			if (left.instance_count != right.instance_count)
				return left.instance_count < right.instance_count;
			return left.instance_index < right.instance_index;
		});
	}

	std::span<DrawData> m_storage;
	std::size_t m_count = 0;
};

export bool Build_Transparent_Draw_Data(
	const RenderScene &scene,
	const MaterialPool &materials,
	const LODSet &lod_set,
	const View &view,
	const GPUScene &gpu_scene,
	DrawPass pass,
	TransparentDrawSet &draw_set) noexcept;

namespace
{
std::uint32_t Depth_Sort_Key(const View &view, const RenderWorldBoundsData &bounds, std::size_t dense_index) noexcept
{
	const float view_z = view.view_matrix(2, 0) * bounds.center_x[dense_index]
		+ view.view_matrix(2, 1) * bounds.center_y[dense_index]
		+ view.view_matrix(2, 2) * bounds.center_z[dense_index]
		+ view.view_matrix(2, 3);
	const float depth = -view_z;
	if (!std::isfinite(depth) || depth <= 0.0f)
		return std::numeric_limits<std::uint32_t>::max();

	return std::numeric_limits<std::uint32_t>::max() - std::bit_cast<std::uint32_t>(depth);
}

std::uint64_t Make_Sort_Key(const View &view, const RenderWorldBoundsData &bounds, std::size_t dense_index, std::uint64_t pass_sort_key) noexcept
{
	return (static_cast<std::uint64_t>(Depth_Sort_Key(view, bounds, dense_index)) << 32)
		| static_cast<std::uint32_t>(pass_sort_key);
}
}

export bool Build_Transparent_Draw_Data(
	const RenderScene &scene,
	const MaterialPool &materials,
	const LODSet &lod_set,
	const View &view,
	const GPUScene &gpu_scene,
	DrawPass pass,
	TransparentDrawSet &draw_set) noexcept
{
	draw_set.Clear();
	if (!pass.pipeline.Is_Valid())
		return false;

	const RenderSceneData scene_data = scene.Data();
	const std::span<const GPUInstanceData> instances = gpu_scene.Instances();
	const std::span<const GPUMaterialData> gpu_materials = gpu_scene.Materials();
	for (const LODSelection &selection : lod_set.Selections()) {
		const std::uint32_t scene_index = scene.Dense_Index(selection.instance);
		if (scene_index == Invalid_Render_Scene_Index || scene_index >= scene_data.Size())
			continue;

		const MaterialHandle material_handle = scene_data.materials[scene_index];
		const Material *material = materials.Resolve(material_handle);
		if (material == nullptr || !Is_Transparent_Material(*material))
			continue;

		const std::uint32_t mesh_index = gpu_scene.Mesh_Index(selection.mesh);
		const std::uint32_t instance_index = gpu_scene.Instance_Index(selection.instance);
		if (mesh_index == Invalid_GPU_Index || instance_index == Invalid_GPU_Index || instance_index >= instances.size())
			continue;

		const std::uint32_t material_index = instances[instance_index].material_index;
		if (material_index == Invalid_GPU_Index || material_index >= gpu_materials.size())
			continue;

		const DrawData draw{
			mesh_index,
			material_index,
			instance_index,
			1,
			pass.pipeline,
			Make_Sort_Key(view, scene_data.world_bounds, scene_index, pass.sort_key)
		};
		if (!draw_set.Try_Append(draw)) {
			draw_set.Clear();
			return false;
		}
	}

	draw_set.Sort();
	return true;
}
