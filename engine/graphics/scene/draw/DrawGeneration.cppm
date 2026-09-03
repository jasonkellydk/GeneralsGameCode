module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

export module Graphics.Scene.DrawGeneration;

export import Graphics.Scene.GPUScene;
export import Graphics.Scene.LOD;
export import Graphics.Shaders.Pipeline;

export struct DrawPass final
{
	std::uint32_t pass_key = 0;
	PipelineHandle pipeline{};
	std::uint64_t sort_key = 0;
};

export struct alignas(16) DrawData final
{
	std::uint32_t mesh_index = Invalid_GPU_Index;
	std::uint32_t material_index = Invalid_GPU_Index;
	std::uint32_t instance_index = Invalid_GPU_Index;
	std::uint32_t instance_count = 1;
	PipelineHandle pipeline{};
	std::uint64_t sort_key = 0;
};

static_assert(sizeof(DrawData) == 32);

export class DrawSet;

export bool Build_Draw_Data(const LODSet &lod_set, const GPUScene &gpu_scene, DrawPass pass, DrawSet &draw_set) noexcept;

export class DrawSet final
{
public:
	explicit DrawSet(std::span<DrawData> storage) noexcept
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
	friend bool Build_Draw_Data(const LODSet &lod_set, const GPUScene &gpu_scene, DrawPass pass, DrawSet &draw_set) noexcept;

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
			if (left.pipeline.Get_Index() != right.pipeline.Get_Index())
				return left.pipeline.Get_Index() < right.pipeline.Get_Index();
			if (left.pipeline.Get_Generation() != right.pipeline.Get_Generation())
				return left.pipeline.Get_Generation() < right.pipeline.Get_Generation();
			if (left.material_index != right.material_index)
				return left.material_index < right.material_index;
			if (left.mesh_index != right.mesh_index)
				return left.mesh_index < right.mesh_index;
			if (left.sort_key != right.sort_key)
				return left.sort_key < right.sort_key;
			if (left.instance_count != right.instance_count)
				return left.instance_count < right.instance_count;
			return left.instance_index < right.instance_index;
		});
	}

	std::span<DrawData> m_storage;
	std::size_t m_count = 0;
};

export bool Build_Draw_Data(const LODSet &lod_set, const GPUScene &gpu_scene, DrawPass pass, DrawSet &draw_set) noexcept
{
	draw_set.Clear();
	if (!pass.pipeline.Is_Valid())
		return false;

	const std::span<const GPUInstanceData> instances = gpu_scene.Instances();
	const std::span<const GPUMaterialData> materials = gpu_scene.Materials();

	for (const LODSelection &selection : lod_set.Selections()) {
		const std::uint32_t mesh_index = gpu_scene.Mesh_Index(selection.mesh);
		const std::uint32_t instance_index = gpu_scene.Instance_Index(selection.instance);
		if (mesh_index == Invalid_GPU_Index || instance_index == Invalid_GPU_Index || instance_index >= instances.size())
			continue;

		const std::uint32_t material_index = instances[instance_index].material_index;
		if (material_index == Invalid_GPU_Index || material_index >= materials.size())
			continue;

		if (!draw_set.Try_Append({
			mesh_index,
			material_index,
			instance_index,
			1,
			pass.pipeline,
			pass.sort_key
		})) {
			draw_set.Clear();
			return false;
		}
	}

	draw_set.Sort();
	return true;
}
