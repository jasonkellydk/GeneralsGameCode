module;

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

export module engine.ecs.core.world;

export import engine.ecs.core.component_registry;
export import engine.ecs.core.entity;
export import engine.ecs.storage.archetype;

export namespace ecs
{

struct WorldConfig
{
	std::size_t chunkTargetBytes{ChunkLayout::DefaultTargetBytes};
};

class World
{
public:
	explicit World(WorldConfig config = {});
	~World() = default;

	World(const World &) = delete;
	World &operator=(const World &) = delete;

	template<typename... Components>
	Entity Create()
	{
		Signature signature;
		signature.reserve(sizeof...(Components));
		(signature.push_back(m_components.Register<Components>()), ...);
		CanonicalizeSignature(signature);

		const Entity entity = AllocateEntity();
		try
		{
			Archetype &archetype = GetOrCreateArchetype(signature);
			const Archetype::Slot slot = archetype.AddEntity(entity);
			SetLocation(entity, EntityLocation{&archetype, slot.chunk, slot.row});
		}
		catch (...)
		{
			ReleaseUnconstructedEntity(entity);
			throw;
		}
		return entity;
	}

	template<typename T>
	ComponentId RegisterComponent()
	{
		return m_components.Register<T>();
	}

	bool IsAlive(Entity entity) const noexcept;
	bool Destroy(Entity entity);

	template<typename T>
	bool Has(Entity entity) const noexcept
	{
		if (!IsAlive(entity))
			return false;
		const ComponentId component = m_components.TryGet<T>();
		return component != InvalidComponentId && m_records[entity.index].location.archetype->Has(component);
	}

	template<typename T>
	bool Add(Entity entity)
	{
		if (!IsAlive(entity))
			return false;

		const ComponentId component = m_components.Register<T>();
		EntityRecord &record = m_records[entity.index];
		if (record.location.archetype->Has(component))
			return false;

		Signature signature = record.location.archetype->GetSignature();
		signature.push_back(component);
		CanonicalizeSignature(signature);
		MoveEntity(entity, signature);
		return true;
	}

	template<typename T>
	bool Remove(Entity entity)
	{
		if (!IsAlive(entity))
			return false;

		const ComponentId component = m_components.TryGet<T>();
		if (component == InvalidComponentId)
			return false;

		EntityRecord &record = m_records[entity.index];
		if (!record.location.archetype->Has(component))
			return false;

		Signature signature = record.location.archetype->GetSignature();
		signature.erase(std::remove(signature.begin(), signature.end(), component), signature.end());
		MoveEntity(entity, signature);
		return true;
	}

	template<typename T>
	T *Get(Entity entity) noexcept
	{
		const ComponentId component = m_components.TryGet<T>();
		if (component == InvalidComponentId || !IsAlive(entity))
			return nullptr;

		EntityRecord &record = m_records[entity.index];
		const std::size_t column = record.location.archetype->ColumnIndex(component);
		if (column == std::numeric_limits<std::size_t>::max())
			return nullptr;
		return static_cast<T *>(record.location.chunk->ComponentData(column)) + record.location.row;
	}

	template<typename T>
	const T *Get(Entity entity) const noexcept
	{
		const ComponentId component = m_components.TryGet<T>();
		if (component == InvalidComponentId || !IsAlive(entity))
			return nullptr;

		const EntityRecord &record = m_records[entity.index];
		const std::size_t column = record.location.archetype->ColumnIndex(component);
		if (column == std::numeric_limits<std::size_t>::max())
			return nullptr;
		return static_cast<const T *>(record.location.chunk->ComponentData(column)) + record.location.row;
	}

	std::size_t EntityCount() const noexcept { return m_entityCount; }
	std::size_t ArchetypeCount() const noexcept { return m_archetypes.Count(); }
	std::uint64_t ArchetypeRevision() const noexcept { return m_archetypes.Revision(); }
	std::vector<Archetype *> GetArchetypes() { return m_archetypes.GetArchetypes(); }
	std::vector<const Archetype *> GetArchetypes() const { return m_archetypes.GetArchetypes(); }
	const ComponentRegistry &Components() const noexcept { return m_components; }

private:
	struct EntityLocation
	{
		Archetype *archetype{nullptr};
		Chunk *chunk{nullptr};
		std::size_t row{0};
	};

	struct EntityRecord
	{
		EntityGeneration generation{1};
		bool alive{false};
		EntityLocation location{};
	};

	Entity AllocateEntity();
	void ReleaseUnconstructedEntity(Entity entity) noexcept;
	void SetLocation(Entity entity, EntityLocation location) noexcept;
	Archetype &GetOrCreateArchetype(const Signature &signature);
	void MoveEntity(Entity entity, const Signature &targetSignature);

