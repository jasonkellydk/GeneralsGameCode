module;

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

export module Graphics.Resources.Bindless.BindlessResourceTable;

export import Graphics.RHI;

namespace Graphics
{

export class BindlessResourceTable final
{
public:
	BindlessResourceTable() = default;
	~BindlessResourceTable() = default;

	BindlessResourceTable(const BindlessResourceTable &) = delete;
	BindlessResourceTable &operator=(const BindlessResourceTable &) = delete;
	BindlessResourceTable(BindlessResourceTable &&) noexcept = default;
	BindlessResourceTable &operator=(BindlessResourceTable &&) noexcept = default;

	void Reserve(
		std::size_t resource_capacity,
		std::size_t buffer_capacity = 0,
		std::size_t texture_capacity = 0,
		std::size_t sampler_capacity = 0,
		std::size_t material_capacity = 0)
	{
		m_resources.reserve(resource_capacity);
		m_free_next.reserve(resource_capacity);
		m_buffers.reserve(buffer_capacity);
		m_textures.reserve(texture_capacity);
		m_samplers.reserve(sampler_capacity);
		m_materials.reserve(material_capacity);
	}

	ResourceIndex Register_Buffer(RHIBufferHandle handle)
	{
		if (!handle.Is_Valid())
			return {};

		RHIBindlessResource resource;
		resource.type = RHIResourceType::Buffer;
		resource.buffer = handle;
		return Register_Handle(m_buffers, handle, resource);
	}

	ResourceIndex Register_Texture(TextureHandle handle, RHITextureHandle texture)
	{
		if (!handle.Is_Valid() || !texture.Is_Valid())
			return {};

		RHIBindlessResource resource;
		resource.type = RHIResourceType::Texture;
		resource.texture = texture;
		return Register_Handle(m_textures, handle, resource);
	}

	ResourceIndex Register_Sampler(SamplerHandle handle)
	{
		if (!handle.Is_Valid())
			return {};

		RHIBindlessResource resource;
		resource.type = RHIResourceType::Sampler;
		return Register_Handle(m_samplers, handle, resource);
	}

	ResourceIndex Register_Material(MaterialHandle handle, RHIBufferHandle constants)
	{
		if (!handle.Is_Valid() || !constants.Is_Valid())
			return {};

		RHIBindlessResource resource;
		resource.type = RHIResourceType::Material;
		resource.buffer = constants;
		return Register_Handle(m_materials, handle, resource);
	}

	bool Update_Buffer(RHIBufferHandle handle, RHIBufferHandle buffer) noexcept
	{
		if (!buffer.Is_Valid())
			return false;

		RHIBindlessResource resource;
		resource.type = RHIResourceType::Buffer;
		resource.buffer = buffer;
		return Update_Handle(m_buffers, handle, resource);
	}

	bool Update_Texture(TextureHandle handle, RHITextureHandle texture) noexcept
	{
		if (!texture.Is_Valid())
			return false;

		RHIBindlessResource resource;
		resource.type = RHIResourceType::Texture;
		resource.texture = texture;
		return Update_Handle(m_textures, handle, resource);
	}

	bool Update_Sampler(SamplerHandle handle) noexcept
	{
		RHIBindlessResource resource;
		resource.type = RHIResourceType::Sampler;
		return Update_Handle(m_samplers, handle, resource);
	}

	bool Update_Material(MaterialHandle handle, RHIBufferHandle constants) noexcept
	{
		if (!constants.Is_Valid())
			return false;

		RHIBindlessResource resource;
		resource.type = RHIResourceType::Material;
		resource.buffer = constants;
		return Update_Handle(m_materials, handle, resource);
	}

	bool Destroy_Buffer(RHIBufferHandle handle) noexcept
	{
		return Destroy_Handle(m_buffers, handle);
	}

	bool Destroy_Texture(TextureHandle handle) noexcept
	{
		return Destroy_Handle(m_textures, handle);
	}

	bool Destroy_Sampler(SamplerHandle handle) noexcept
	{
		return Destroy_Handle(m_samplers, handle);
	}

	bool Destroy_Material(MaterialHandle handle) noexcept
	{
		return Destroy_Handle(m_materials, handle);
	}

	ResourceIndex Buffer_Index(RHIBufferHandle handle) const noexcept
	{
		return Find_Index(m_buffers, handle);
	}

	ResourceIndex Texture_Index(TextureHandle handle) const noexcept
	{
		return Find_Index(m_textures, handle);
	}

	ResourceIndex Sampler_Index(SamplerHandle handle) const noexcept
	{
		return Find_Index(m_samplers, handle);
	}

	ResourceIndex Material_Index(MaterialHandle handle) const noexcept
	{
		return Find_Index(m_materials, handle);
	}

	RHIBindlessResource Resolve(ResourceIndex index) const noexcept
	{
		if (!Is_Valid(index))
			return {};

		return m_resources[index.Get_Index()];
	}

	bool Is_Valid(ResourceIndex index) const noexcept
	{
		if (!index.Is_Valid() || index.Get_Index() >= m_resources.size())
			return false;

		const RHIBindlessResource &resource = m_resources[index.Get_Index()];
		return resource.index == index && resource.type != RHIResourceType::Invalid;
	}

	std::span<const RHIBindlessResource> Resources() const noexcept
	{
		return m_resources;
	}

