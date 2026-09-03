module;

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

export module Graphics.Scene.Beams;

export import Graphics.Resources.Bindless.BindlessResourceTable;
export import Graphics.Resources.Handles.ResourceHandle;
export import Graphics.Resources.Materials.Material;
export import Graphics.RenderGraph.Execution;
export import Graphics.Shaders.Library;

import Graphics.Memory.AlignedAllocator;

namespace Graphics
{

export inline constexpr std::uint32_t Invalid_Beam_Index = std::numeric_limits<std::uint32_t>::max();

export enum class BeamFlags : std::uint32_t
{
	None = 0,
	Enabled = 1u << 0
};

export constexpr BeamFlags operator|(BeamFlags left, BeamFlags right) noexcept
{
	return static_cast<BeamFlags>(static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

export constexpr BeamFlags operator&(BeamFlags left, BeamFlags right) noexcept
{
	return static_cast<BeamFlags>(static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
}

export constexpr bool Has_Beam_Flag(BeamFlags flags, BeamFlags flag) noexcept
{
	return (flags & flag) == flag;
}

export struct BeamDescription final
{
	std::array<float, 3> start{};
	std::array<float, 3> end{};
	float width = 0.5f;
	std::array<float, 4> color{1.0f, 1.0f, 1.0f, 1.0f};
	float opacity = 1.0f;
	MaterialHandle material{};
	BeamFlags flags = BeamFlags::Enabled;
};

static_assert(std::is_nothrow_move_constructible_v<BeamDescription>);
static_assert(std::is_nothrow_move_assignable_v<BeamDescription>);

export struct BeamData final
{
	std::span<const float> start_x{};
	std::span<const float> start_y{};
	std::span<const float> start_z{};
	std::span<const float> end_x{};
	std::span<const float> end_y{};
	std::span<const float> end_z{};
	std::span<const float> widths{};
	std::span<const float> color_r{};
	std::span<const float> color_g{};
	std::span<const float> color_b{};
	std::span<const float> color_a{};
	std::span<const float> opacities{};
	std::span<const MaterialHandle> materials{};
	std::span<const BeamFlags> flags{};
	std::span<const BeamHandle> handles{};

	std::size_t Size() const noexcept
	{
		return start_x.size();
	}
};

export struct BeamView final
{
	std::array<float, 16> view_projection{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	std::array<float, 3> camera_right{1.0f, 0.0f, 0.0f};
	std::array<float, 3> camera_up{0.0f, 1.0f, 0.0f};
	std::array<float, 3> camera_forward{0.0f, 0.0f, -1.0f};
};

export struct BeamVertex final
{
	float position[3]{};
	float color[4]{};
	float uv[2]{};
};

static_assert(sizeof(BeamVertex) == 36);

export class BeamSet final
{
public:
	void Reserve(std::size_t capacity)
	{
		m_start_x.reserve(capacity);
		m_start_y.reserve(capacity);
		m_start_z.reserve(capacity);
		m_end_x.reserve(capacity);
		m_end_y.reserve(capacity);
		m_end_z.reserve(capacity);
		m_widths.reserve(capacity);
		m_color_r.reserve(capacity);
		m_color_g.reserve(capacity);
		m_color_b.reserve(capacity);
		m_color_a.reserve(capacity);
		m_opacities.reserve(capacity);
		m_materials.reserve(capacity);
		m_flags.reserve(capacity);
		m_dense_handles.reserve(capacity);
		m_slots.reserve(capacity);
	}

	BeamHandle Create(const BeamDescription &description = {})
	{
		Ensure_Capacity();
		if (m_slots.size() >= std::numeric_limits<std::uint32_t>::max() && m_free_head == Invalid_Beam_Index)
			return {};

		const bool reuses_slot = m_free_head != Invalid_Beam_Index;
		const std::uint32_t slot_index = reuses_slot ? m_free_head : static_cast<std::uint32_t>(m_slots.size());
		const std::uint32_t dense_index = static_cast<std::uint32_t>(Size());

		Append(description);
		if (reuses_slot) {
			Slot &slot = m_slots[slot_index];
			m_free_head = slot.next_free;
			slot.next_free = Invalid_Beam_Index;
			slot.dense_index = dense_index;
		} else {
			m_slots.push_back({dense_index, Invalid_Beam_Index, 1});
		}

		m_dense_handles.emplace_back(slot_index, m_slots[slot_index].generation);
		return BeamHandle(slot_index, m_slots[slot_index].generation);
	}

	bool Destroy(BeamHandle handle) noexcept
	{
		if (!Is_Valid(handle))
			return false;

		const std::uint32_t slot_index = handle.Get_Index();
		const std::uint32_t dense_index = m_slots[slot_index].dense_index;
		const std::uint32_t last_dense_index = static_cast<std::uint32_t>(Size() - 1);
		if (dense_index != last_dense_index) {
			Move_Dense(dense_index, last_dense_index);
			const std::uint32_t moved_slot_index = m_dense_handles[last_dense_index].Get_Index();
			m_dense_handles[dense_index] = m_dense_handles[last_dense_index];
			m_slots[moved_slot_index].dense_index = dense_index;
		}

		Pop_Dense();
		Slot &slot = m_slots[slot_index];
		slot.dense_index = Invalid_Beam_Index;
		slot.generation = Next_Generation(slot.generation);
		slot.next_free = m_free_head;
		m_free_head = slot_index;
		return true;
	}

	bool Update(BeamHandle handle, const BeamDescription &description) noexcept
	{
		if (!Is_Valid(handle))
			return false;

		const std::uint32_t dense_index = m_slots[handle.Get_Index()].dense_index;
		m_start_x[dense_index] = description.start[0];
		m_start_y[dense_index] = description.start[1];
		m_start_z[dense_index] = description.start[2];
		m_end_x[dense_index] = description.end[0];
		m_end_y[dense_index] = description.end[1];
		m_end_z[dense_index] = description.end[2];
		m_widths[dense_index] = description.width;
		m_color_r[dense_index] = description.color[0];
		m_color_g[dense_index] = description.color[1];
		m_color_b[dense_index] = description.color[2];
		m_color_a[dense_index] = description.color[3];
		m_opacities[dense_index] = description.opacity;
		m_materials[dense_index] = description.material;
		m_flags[dense_index] = description.flags;
		return true;
	}

	bool Is_Valid(BeamHandle handle) const noexcept
	{
		if (!handle.Is_Valid() || handle.Get_Index() >= m_slots.size())
			return false;

		const Slot &slot = m_slots[handle.Get_Index()];
		return slot.dense_index != Invalid_Beam_Index && slot.generation == handle.Get_Generation();
	}

	std::uint32_t Dense_Index(BeamHandle handle) const noexcept
	{
		return Is_Valid(handle) ? m_slots[handle.Get_Index()].dense_index : Invalid_Beam_Index;
	}

	std::size_t Size() const noexcept
	{
		return m_start_x.size();
	}

	BeamData Data() const noexcept
	{
		return {
			m_start_x,
			m_start_y,
			m_start_z,
			m_end_x,
			m_end_y,
			m_end_z,
			m_widths,
			m_color_r,
			m_color_g,
			m_color_b,
			m_color_a,
			m_opacities,
			m_materials,
			m_flags,
			m_dense_handles
		};
	}

	void Clear() noexcept
	{
		m_start_x.clear();
		m_start_y.clear();
		m_start_z.clear();
		m_end_x.clear();
		m_end_y.clear();
		m_end_z.clear();
		m_widths.clear();
		m_color_r.clear();
		m_color_g.clear();
		m_color_b.clear();
		m_color_a.clear();
		m_opacities.clear();
		m_materials.clear();
		m_flags.clear();
		m_dense_handles.clear();
		m_slots.clear();
		m_free_head = Invalid_Beam_Index;
	}

private:
	struct Slot final
	{
		std::uint32_t dense_index = Invalid_Beam_Index;
		std::uint32_t next_free = Invalid_Beam_Index;
		std::uint32_t generation = 0;
	};

	static std::size_t Next_Capacity(std::size_t current) noexcept
	{
		return current == 0
			? 1
			: current > std::numeric_limits<std::size_t>::max() / 2
				? std::numeric_limits<std::size_t>::max()
				: current * 2;
	}

	static std::uint32_t Next_Generation(std::uint32_t generation) noexcept
	{
		const std::uint32_t next = generation + 1;
		return next == 0 ? 1 : next;
	}

	void Ensure_Capacity()
	{
		if (m_start_x.size() < m_start_x.capacity())
			return;

		Reserve(Next_Capacity(m_start_x.size()));
	}

	void Append(const BeamDescription &description)
	{
		m_start_x.push_back(description.start[0]);
		m_start_y.push_back(description.start[1]);
		m_start_z.push_back(description.start[2]);
		m_end_x.push_back(description.end[0]);
		m_end_y.push_back(description.end[1]);
		m_end_z.push_back(description.end[2]);
		m_widths.push_back(description.width);
		m_color_r.push_back(description.color[0]);
		m_color_g.push_back(description.color[1]);
		m_color_b.push_back(description.color[2]);
		m_color_a.push_back(description.color[3]);
		m_opacities.push_back(description.opacity);
		m_materials.push_back(description.material);
		m_flags.push_back(description.flags);
	}

	void Move_Dense(std::uint32_t destination, std::uint32_t source) noexcept
	{
		m_start_x[destination] = m_start_x[source];
		m_start_y[destination] = m_start_y[source];
		m_start_z[destination] = m_start_z[source];
		m_end_x[destination] = m_end_x[source];
		m_end_y[destination] = m_end_y[source];
		m_end_z[destination] = m_end_z[source];
		m_widths[destination] = m_widths[source];
		m_color_r[destination] = m_color_r[source];
		m_color_g[destination] = m_color_g[source];
		m_color_b[destination] = m_color_b[source];
		m_color_a[destination] = m_color_a[source];
		m_opacities[destination] = m_opacities[source];
		m_materials[destination] = m_materials[source];
		m_flags[destination] = m_flags[source];
	}

	void Pop_Dense() noexcept
	{
		m_start_x.pop_back();
		m_start_y.pop_back();
		m_start_z.pop_back();
		m_end_x.pop_back();
		m_end_y.pop_back();
		m_end_z.pop_back();
		m_widths.pop_back();
		m_color_r.pop_back();
		m_color_g.pop_back();
		m_color_b.pop_back();
		m_color_a.pop_back();
		m_opacities.pop_back();
		m_materials.pop_back();
		m_flags.pop_back();
		m_dense_handles.pop_back();
	}

	AlignedVector<float> m_start_x;
	AlignedVector<float> m_start_y;
	AlignedVector<float> m_start_z;
	AlignedVector<float> m_end_x;
	AlignedVector<float> m_end_y;
	AlignedVector<float> m_end_z;
	AlignedVector<float> m_widths;
	AlignedVector<float> m_color_r;
	AlignedVector<float> m_color_g;
	AlignedVector<float> m_color_b;
	AlignedVector<float> m_color_a;
	AlignedVector<float> m_opacities;
	AlignedVector<MaterialHandle> m_materials;
	AlignedVector<BeamFlags> m_flags;
	std::vector<BeamHandle> m_dense_handles;
	std::vector<Slot> m_slots;
	std::uint32_t m_free_head = Invalid_Beam_Index;
};

export std::size_t Build_Beam_Vertices(const BeamData &beams, const BeamView &view, std::span<BeamVertex> output) noexcept;

export struct BeamPassInput final
{
	RHITextureHandle color_target{};
	RHITextureHandle depth_target{};
	RHIBufferHandle vertex_buffer{};
	PipelineHandle pipeline{};
	std::span<const RHIBindlessResource> bindless_resources{};
	GraphResourceHandle color_resource{};
	GraphResourceHandle depth_resource{};
	RHIViewport viewport{};
	std::uint32_t vertex_count = 0;
};

export class BeamPass final
{
public:
	static GraphPassHandle Add_To_Graph(RenderGraph &graph, GraphResourceHandle color_resource, GraphResourceHandle depth_resource, std::uint32_t pass_key = 0)
	{
		if (!graph.Is_Resource_Valid(color_resource) || !graph.Is_Resource_Valid(depth_resource) || color_resource == depth_resource)
			return {};
		if (graph.Resource_Kind(color_resource) != GraphResourceKind::Texture || graph.Resource_Kind(depth_resource) != GraphResourceKind::Texture)
			return {};

		const std::array<GraphResourceUse, 2> uses = {
			GraphResourceUse::Write(color_resource),
			GraphResourceUse::Write(depth_resource)
		};
		return graph.Add_Pass({pass_key}, uses);
	}

	static bool Execute(CommandList &commands, const PassResources &resources, const BeamPassInput &input) noexcept
	{
		const RHITextureHandle color_target = resources.Texture(input.color_resource);
		const RHITextureHandle depth_target = resources.Texture(input.depth_resource);
		if (!color_target.Is_Valid() || !depth_target.Is_Valid() || !input.vertex_buffer.Is_Valid() || !input.pipeline.Is_Valid() || input.vertex_count == 0 || input.viewport.width == 0 || input.viewport.height == 0)
			return false;

		return commands.Set_Render_Targets(color_target, depth_target)
			&& commands.Set_Viewport(input.viewport)
			&& commands.Set_Bindless_Resources(input.bindless_resources)
			&& commands.Bind_Pipeline(input.pipeline)
			&& commands.Set_Vertex_Buffer(0, input.vertex_buffer, sizeof(BeamVertex), 0)
			&& commands.Draw(input.vertex_count);
	}
};

export class BeamRenderer final
{
public:
	bool Initialize(Device &device, const std::filesystem::path &shader_directory, std::size_t max_beams = 4096)
	{
		if (m_device != nullptr || max_beams == 0 || max_beams > std::numeric_limits<std::uint32_t>::max() / 6u)
			return false;

		const std::size_t vertex_count = max_beams * 6;
		if (vertex_count > std::numeric_limits<std::size_t>::max() / sizeof(BeamVertex) || vertex_count * sizeof(BeamVertex) > std::numeric_limits<std::uint32_t>::max())
			return false;

		m_device = &device;
		m_beams.Reserve(max_beams);
		m_vertices.resize(vertex_count);
		m_graph = std::make_unique<RenderGraph>();
		m_graph->Reserve(2, 1, 2);
		m_color_resource = m_graph->Create_Resource({GraphResourceKind::Texture});
		m_depth_resource = m_graph->Create_Resource({GraphResourceKind::Texture});
		m_pass = BeamPass::Add_To_Graph(*m_graph, m_color_resource, m_depth_resource, 30);
		if (!m_color_resource.Is_Valid() || !m_depth_resource.Is_Valid() || !m_pass.Is_Valid()) {
			Shutdown();
			return false;
		}

		m_shader = m_shaders.Load_Basic_Opaque(shader_directory);
		if (!m_shader.Is_Valid()) {
			Shutdown();
			return false;
		}

		Material material;
		material.shader = m_shader;
		material.parameters.values[0] = 1.0f;
		material.parameters.values[1] = 1.0f;
		material.parameters.values[2] = 1.0f;
		material.parameters.values[3] = 1.0f;
		m_material = m_materials.Create(material);
		if (!m_material.Is_Valid()) {
			Shutdown();
			return false;
		}

		PipelineDesc pipeline_description = m_shaders.Make_Pipeline_Description(m_shader, Make_Basic_Opaque_Pipeline());
		pipeline_description.depth_test = true;
		pipeline_description.depth_write = false;
		pipeline_description.blend_mode = RHIBlendMode::Alpha;
		m_pipeline = m_shaders.Create_Pipeline(device, material, m_shader, pipeline_description);
		if (!m_pipeline.Is_Valid()) {
			Shutdown();
			return false;
		}

		BeamInstanceData instance_data;
		instance_data.transform[0] = 1.0f;
		instance_data.transform[5] = 1.0f;
		instance_data.transform[10] = 1.0f;
		instance_data.transform[15] = 1.0f;
		m_instance_buffer = device.Create_Buffer_Initialized(
			{static_cast<std::uint32_t>(sizeof(instance_data)), RHIBufferUsage::Storage, static_cast<std::uint32_t>(sizeof(instance_data))},
			std::as_bytes(std::span<const BeamInstanceData>(&instance_data, 1)));
		if (!m_instance_buffer.Is_Valid()) {
			Shutdown();
			return false;
		}

		m_material_constants = device.Create_Buffer_Initialized(
			{static_cast<std::uint32_t>(material.parameters.Bytes().size()), RHIBufferUsage::Constant, 16},
			material.parameters.Bytes());
		if (!m_material_constants.Is_Valid()) {
			Shutdown();
			return false;
		}

		m_vertex_buffer = device.Create_Buffer({static_cast<std::uint32_t>(vertex_count * sizeof(BeamVertex)), RHIBufferUsage::Vertex, sizeof(BeamVertex)});
		if (!m_vertex_buffer.Is_Valid()) {
			Shutdown();
			return false;
		}

		m_bindless.Reserve(2, 1, 0, 0, 1);
		if (!m_bindless.Register_Buffer(m_instance_buffer).Is_Valid() || !m_bindless.Register_Material(m_material, m_material_constants).Is_Valid()) {
			Shutdown();
			return false;
		}

		return true;
	}

	void Shutdown() noexcept
	{
		if (m_device != nullptr) {
			if (m_vertex_buffer.Is_Valid())
				m_device->Destroy_Buffer(m_vertex_buffer);
			if (m_material_constants.Is_Valid())
				m_device->Destroy_Buffer(m_material_constants);
			if (m_instance_buffer.Is_Valid())
				m_device->Destroy_Buffer(m_instance_buffer);
			if (m_pipeline.Is_Valid())
				m_device->Destroy_Pipeline(m_pipeline);
		}

		m_vertex_buffer = {};
		m_material_constants = {};
		m_instance_buffer = {};
		m_pipeline = {};
		m_bindless.Clear();
		if (m_material.Is_Valid())
			m_materials.Destroy(m_material);
		m_material = {};
		if (m_shader.Is_Valid())
			m_shaders.Destroy(m_shader);
		m_shader = {};
		m_plan = {};
		m_graph.reset();
		m_pass = {};
		m_color_resource = {};
		m_depth_resource = {};
		m_beams.Clear();
		m_vertex_count = 0;
		m_device = nullptr;
	}

	bool Is_Initialized() const noexcept
	{
		return m_device != nullptr && m_pipeline.Is_Valid() && m_vertex_buffer.Is_Valid();
	}

	BeamHandle Create(const BeamDescription &description = {})
	{
		return Is_Initialized() ? m_beams.Create(description) : BeamHandle{};
	}

	bool Update(BeamHandle handle, const BeamDescription &description) noexcept
	{
		return Is_Initialized() && m_beams.Update(handle, description);
	}

	bool Destroy(BeamHandle handle) noexcept
	{
		return Is_Initialized() && m_beams.Destroy(handle);
	}

	BeamData Data() const noexcept
	{
		return m_beams.Data();
	}

	std::size_t Size() const noexcept
	{
		return m_beams.Size();
	}

	bool Set_View(const BeamView &view) noexcept
	{
		if (!Is_Initialized())
			return false;
		m_view = view;
		return true;
	}

	const BeamView &View() const noexcept
	{
		return m_view;
	}

	std::span<const BeamVertex> Vertices() const noexcept
	{
		return {m_vertices.data(), m_vertex_count};
	}

	bool Render(CommandList &commands, RHITextureHandle color_target, RHITextureHandle depth_target, RHIViewport viewport) noexcept
	{
		if (!Is_Initialized() || !color_target.Is_Valid() || !depth_target.Is_Valid() || viewport.width == 0 || viewport.height == 0)
			return false;

		m_vertex_count = static_cast<std::uint32_t>(Build_Beam_Vertices(m_beams.Data(), m_view, std::span<BeamVertex>(m_vertices)));
		if (m_vertex_count == 0)
			return true;
		if (!m_device->Update_Buffer(m_vertex_buffer, 0, std::as_bytes(std::span<const BeamVertex>(m_vertices.data(), m_vertex_count))))
			return false;

		m_bindings[0] = GraphResourceBinding::Texture(m_color_resource, color_target);
		m_bindings[1] = GraphResourceBinding::Texture(m_depth_resource, depth_target);
		if (!m_plan.Is_Valid() && !m_plan.Compile(*m_graph, m_bindings))
			return false;

		const BeamPassInput input{
			color_target,
			depth_target,
			m_vertex_buffer,
			m_pipeline,
			m_bindless.Resources(),
			m_color_resource,
			m_depth_resource,
			viewport,
			m_vertex_count
		};
		return m_plan.Execute(*m_graph, commands, [&](GraphPassHandle pass, CommandList &command_list, const PassResources &resources) noexcept {
			return pass == m_pass && BeamPass::Execute(command_list, resources, input);
		});
	}

	bool Render(RHITextureHandle color_target, RHITextureHandle depth_target, RHIViewport viewport) noexcept
	{
		return m_device != nullptr && Render(m_device->Immediate_Command_List(), color_target, depth_target, viewport);
	}

private:
	struct alignas(16) BeamInstanceData final
	{
		std::array<float, 16> transform{};
		std::array<float, 4> bounds{};
		std::uint32_t mesh_index = 0;
		std::uint32_t material_index = 0;
		std::uint32_t flags = 0;
		std::uint32_t reserved = 0;
	};

	static_assert(sizeof(BeamInstanceData) == 96);

	Device *m_device = nullptr;
	BeamSet m_beams;
	BeamView m_view{};
	AlignedVector<BeamVertex> m_vertices;
	std::uint32_t m_vertex_count = 0;
	ShaderLibrary m_shaders;
	ShaderHandle m_shader{};
	MaterialPool m_materials;
	MaterialHandle m_material{};
	PipelineHandle m_pipeline{};
	RHIBufferHandle m_instance_buffer{};
	RHIBufferHandle m_material_constants{};
	RHIBufferHandle m_vertex_buffer{};
	BindlessResourceTable m_bindless;
	std::unique_ptr<RenderGraph> m_graph;
	ExecutionPlan m_plan;
	std::array<GraphResourceBinding, 2> m_bindings{};
	GraphResourceHandle m_color_resource{};
	GraphResourceHandle m_depth_resource{};
	GraphPassHandle m_pass{};
};

namespace
{
struct Vec3 final
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

Vec3 operator-(Vec3 left, Vec3 right) noexcept
{
	return {left.x - right.x, left.y - right.y, left.z - right.z};
}

Vec3 operator+(Vec3 left, Vec3 right) noexcept
{
	return {left.x + right.x, left.y + right.y, left.z + right.z};
}

Vec3 operator*(Vec3 value, float scalar) noexcept
{
	return {value.x * scalar, value.y * scalar, value.z * scalar};
}

float Dot(Vec3 left, Vec3 right) noexcept
{
	return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vec3 Cross(Vec3 left, Vec3 right) noexcept
{
	return {
		left.y * right.z - left.z * right.y,
		left.z * right.x - left.x * right.z,
		left.x * right.y - left.y * right.x
	};
}

Vec3 Normalize(Vec3 value) noexcept
{
	const float length_squared = Dot(value, value);
	if (!(length_squared > 1.0e-12f) || !std::isfinite(length_squared))
		return {};

	const float inverse_length = 1.0f / std::sqrt(length_squared);
	return value * inverse_length;
}

std::array<float, 4> Transform(const std::array<float, 16> &matrix, Vec3 point) noexcept
{
	return {
		matrix[0] * point.x + matrix[1] * point.y + matrix[2] * point.z + matrix[3],
		matrix[4] * point.x + matrix[5] * point.y + matrix[6] * point.z + matrix[7],
		matrix[8] * point.x + matrix[9] * point.y + matrix[10] * point.z + matrix[11],
		matrix[12] * point.x + matrix[13] * point.y + matrix[14] * point.z + matrix[15]
	};
}

void Write_Vertex(BeamVertex &vertex, const std::array<float, 4> &position, const std::array<float, 4> &color, float u, float v) noexcept
{
	vertex.position[0] = position[0];
	vertex.position[1] = position[1];
	vertex.position[2] = position[2];
	vertex.color[0] = color[0];
	vertex.color[1] = color[1];
	vertex.color[2] = color[2];
	vertex.color[3] = color[3];
	vertex.uv[0] = u;
	vertex.uv[1] = v;
}
}

export std::size_t Build_Beam_Vertices(const BeamData &beams, const BeamView &view, std::span<BeamVertex> output) noexcept
{
	if (beams.Size() > output.size() / 6)
		return 0;

	std::size_t output_count = 0;
	for (std::size_t index = 0; index < beams.Size(); ++index) {
		if (!Has_Beam_Flag(beams.flags[index], BeamFlags::Enabled) || !(beams.widths[index] > 0.0f))
			continue;

		const Vec3 start{beams.start_x[index], beams.start_y[index], beams.start_z[index]};
		const Vec3 end{beams.end_x[index], beams.end_y[index], beams.end_z[index]};
		const Vec3 direction = Normalize(end - start);
		if (Dot(direction, direction) == 0.0f)
			continue;

		Vec3 side = Normalize(Cross(direction, {view.camera_forward[0], view.camera_forward[1], view.camera_forward[2]}));
		if (Dot(side, side) == 0.0f)
			side = Normalize({view.camera_right[0], view.camera_right[1], view.camera_right[2]});
		if (Dot(side, side) == 0.0f)
			continue;

		const float half_width = beams.widths[index] * 0.5f;
		const Vec3 offset = side * half_width;
		const Vec3 start_left = start - offset;
		const Vec3 start_right = start + offset;
		const Vec3 end_left = end - offset;
		const Vec3 end_right = end + offset;
		const std::array<float, 4> start_clip = Transform(view.view_projection, start_left);
		const std::array<float, 4> start_right_clip = Transform(view.view_projection, start_right);
		const std::array<float, 4> end_clip = Transform(view.view_projection, end_left);
		const std::array<float, 4> end_right_clip = Transform(view.view_projection, end_right);
		if (start_clip[3] == 0.0f || start_right_clip[3] == 0.0f || end_clip[3] == 0.0f || end_right_clip[3] == 0.0f)
			continue;

		const std::array<float, 4> color = {
			beams.color_r[index],
			beams.color_g[index],
			beams.color_b[index],
			beams.color_a[index] * beams.opacities[index]
		};
		Write_Vertex(output[output_count++], start_clip, color, 0.0f, 0.0f);
		Write_Vertex(output[output_count++], start_right_clip, color, 1.0f, 0.0f);
		Write_Vertex(output[output_count++], end_clip, color, 0.0f, 1.0f);
		Write_Vertex(output[output_count++], end_clip, color, 0.0f, 1.0f);
		Write_Vertex(output[output_count++], start_right_clip, color, 1.0f, 0.0f);
		Write_Vertex(output[output_count++], end_right_clip, color, 1.0f, 1.0f);
	}

	return output_count;
}

namespace
{
BeamRenderer g_beam_renderer;

std::uint64_t Encode(BeamHandle handle) noexcept
{
	if (!handle.Is_Valid())
		return 0;

	return (static_cast<std::uint64_t>(handle.Get_Generation()) << 32) | handle.Get_Index();
}

BeamHandle Decode(std::uint64_t handle) noexcept
{
	return handle == 0 ? BeamHandle{} : BeamHandle(static_cast<std::uint32_t>(handle), static_cast<std::uint32_t>(handle >> 32));
}

BeamDescription Make_Description(
	float start_x,
	float start_y,
	float start_z,
	float end_x,
	float end_y,
	float end_z,
	float width,
	float red,
	float green,
	float blue,
	float opacity,
	std::uint32_t flags) noexcept
{
	BeamDescription description;
	description.start = {start_x, start_y, start_z};
	description.end = {end_x, end_y, end_z};
	description.width = width;
	description.color = {red, green, blue, 1.0f};
	description.opacity = opacity;
	description.flags = static_cast<BeamFlags>(flags);
	return description;
}
}

export extern "C" bool Graphics_Beam_Initialize(Device *device, const char *shader_directory, std::uint32_t max_beams)
{
	return device != nullptr && g_beam_renderer.Initialize(*device, std::filesystem::path(shader_directory != nullptr ? shader_directory : ""), max_beams);
}

export extern "C" void Graphics_Beam_Shutdown() noexcept
{
	g_beam_renderer.Shutdown();
}

export extern "C" bool Graphics_Beam_Set_View(
	const float *view_projection,
	const float *camera_right,
	const float *camera_up,
	const float *camera_forward) noexcept
{
	if (!g_beam_renderer.Is_Initialized() || view_projection == nullptr || camera_right == nullptr || camera_up == nullptr || camera_forward == nullptr)
		return false;

	BeamView view = g_beam_renderer.View();
	for (std::size_t index = 0; index < view.view_projection.size(); ++index)
		view.view_projection[index] = view_projection[index];
	for (std::size_t index = 0; index < view.camera_right.size(); ++index) {
		view.camera_right[index] = camera_right[index];
		view.camera_up[index] = camera_up[index];
		view.camera_forward[index] = camera_forward[index];
	}
	return g_beam_renderer.Set_View(view);
}

export extern "C" std::uint64_t Graphics_Beam_Create(
	float start_x,
	float start_y,
	float start_z,
	float end_x,
	float end_y,
	float end_z,
	float width,
	float red,
	float green,
	float blue,
	float opacity,
	std::uint32_t flags)
{
	return Encode(g_beam_renderer.Create(Make_Description(start_x, start_y, start_z, end_x, end_y, end_z, width, red, green, blue, opacity, flags)));
}

export extern "C" bool Graphics_Beam_Update(
	std::uint64_t handle,
	float start_x,
	float start_y,
	float start_z,
	float end_x,
	float end_y,
	float end_z,
	float width,
	float red,
	float green,
	float blue,
	float opacity,
	std::uint32_t flags) noexcept
{
	return g_beam_renderer.Update(Decode(handle), Make_Description(start_x, start_y, start_z, end_x, end_y, end_z, width, red, green, blue, opacity, flags));
}

export extern "C" bool Graphics_Beam_Destroy(std::uint64_t handle) noexcept
{
	return g_beam_renderer.Destroy(Decode(handle));
}

export extern "C" bool Graphics_Beam_Render(RHITextureHandle color_target, RHITextureHandle depth_target, std::uint32_t width, std::uint32_t height) noexcept
{
	if (!g_beam_renderer.Is_Initialized() || width == 0 || height == 0)
		return false;

	return g_beam_renderer.Render(color_target, depth_target, {0, 0, width, height, 0.0f, 1.0f});
}

}
