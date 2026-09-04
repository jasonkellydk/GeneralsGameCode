module;

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

export module engine.ecs.storage.archetype;

export import engine.ecs.core.component_registry;
export import engine.ecs.storage.chunk;
export import engine.ecs.storage.signature;

export namespace ecs
{

class Archetype
{
public:
	struct Slot
	{
		Chunk *chunk{nullptr};
		std::size_t row{0};
	};

	Archetype(Signature signature,
		std::vector<const ComponentInfo *> components,
		std::size_t chunkTargetBytes);

	const Signature &GetSignature() const noexcept { return m_signature; }
	const ChunkLayout &Layout() const noexcept { return m_layout; }
	const std::vector<std::unique_ptr<Chunk>> &Chunks() const noexcept { return m_chunks; }

	bool Has(ComponentId component) const noexcept;
	std::size_t ColumnIndex(ComponentId component) const noexcept;
	std::size_t EntityCount() const noexcept { return m_entityCount; }

	Slot AddEntity(Entity entity);
	Entity RemoveEntity(Chunk *chunk, std::size_t row);

private:
	Signature m_signature;
	std::vector<const ComponentInfo *> m_components;
	ChunkLayout m_layout;
	std::vector<std::unique_ptr<Chunk>> m_chunks;
	std::size_t m_entityCount{0};
};

class ArchetypeRegistry
{
public:
	Archetype &GetOrCreate(const Signature &signature,
		const ComponentRegistry &components,
		std::size_t chunkTargetBytes,
		bool *created = nullptr);

	std::vector<Archetype *> GetArchetypes();
	std::vector<const Archetype *> GetArchetypes() const;
	std::size_t Count() const noexcept { return m_archetypes.size(); }
	std::uint64_t Revision() const noexcept { return m_revision; }

private:
	std::map<Signature, std::unique_ptr<Archetype>, SignatureLess> m_archetypes;
	std::uint64_t m_revision{0};
};

} // namespace ecs

namespace ecs
{

Archetype::Archetype(Signature signature,
	std::vector<const ComponentInfo *> components,
	std::size_t chunkTargetBytes) :
	m_signature(std::move(signature)),
	m_components(std::move(components)),
	m_layout(ChunkLayout::Build(m_components, chunkTargetBytes))
{
}

bool Archetype::Has(ComponentId component) const noexcept
{
	return std::binary_search(m_signature.begin(), m_signature.end(), component);
}

std::size_t Archetype::ColumnIndex(ComponentId component) const noexcept
{
	const auto found = std::lower_bound(m_signature.begin(), m_signature.end(), component);
	if (found == m_signature.end() || *found != component)
		return std::numeric_limits<std::size_t>::max();
	return static_cast<std::size_t>(found - m_signature.begin());
}

Archetype::Slot Archetype::AddEntity(Entity entity)
{
	Chunk *target = nullptr;
	for (const std::unique_ptr<Chunk> &chunk : m_chunks)
	{
		if (!chunk->IsFull())
		{
			target = chunk.get();
			break;
		}
	}

	if (target == nullptr)
	{
		m_chunks.push_back(std::make_unique<Chunk>(ChunkLayout(m_layout), m_components));
		target = m_chunks.back().get();
	}

	const std::size_t row = target->Size();
	target->AddEntity(entity);
	++m_entityCount;
	return Slot{target, row};
}

Entity Archetype::RemoveEntity(Chunk *chunk, std::size_t row)
{
	assert(chunk != nullptr);
	assert(std::find_if(m_chunks.begin(), m_chunks.end(), [chunk](const std::unique_ptr<Chunk> &candidate) {
		return candidate.get() == chunk;
	}) != m_chunks.end());

	const Entity moved = chunk->RemoveSwap(row);
	assert(m_entityCount > 0);
	--m_entityCount;
	return moved;
}

Archetype &ArchetypeRegistry::GetOrCreate(const Signature &signature,
	const ComponentRegistry &components,
	std::size_t chunkTargetBytes,
	bool *created)
{
	if (!components.IsFrozen())
		throw std::logic_error("ECS component registry must be finalized before creating archetypes");

	Signature canonicalSignature = signature;
	CanonicalizeSignature(canonicalSignature);

	const auto found = m_archetypes.find(canonicalSignature);
	if (found != m_archetypes.end())
	{
		if (created != nullptr)
			*created = false;
		return *found->second;
	}

	std::vector<const ComponentInfo *> infos;
	infos.reserve(canonicalSignature.size());
	for (ComponentId component : canonicalSignature)
		infos.push_back(&components.Get(component));

	auto archetype = std::make_unique<Archetype>(std::move(canonicalSignature), std::move(infos), chunkTargetBytes);
	Archetype *result = archetype.get();
	m_archetypes.emplace(result->GetSignature(), std::move(archetype));
	++m_revision;
	if (created != nullptr)
		*created = true;
	return *result;
}

std::vector<Archetype *> ArchetypeRegistry::GetArchetypes()
{
	std::vector<Archetype *> result;
	result.reserve(m_archetypes.size());
	for (const auto &entry : m_archetypes)
		result.push_back(entry.second.get());
	return result;
}

std::vector<const Archetype *> ArchetypeRegistry::GetArchetypes() const
{
	std::vector<const Archetype *> result;
	result.reserve(m_archetypes.size());
	for (const auto &entry : m_archetypes)
		result.push_back(entry.second.get());
	return result;
}

} // namespace ecs