	void Clear() noexcept
	{
		for (std::size_t index = 0; index < m_resources.size(); ++index) {
			RHIBindlessResource &resource = m_resources[index];
			if (resource.type != RHIResourceType::Invalid)
				resource.index = ResourceIndex(static_cast<std::uint32_t>(index), Next_Generation(resource.index.Get_Generation()));
			resource.type = RHIResourceType::Invalid;
			resource.buffer = {};
			resource.texture = {};
			m_free_next[index] = index + 1 < m_resources.size() ? static_cast<std::uint32_t>(index + 1) : Invalid_Index;
		}

		m_free_head = m_resources.empty() ? Invalid_Index : 0;
		Clear_Mappings(m_buffers);
		Clear_Mappings(m_textures);
		Clear_Mappings(m_samplers);
		Clear_Mappings(m_materials);
	}

private:
	static constexpr std::uint32_t Invalid_Index = std::numeric_limits<std::uint32_t>::max();

	struct MappingSlot final
	{
		std::uint32_t generation = 0;
		std::uint32_t last_generation = 0;
		ResourceIndex index{};
	};

	static std::uint32_t Next_Generation(std::uint32_t generation) noexcept
	{
		const std::uint32_t next = generation + 1;
		return next == 0 ? 1 : next;
	}

	ResourceIndex Allocate_Index()
	{
		if (m_free_head != Invalid_Index) {
			const std::uint32_t slot = m_free_head;
			m_free_head = m_free_next[slot];
			const ResourceIndex index = ResourceIndex(slot, Next_Generation(m_resources[slot].index.Get_Generation()));
			m_resources[slot] = {index, RHIResourceType::Invalid, {}, {}};
			m_free_next[slot] = Invalid_Index;
			return index;
		}

		const std::uint32_t slot = static_cast<std::uint32_t>(m_resources.size());
		const ResourceIndex index(slot, 1);
		m_resources.push_back({index, RHIResourceType::Invalid, {}, {}});
		m_free_next.push_back(Invalid_Index);
		return index;
	}

	bool Set_Resource(ResourceIndex index, RHIBindlessResource resource) noexcept
	{
		if (!index.Is_Valid() || index.Get_Index() >= m_resources.size())
			return false;

		RHIBindlessResource &destination = m_resources[index.Get_Index()];
		if (destination.index != index)
			return false;

		resource.index = index;
		destination = resource;
		return true;
	}

	template <typename Handle>
	ResourceIndex Register_Handle(std::vector<MappingSlot> &mappings, Handle handle, RHIBindlessResource resource)
	{
		MappingSlot &mapping = Mapping(mappings, handle);
		if (mapping.generation != 0) {
			if (mapping.generation != handle.Get_Generation() || !mapping.index.Is_Valid())
				return {};

			return Set_Resource(mapping.index, resource) ? mapping.index : ResourceIndex{};
		}

		if (mapping.last_generation == handle.Get_Generation())
			return {};

		const ResourceIndex index = Allocate_Index();
		if (!Set_Resource(index, resource))
			return {};

		mapping.generation = handle.Get_Generation();
		mapping.index = index;
		return index;
	}

	template <typename Handle>
	bool Update_Handle(const std::vector<MappingSlot> &mappings, Handle handle, RHIBindlessResource resource) noexcept
	{
		if (!handle.Is_Valid() || handle.Get_Index() >= mappings.size())
			return false;

		const MappingSlot &mapping = mappings[handle.Get_Index()];
		return mapping.generation == handle.Get_Generation() && Set_Resource(mapping.index, resource);
	}

	template <typename Handle>
	bool Destroy_Handle(std::vector<MappingSlot> &mappings, Handle handle) noexcept
	{
		if (!handle.Is_Valid() || handle.Get_Index() >= mappings.size())
			return false;

		MappingSlot &mapping = mappings[handle.Get_Index()];
		if (mapping.generation != handle.Get_Generation() || !mapping.index.Is_Valid())
			return false;

		Release_Index(mapping.index);
		mapping.last_generation = mapping.generation;
		mapping.generation = 0;
		mapping.index = {};
		return true;
	}

	template <typename Handle>
	static MappingSlot &Mapping(std::vector<MappingSlot> &mappings, Handle handle)
	{
		if (mappings.size() <= handle.Get_Index())
			mappings.resize(static_cast<std::size_t>(handle.Get_Index()) + 1);
		return mappings[handle.Get_Index()];
	}

	template <typename Handle>
	static ResourceIndex Find_Index(const std::vector<MappingSlot> &mappings, Handle handle) noexcept
	{
		if (!handle.Is_Valid() || handle.Get_Index() >= mappings.size())
			return {};

		const MappingSlot &mapping = mappings[handle.Get_Index()];
		return mapping.generation == handle.Get_Generation() ? mapping.index : ResourceIndex{};
	}

	void Release_Index(ResourceIndex index) noexcept
	{
		if (!Is_Valid(index))
			return;

		RHIBindlessResource &resource = m_resources[index.Get_Index()];
		resource.index = ResourceIndex(index.Get_Index(), Next_Generation(index.Get_Generation()));
		resource.type = RHIResourceType::Invalid;
		resource.buffer = {};
		resource.texture = {};
		m_free_next[index.Get_Index()] = m_free_head;
		m_free_head = index.Get_Index();
	}

	static void Clear_Mappings(std::vector<MappingSlot> &mappings) noexcept
	{
		for (MappingSlot &mapping : mappings)
			mapping = {};
	}

	std::vector<RHIBindlessResource> m_resources;
	std::vector<std::uint32_t> m_free_next;
	std::vector<MappingSlot> m_buffers;
	std::vector<MappingSlot> m_textures;
	std::vector<MappingSlot> m_samplers;
	std::vector<MappingSlot> m_materials;
	std::uint32_t m_free_head = Invalid_Index;
};

}