	WorldConfig m_config;
	ComponentRegistry m_components;
	ArchetypeRegistry m_archetypes;
	std::vector<EntityRecord> m_records;
	std::vector<EntityIndex> m_freeIndices;
	std::size_t m_entityCount{0};
};

} // namespace ecs

namespace ecs
{

World::World(WorldConfig config) :
	m_config(config)
{
	GetOrCreateArchetype(Signature{});
}

Entity World::AllocateEntity()
{
	if (!m_freeIndices.empty())
	{
		const EntityIndex index = m_freeIndices.back();
		m_freeIndices.pop_back();
		EntityRecord &record = m_records[index];
		record.alive = true;
		record.location = EntityLocation{};
		++m_entityCount;
		return Entity{index, record.generation};
	}

	if (m_records.size() >= static_cast<std::size_t>(Entity::InvalidIndex))
		throw std::length_error("ECS entity registry exhausted");

	const EntityIndex index = static_cast<EntityIndex>(m_records.size());
	m_records.push_back(EntityRecord{});
	EntityRecord &record = m_records.back();
	record.alive = true;
	++m_entityCount;
	return Entity{index, record.generation};
}

void World::ReleaseUnconstructedEntity(Entity entity) noexcept
{
	assert(entity.index < m_records.size());
	EntityRecord &record = m_records[entity.index];
	record.alive = false;
	record.location = EntityLocation{};
	assert(m_entityCount > 0);
	--m_entityCount;
	m_freeIndices.push_back(entity.index);
}

void World::SetLocation(Entity entity, EntityLocation location) noexcept
{
	assert(entity.index < m_records.size());
	EntityRecord &record = m_records[entity.index];
	assert(record.alive && record.generation == entity.generation);
	record.location = location;
}

Archetype &World::GetOrCreateArchetype(const Signature &signature)
{
	return m_archetypes.GetOrCreate(signature, m_components, m_config.chunkTargetBytes);
}

bool World::IsAlive(Entity entity) const noexcept
{
	return entity.IsValid() && entity.index < m_records.size() &&
		m_records[entity.index].alive && m_records[entity.index].generation == entity.generation;
}

bool World::Destroy(Entity entity)
{
	if (!IsAlive(entity))
		return false;

	EntityRecord &record = m_records[entity.index];
	const EntityLocation location = record.location;
	const Entity moved = location.archetype->RemoveEntity(location.chunk, location.row);
	if (moved.IsValid())
	{
		EntityRecord &movedRecord = m_records[moved.index];
		movedRecord.location = EntityLocation{location.archetype, location.chunk, location.row};
	}

	record.alive = false;
	record.location = EntityLocation{};
	if (record.generation == std::numeric_limits<EntityGeneration>::max())
		record.generation = 1;
	else
		++record.generation;
	assert(m_entityCount > 0);
	--m_entityCount;
	m_freeIndices.push_back(entity.index);
	return true;
}

void World::MoveEntity(Entity entity, const Signature &targetSignature)
{
	assert(IsAlive(entity));
	EntityRecord &record = m_records[entity.index];
	const EntityLocation sourceLocation = record.location;
	Archetype *source = sourceLocation.archetype;
	Archetype &target = GetOrCreateArchetype(targetSignature);
	if (source == &target)
		return;

	const Archetype::Slot destination = target.AddEntity(entity);
	for (ComponentId component : source->GetSignature())
	{
		const std::size_t destinationColumn = target.ColumnIndex(component);
		if (destinationColumn == std::numeric_limits<std::size_t>::max())
			continue;

		const std::size_t sourceColumn = source->ColumnIndex(component);
		const ComponentInfo &info = m_components.Get(component);
		void *destinationData = destination.chunk->ComponentData(destinationColumn);
		void *sourceData = sourceLocation.chunk->ComponentData(sourceColumn);
		const ChunkLayout::Column &destinationLayout = target.Layout().Columns()[destinationColumn];
		const ChunkLayout::Column &sourceLayout = source->Layout().Columns()[sourceColumn];
		void *destinationElement = static_cast<std::byte *>(destinationData) + destination.row * destinationLayout.size;
		void *sourceElement = static_cast<std::byte *>(sourceData) + sourceLocation.row * sourceLayout.size;
		info.destroy(destinationElement);
		info.constructMove(destinationElement, sourceElement);
	}

	const Entity moved = source->RemoveEntity(sourceLocation.chunk, sourceLocation.row);
	if (moved.IsValid())
	{
		EntityRecord &movedRecord = m_records[moved.index];
		movedRecord.location = EntityLocation{source, sourceLocation.chunk, sourceLocation.row};
	}
	record.location = EntityLocation{&target, destination.chunk, destination.row};
}

} // namespace ecs
