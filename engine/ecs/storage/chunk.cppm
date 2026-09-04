module;

#include <cassert>
#include <cstddef>
#include <memory>
#include <new>
#include <utility>
#include <vector>

export module engine.ecs.storage.chunk;

export import engine.ecs.core.entity;
export import engine.ecs.core.component_registry;
export import engine.ecs.storage.chunk_layout;

export namespace ecs
{

class Chunk
{
public:
	Chunk(ChunkLayout layout, std::vector<const ComponentInfo *> components);
	~Chunk();

	Chunk(const Chunk &) = delete;
	Chunk &operator=(const Chunk &) = delete;

	std::size_t Size() const noexcept { return m_size; }
	std::size_t Capacity() const noexcept { return m_layout.Capacity(); }
	bool IsFull() const noexcept { return m_size == Capacity(); }

	Entity *Entities() noexcept;
	const Entity *Entities() const noexcept;

	void *ComponentData(std::size_t column) noexcept;
	const void *ComponentData(std::size_t column) const noexcept;

	const ChunkLayout &Layout() const noexcept { return m_layout; }

	void AddEntity(Entity entity);
	Entity RemoveSwap(std::size_t row);

private:
	std::byte *DataAt(std::size_t offset) noexcept;
	const std::byte *DataAt(std::size_t offset) const noexcept;

	ChunkLayout m_layout;
	std::vector<const ComponentInfo *> m_components;
	std::byte *m_data{nullptr};
	std::size_t m_size{0};
};

} // namespace ecs

namespace ecs
{

Chunk::Chunk(ChunkLayout layout, std::vector<const ComponentInfo *> components) :
	m_layout(std::move(layout)),
	m_components(std::move(components))
{
	m_data = static_cast<std::byte *>(::operator new(m_layout.TotalBytes(), std::align_val_t(m_layout.Alignment())));
}

Chunk::~Chunk()
{
	for (std::size_t row = 0; row < m_size; ++row)
	{
		for (const ComponentInfo *info : m_components)
		{
			const ChunkLayout::Column *column = m_layout.Find(info->id);
			info->destroy(DataAt(column->offset + row * column->size));
		}
	}

	::operator delete(m_data, std::align_val_t(m_layout.Alignment()));
}

std::byte *Chunk::DataAt(std::size_t offset) noexcept
{
	return m_data + offset;
}

const std::byte *Chunk::DataAt(std::size_t offset) const noexcept
{
	return m_data + offset;
}

Entity *Chunk::Entities() noexcept
{
	return reinterpret_cast<Entity *>(DataAt(m_layout.EntityOffset()));
}

const Entity *Chunk::Entities() const noexcept
{
	return reinterpret_cast<const Entity *>(DataAt(m_layout.EntityOffset()));
}

void *Chunk::ComponentData(std::size_t column) noexcept
{
	return DataAt(m_layout.Columns()[column].offset);
}

const void *Chunk::ComponentData(std::size_t column) const noexcept
{
	return DataAt(m_layout.Columns()[column].offset);
}

void Chunk::AddEntity(Entity entity)
{
	assert(!IsFull());
	const std::size_t row = m_size;
	std::size_t constructed = 0;
	try
	{
		for (const ComponentInfo *info : m_components)
		{
			const ChunkLayout::Column *column = m_layout.Find(info->id);
			info->constructDefault(DataAt(column->offset + row * column->size));
			++constructed;
		}
	}
	catch (...)
	{
		for (std::size_t i = 0; i < constructed; ++i)
		{
			const ComponentInfo *info = m_components[i];
			const ChunkLayout::Column *column = m_layout.Find(info->id);
			info->destroy(DataAt(column->offset + row * column->size));
		}
		throw;
	}

	Entities()[row] = entity;
	++m_size;
}

Entity Chunk::RemoveSwap(std::size_t row)
{
	assert(row < m_size);
	const std::size_t last = m_size - 1;
	if (row == last)
	{
		for (const ComponentInfo *info : m_components)
		{
			const ChunkLayout::Column *column = m_layout.Find(info->id);
			info->destroy(DataAt(column->offset + last * column->size));
		}
		--m_size;
		return Entity::Null();
	}

	const Entity moved = Entities()[last];
	Entities()[row] = moved;
	for (const ComponentInfo *info : m_components)
	{
		const ChunkLayout::Column *column = m_layout.Find(info->id);
		void *destination = DataAt(column->offset + row * column->size);
		void *source = DataAt(column->offset + last * column->size);
		info->destroy(destination);
		info->constructMove(destination, source);
		info->destroy(source);
	}
	--m_size;
	return moved;
}

} // namespace ecs
