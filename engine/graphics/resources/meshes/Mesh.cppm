module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

export module Graphics.Resources.Meshes.Mesh;

export import Graphics.Resources.Handles.ResourceHandle;
export import Graphics.Resources.Pools.ResourcePool;

namespace Graphics
{

export enum class MeshIndexFormat : std::uint8_t
{
	None,
	UInt16,
	UInt32
};

export struct MeshLod final
{
	MeshHandle mesh{};
	float max_screen_size = 0.0f;
};

export struct Mesh final
{
	using Count = std::uint32_t;
	using Stride = std::uint32_t;
	static constexpr std::uint8_t MaxLodCount = 4;

	Count vertex_count = 0;
	Count index_count = 0;
	Stride vertex_stride = 0;
	MeshIndexFormat index_format = MeshIndexFormat::None;
	std::array<MeshLod, MaxLodCount> lods{};
	std::uint8_t lod_count = 0;
	std::span<const std::byte> vertex_data{};
	std::span<const std::byte> index_data{};
	std::uint32_t revision = 1;

	void Mark_Dirty() noexcept
	{
		++revision;
		if (revision == 0)
			revision = 1;
	}
};

export using MeshPool = ResourcePool<Mesh, MeshHandle>;

}
