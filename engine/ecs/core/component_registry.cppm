module;

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

export module engine.ecs.core.component_registry;

export namespace ecs
{

using ComponentId = std::uint32_t;
inline constexpr ComponentId InvalidComponentId = static_cast<ComponentId>(-1);

struct ComponentInfo
{
	ComponentId id{InvalidComponentId};
	std::size_t size{0};
	std::size_t alignment{1};

	void (*constructDefault)(void *destination) = nullptr;
	void (*constructMove)(void *destination, void *source) noexcept = nullptr;
	void (*destroy)(void *object) noexcept = nullptr;
};

class ComponentRegistry
{
public:
	template<typename T>
	ComponentId Register()
	{
		static_assert(std::is_object_v<T>, "ECS components must be object types");
		static_assert(!std::is_const_v<T> && !std::is_volatile_v<T>, "ECS component types cannot be cv-qualified");
		static_assert(std::is_default_constructible_v<T>, "ECS components must be default constructible");
		static_assert(std::is_move_constructible_v<T>, "ECS components must be move constructible");
		static_assert(std::is_nothrow_move_constructible_v<T>, "ECS components must be nothrow move constructible");
		static_assert(std::is_destructible_v<T>, "ECS components must be destructible");
		static_assert(std::is_nothrow_destructible_v<T>, "ECS components must be nothrow destructible");

		const std::type_index type = typeid(T);
		const auto found = m_typeToId.find(type);
		if (found != m_typeToId.end())
			return found->second;

		if (m_infos.size() >= static_cast<std::size_t>(InvalidComponentId))
			throw std::length_error("ECS component registry exhausted");

		const ComponentId id = static_cast<ComponentId>(m_infos.size());
		const auto inserted = m_typeToId.emplace(type, id);
		if (!inserted.second)
			return inserted.first->second;

		try
		{
			m_infos.push_back(ComponentInfo{
				id,
				sizeof(T),
				alignof(T),
				&ConstructDefault<T>,
				&ConstructMove<T>,
				&Destroy<T>});
		}
		catch (...)
		{
			m_typeToId.erase(inserted.first);
			throw;
		}

		return id;
	}

	template<typename T>
	ComponentId TryGet() const noexcept
	{
		const auto found = m_typeToId.find(std::type_index(typeid(T)));
		return found == m_typeToId.end() ? InvalidComponentId : found->second;
	}

	const ComponentInfo *TryGet(ComponentId id) const noexcept;
	const ComponentInfo &Get(ComponentId id) const;
	std::size_t Count() const noexcept { return m_infos.size(); }

private:
	template<typename T>
	static void ConstructDefault(void *destination)
	{
		std::construct_at(static_cast<T *>(destination));
	}

	template<typename T>
	static void ConstructMove(void *destination, void *source) noexcept
	{
		std::construct_at(static_cast<T *>(destination), std::move(*static_cast<T *>(source)));
	}

	template<typename T>
	static void Destroy(void *object) noexcept
	{
		std::destroy_at(static_cast<T *>(object));
	}

	// Archetypes retain pointers to these records; deque growth preserves their
	// addresses as additional component types are registered.
	std::deque<ComponentInfo> m_infos;
	std::unordered_map<std::type_index, ComponentId> m_typeToId;
};

} // namespace ecs

namespace ecs
{

const ComponentInfo *ComponentRegistry::TryGet(ComponentId id) const noexcept
{
	return id < m_infos.size() ? &m_infos[id] : nullptr;
}

const ComponentInfo &ComponentRegistry::Get(ComponentId id) const
{
	const ComponentInfo *info = TryGet(id);
	if (info == nullptr)
		throw std::out_of_range("ECS component id is not registered");
	return *info;
}

} // namespace ecs
