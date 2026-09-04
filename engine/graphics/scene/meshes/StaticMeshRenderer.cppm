module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <vector>

export module Graphics.Scene.StaticMeshes;

export import Graphics.Passes.Opaque;
export import Graphics.Resources.Bindless.BindlessResourceTable;
export import Graphics.Resources.Materials.Material;
export import Graphics.Resources.Residency.GPUResourceResidency;
export import Graphics.Scene.DrawGeneration;
export import Graphics.Scene.GPUScene;
export import Graphics.Scene.Models.ModelVisibility;
export import Graphics.Scene.Views.View;
export import Graphics.Scene.Visibility;
export import Graphics.Shaders.Library;
export import Graphics.RHI.Frame;

namespace Graphics
{

export struct StaticMeshVertex final
{
	float position[3]{};
	float color[4]{1.0f, 1.0f, 1.0f, 1.0f};
	float uv[2]{};
};

static_assert(sizeof(StaticMeshVertex) == 36);

export struct StaticMeshSource final
{
	std::uint32_t vertex_count = 0;
	std::uint32_t index_count = 0;
	std::uint32_t vertex_stride = sizeof(StaticMeshVertex);
	MeshIndexFormat index_format = MeshIndexFormat::None;
	std::span<const std::byte> vertex_data{};
	std::span<const std::byte> index_data{};
	std::array<float, 3> bounds_center{};
	float bounds_radius = 0.0f;
	std::span<const MeshPart> parts{};
};

export bool Validate_Static_Mesh_Source(const StaticMeshSource &source) noexcept
{
	if (source.vertex_count == 0 || source.index_count == 0 || source.vertex_stride != sizeof(StaticMeshVertex) || source.index_format == MeshIndexFormat::None)
		return false;

	const std::size_t index_stride = source.index_format == MeshIndexFormat::UInt16 ? sizeof(std::uint16_t) : sizeof(std::uint32_t);
	if (source.vertex_count > std::numeric_limits<std::size_t>::max() / source.vertex_stride
		|| source.index_count > std::numeric_limits<std::size_t>::max() / index_stride)
		return false;

	return source.vertex_data.size() == static_cast<std::size_t>(source.vertex_count) * source.vertex_stride
		&& source.index_data.size() == static_cast<std::size_t>(source.index_count) * index_stride
		&& source.vertex_data.size() <= std::numeric_limits<std::uint32_t>::max()
		&& source.index_data.size() <= std::numeric_limits<std::uint32_t>::max()
		&& source.bounds_radius >= 0.0f
		&& source.parts.size() <= Max_Model_Part_Count;
}

export struct alignas(16) GPUViewData final
{
	std::array<float, 16> view_projection{};
};

static_assert(sizeof(GPUViewData) == 64);

export class StaticMeshRenderer final
{
public:
	bool Initialize(Device &device, const std::filesystem::path &shader_directory, std::size_t max_meshes = 4096, std::size_t max_instances = 16384)
	{
		if (m_device != nullptr || !device.Is_Valid() || max_meshes == 0 || max_instances == 0 || max_instances > std::numeric_limits<std::uint32_t>::max() / sizeof(GPUInstanceData))
			return false;

		m_device = &device;
		m_mesh_sources.reserve(max_meshes);
		m_scene.Reserve(max_instances);
		m_meshes.Reserve(max_meshes);
		m_materials.Reserve(1);
		m_gpu_scene.Reserve(max_instances, max_meshes, 1);
		m_visible_storage.resize(max_instances);
		m_lod_storage.resize(max_instances);
		m_draw_storage.resize(max_instances);
		m_visible = std::make_unique<VisibleSet>(m_visible_storage);
		m_lod = std::make_unique<LODSet>(m_lod_storage);
		m_draws = std::make_unique<DrawSet>(m_draw_storage);
		m_mesh_bindings.resize(max_meshes);
		m_mesh_part_bindings.reserve(max_meshes * Max_Model_Part_Count);
		m_dirty_instances.reserve(max_instances);
		m_instance_capacity = max_instances;

		m_residency = std::make_unique<GPUResourceResidency>(device);
		m_shader = m_shaders.Load_Basic_Opaque(shader_directory);
		if (!m_shader.Is_Valid()) {
			Shutdown();
			return false;
		}

		const PipelineDesc pipeline_description = m_shaders.Make_Pipeline_Description(m_shader, Make_Basic_Opaque_Pipeline());
		m_pipeline = m_shaders.Create_Pipeline(device, m_shader, pipeline_description);
		if (!m_pipeline.Is_Valid()) {
			Shutdown();
			return false;
		}

		m_instance_buffer = device.Create_Buffer({
			static_cast<std::uint32_t>(max_instances * sizeof(GPUInstanceData)),
			RHIBufferUsage::Storage,
			static_cast<std::uint32_t>(sizeof(GPUInstanceData))
		});
		if (!m_instance_buffer.Is_Valid()) {
			Shutdown();
			return false;
		}

		m_bindless.Reserve(4, 3, 0, 0, 1);
		if (!m_bindless.Register_Buffer(m_instance_buffer).Is_Valid()) {
			Shutdown();
			return false;
		}

		const GPULightData empty_light{};
		m_light_buffer = device.Create_Buffer_Initialized(
			{static_cast<std::uint32_t>(sizeof(GPULightData)), RHIBufferUsage::Storage, static_cast<std::uint32_t>(sizeof(GPULightData))},
			std::as_bytes(std::span<const GPULightData>(&empty_light, 1)));
		if (!m_light_buffer.Is_Valid() || !m_bindless.Register_Buffer(m_light_buffer).Is_Valid()) {
			Shutdown();
			return false;
		}

		m_view_buffer = device.Create_Buffer_Initialized(
			{static_cast<std::uint32_t>(sizeof(GPUViewData)), RHIBufferUsage::Storage, static_cast<std::uint32_t>(sizeof(GPUViewData))},
			std::as_bytes(std::span<const GPUViewData>(&m_gpu_view, 1)));
		if (!m_view_buffer.Is_Valid() || !m_bindless.Register_Buffer(m_view_buffer).Is_Valid()) {
			Shutdown();
			return false;
		}

		Material material;
		material.shader = m_shader;
		material.parameters.values[0] = 1.0f;
		material.parameters.values[1] = 0.0f;
		material.parameters.values[2] = 1.0f;
		material.parameters.values[3] = 1.0f;
		m_default_material = m_materials.Create(material);
		if (!m_default_material.Is_Valid() || !m_residency->Upload_Material(m_default_material, material)) {
			Shutdown();
			return false;
		}
		const GPUResidentMaterial resident_material = m_residency->Material_Info(m_default_material);
		if (!resident_material.constants.Is_Valid() || !m_bindless.Register_Material(m_default_material, resident_material.constants).Is_Valid()) {
			Shutdown();
			return false;
		}

		m_graph.Reserve(2, 1, 2);
		m_color_resource = m_graph.Create_Resource({GraphResourceKind::Texture});
		m_depth_resource = m_graph.Create_Resource({GraphResourceKind::Texture});
		m_opaque_pass = OpaquePass::Add_To_Graph(m_graph, m_color_resource, m_depth_resource, 10);
		if (!m_color_resource.Is_Valid() || !m_depth_resource.Is_Valid() || !m_opaque_pass.Is_Valid()) {
			Shutdown();
			return false;
		}

		m_view = View(Matrix4x4::Identity(), Matrix4x4::Identity(), {}, {});
		Set_View(m_view);
		m_scene_dirty = true;
		return true;
	}

	void Shutdown() noexcept
	{
		if (m_device != nullptr) {
			for (const std::unique_ptr<MeshStorage> &storage : m_mesh_sources) {
				if (storage != nullptr && storage->handle.Is_Valid())
					Destroy_Mesh(storage->handle);
			}
			if (m_default_material.Is_Valid()) {
				m_bindless.Destroy_Material(m_default_material);
				m_residency->Destroy_Material(m_default_material);
				m_materials.Destroy(m_default_material);
			}
			if (m_instance_buffer.Is_Valid())
				m_device->Destroy_Buffer(m_instance_buffer);
			if (m_light_buffer.Is_Valid())
				m_device->Destroy_Buffer(m_light_buffer);
			if (m_view_buffer.Is_Valid())
				m_device->Destroy_Buffer(m_view_buffer);
			if (m_pipeline.Is_Valid())
				m_device->Destroy_Pipeline(m_pipeline);
		}

		m_bindless.Clear();
		m_residency.reset();
		m_shaders.Destroy(m_shader);
		m_shader = {};
		m_pipeline = {};
		m_default_material = {};
		m_instance_buffer = {};
		m_light_buffer = {};
		m_view_buffer = {};
		m_mesh_sources.clear();
		m_visible_storage.clear();
		m_lod_storage.clear();
		m_draw_storage.clear();
		m_mesh_bindings.clear();
		m_mesh_part_bindings.clear();
		m_dirty_instances.clear();
		m_graph = {};
		m_plan = {};
		m_device = nullptr;
		m_instance_capacity = 0;
		m_graph_compiled = false;
		m_scene_dirty = true;
		m_view_dirty = false;
	}

	bool Is_Initialized() const noexcept
	{
		return m_device != nullptr;
	}

	MeshHandle Create_Mesh(const StaticMeshSource &source)
	{
		if (!Is_Initialized() || !Validate_Static_Mesh_Source(source))
			return {};

		auto storage = std::make_unique<MeshStorage>();
		storage->vertex_data.assign(source.vertex_data.begin(), source.vertex_data.end());
		storage->index_data.assign(source.index_data.begin(), source.index_data.end());
		storage->parts.assign(source.parts.begin(), source.parts.end());
		if (storage->parts.empty())
			storage->parts.push_back({0, source.index_count, 0});
		for (const MeshPart &part : storage->parts) {
			if (part.index_count == 0 || static_cast<std::uint64_t>(part.first_index) + part.index_count > source.index_count)
				return {};
		}
		Mesh resource;
		resource.vertex_count = source.vertex_count;
		resource.index_count = source.index_count;
		resource.vertex_stride = source.vertex_stride;
		resource.index_format = source.index_format;
		resource.vertex_data = std::span<const std::byte>(storage->vertex_data);
		resource.index_data = std::span<const std::byte>(storage->index_data);
		resource.parts = std::span<const MeshPart>(storage->parts);
		const MeshHandle handle = m_meshes.Create(std::move(resource));
		if (!handle.Is_Valid())
			return {};

		if (handle.Get_Index() >= m_mesh_sources.size())
			m_mesh_sources.resize(static_cast<std::size_t>(handle.Get_Index()) + 1);
		storage->handle = handle;
		m_mesh_sources[handle.Get_Index()] = std::move(storage);
		if (!m_residency->Upload_Mesh(handle, m_meshes)) {
			m_mesh_sources[handle.Get_Index()] = {};
			m_meshes.Destroy(handle);
			return {};
		}

		m_scene_dirty = true;
		return handle;
	}

	bool Destroy_Mesh(MeshHandle handle) noexcept
	{
		if (!handle.Is_Valid() || m_device == nullptr || m_meshes.Resolve(handle) == nullptr)
			return false;

		m_residency->Destroy_Mesh(handle);
		if (!m_meshes.Destroy(handle))
			return false;
		if (handle.Get_Index() < m_mesh_sources.size())
			m_mesh_sources[handle.Get_Index()] = {};
		m_scene_dirty = true;
		return true;
	}

	MaterialHandle Default_Material() const noexcept
	{
		return m_default_material;
	}

	InstanceHandle Create_Instance(const RenderInstance &instance)
	{
		if (!Is_Initialized() || !instance.mesh.Is_Valid() || m_meshes.Resolve(instance.mesh) == nullptr || instance.material != m_default_material)
			return {};

		const InstanceHandle handle = m_scene.Create(instance);
		m_scene_dirty = true;
		return handle;
	}

	bool Update_Instance(InstanceHandle handle, const RenderInstance &instance) noexcept
	{
		if (!Is_Initialized() || instance.material != m_default_material || !m_meshes.Resolve(instance.mesh) || !m_scene.Update(handle, instance))
			return false;

		if (!m_scene_dirty && m_dirty_instances.size() < m_dirty_instances.capacity())
			m_dirty_instances.push_back(handle);
		return true;
	}

	bool Update_Instance_Visibility(InstanceHandle handle, SubmeshVisibilityMask visibility_mask) noexcept
	{
		if (!Is_Initialized() || !m_scene.Update_Visibility(handle, visibility_mask))
			return false;

		if (!m_scene_dirty && m_dirty_instances.size() < m_dirty_instances.capacity())
			m_dirty_instances.push_back(handle);
		return true;
	}

	std::size_t Mesh_Part_Count(MeshHandle handle) const noexcept
	{
		const Mesh *mesh = m_meshes.Resolve(handle);
		return mesh == nullptr ? 0 : mesh->parts.size();
	}

	bool Destroy_Instance(InstanceHandle handle) noexcept
	{
		if (!m_scene.Destroy(handle))
			return false;

		m_scene_dirty = true;
		m_dirty_instances.clear();
		return true;
	}

	void Set_View(const View &view) noexcept
	{
		m_view = view;
		m_gpu_view.view_projection = Multiply(view.projection_matrix, view.view_matrix);
		m_view_dirty = true;
	}

	bool Render(CommandList &command_list, const FrameTargets &targets, bool clear_targets = true) noexcept
	{
		if (!Is_Initialized() || !targets.backbuffer.texture.Is_Valid() || !targets.depth.texture.Is_Valid())
			return false;
		if (!Sync_GPU_Data())
			return false;

		if (!Build_Visible_Set(m_scene, m_view, *m_visible))
			return false;
		if (!Build_LOD_Set(m_scene, m_meshes, *m_visible, m_view, *m_lod))
			return false;
		if (!Build_Draw_Data(*m_lod, m_gpu_scene, {0, m_pipeline, 0}, *m_draws))
			return false;

		m_bindings[0] = GraphResourceBinding::Texture(m_color_resource, targets.backbuffer.texture);
		m_bindings[1] = GraphResourceBinding::Texture(m_depth_resource, targets.depth.texture);
		if (!m_graph_compiled) {
			if (!m_plan.Compile(m_graph, m_bindings))
				return false;
			m_graph_compiled = true;
		}

		const RHIViewport viewport{
			targets.backbuffer.width != 0 ? 0u : 0u,
			targets.backbuffer.height != 0 ? 0u : 0u,
			targets.backbuffer.width,
			targets.backbuffer.height,
			0.0f,
			1.0f
		};
		const OpaquePassInput input{
			m_draws->Records(),
			{m_mesh_bindings.data(), m_gpu_scene.Meshes().size()},
			m_bindless.Resources(),
			m_color_resource,
			m_depth_resource,
			viewport,
			{0.035f, 0.045f, 0.075f, 1.0f},
			1.0f,
			clear_targets,
			clear_targets,
			{m_mesh_part_bindings.data(), m_mesh_part_bindings.size()}
		};
		return m_plan.Execute(m_graph, command_list, [&input](GraphPassHandle, CommandList &commands, const PassResources &resources) noexcept {
			return OpaquePass::Execute(commands, resources, input);
		});
	}

private:
	struct MeshStorage final
	{
		MeshHandle handle{};
		std::vector<std::byte> vertex_data;
		std::vector<std::byte> index_data;
		std::vector<MeshPart> parts;
	};

	static std::array<float, 16> Multiply(const Matrix4x4 &left, const Matrix4x4 &right) noexcept
	{
		std::array<float, 16> result{};
		for (std::size_t row = 0; row < 4; ++row)
			for (std::size_t column = 0; column < 4; ++column)
				for (std::size_t element = 0; element < 4; ++element)
					result[row * 4 + column] += left(row, element) * right(element, column);
		return result;
	}

	bool Sync_GPU_Data() noexcept
	{
		if (m_view_dirty && !m_device->Update_Buffer(m_view_buffer, 0, std::as_bytes(std::span<const GPUViewData>(&m_gpu_view, 1))))
			return false;
		m_view_dirty = false;

		if (m_scene_dirty) {
			if (!m_gpu_scene.Build(m_scene, m_meshes, m_textures, m_samplers, m_materials))
				return false;
			if (!m_device->Update_Buffer(m_instance_buffer, 0, std::as_bytes(m_gpu_scene.Instances())))
				return m_gpu_scene.Instances().empty();
			if (!Build_Mesh_Bindings())
				return false;
			m_scene_dirty = false;
			m_dirty_instances.clear();
			m_gpu_scene.Clear_Dirty();
			return true;
		}

		if (m_dirty_instances.empty())
			return true;
		for (const InstanceHandle handle : m_dirty_instances) {
			if (!m_gpu_scene.Sync_Instance(handle, m_scene))
				return false;
		}
		const std::span<const GPUInstanceData> instances = m_gpu_scene.Instances();
		if (!instances.empty() && !m_device->Update_Buffer(m_instance_buffer, 0, std::as_bytes(instances)))
			return false;
		m_dirty_instances.clear();
		m_gpu_scene.Clear_Dirty();
		return true;
	}

	bool Build_Mesh_Bindings() noexcept
	{
		const std::span<const GPUMeshData> gpu_meshes = m_gpu_scene.Meshes();
		if (gpu_meshes.size() > m_mesh_bindings.size())
			return false;
		for (OpaqueMeshBinding &binding : m_mesh_bindings)
			binding = {};
		m_mesh_part_bindings.clear();

		bool complete = true;
		m_meshes.For_Each([&](MeshHandle handle, const Mesh &mesh) noexcept {
			const std::uint32_t gpu_index = m_gpu_scene.Mesh_Index(handle);
			const GPUResidentMesh resident = m_residency->Mesh_Info(handle);
			if (gpu_index >= gpu_meshes.size() || !resident.vertex_buffer.Is_Valid() || !resident.index_buffer.Is_Valid()) {
				complete = false;
				return;
			}

			OpaqueMeshBinding &binding = m_mesh_bindings[gpu_index];
			binding.vertex_buffer = resident.vertex_buffer;
			binding.index_buffer = resident.index_buffer;
			binding.index_format = resident.index_format == MeshIndexFormat::UInt16 ? RHIIndexFormat::UInt16 : RHIIndexFormat::UInt32;
			binding.vertex_stride = resident.vertex_stride;
			binding.index_count = resident.index_count;
			binding.submesh_offset = static_cast<std::uint32_t>(m_mesh_part_bindings.size());
			binding.submesh_count = static_cast<std::uint32_t>(mesh.parts.size());
			for (const MeshPart &part : mesh.parts)
				m_mesh_part_bindings.push_back({part.first_index, part.index_count, part.base_vertex, 0});
		});
		return complete;
	}

	Device *m_device = nullptr;
	std::size_t m_instance_capacity = 0;
	MeshPool m_meshes;
	TexturePool m_textures;
	SamplerPool m_samplers;
	MaterialPool m_materials;
	RenderScene m_scene;
	GPUScene m_gpu_scene;
	ShaderLibrary m_shaders;
	BindlessResourceTable m_bindless;
	std::unique_ptr<GPUResourceResidency> m_residency;
	std::vector<std::unique_ptr<MeshStorage>> m_mesh_sources;
	std::vector<InstanceHandle> m_dirty_instances;
	std::vector<InstanceHandle> m_visible_storage;
	std::vector<LODSelection> m_lod_storage;
	std::vector<DrawData> m_draw_storage;
	std::vector<OpaqueMeshBinding> m_mesh_bindings;
	std::vector<OpaqueSubmeshBinding> m_mesh_part_bindings;
	std::unique_ptr<VisibleSet> m_visible;
	std::unique_ptr<LODSet> m_lod;
	std::unique_ptr<DrawSet> m_draws;
	View m_view{};
	GPUViewData m_gpu_view{};
	RHIBufferHandle m_instance_buffer{};
	RHIBufferHandle m_light_buffer{};
	RHIBufferHandle m_view_buffer{};
	ShaderHandle m_shader{};
	PipelineHandle m_pipeline{};
	MaterialHandle m_default_material{};
	RenderGraph m_graph;
	ExecutionPlan m_plan;
	std::array<GraphResourceBinding, 2> m_bindings{};
	GraphResourceHandle m_color_resource{};
	GraphResourceHandle m_depth_resource{};
	GraphPassHandle m_opaque_pass{};
	bool m_scene_dirty = true;
	bool m_view_dirty = false;
	bool m_graph_compiled = false;
};

export class StaticMeshBinding final
{
public:
	bool Replace(StaticMeshRenderer &renderer, const StaticMeshSource &source, const RenderTransform &transform,
		const RenderBounds &bounds, MaterialHandle material, RenderInstanceFlags flags,
		SubmeshVisibilityMask visibility_mask = All_Submeshes_Visible)
	{
		if (!renderer.Is_Initialized() || !material.Is_Valid())
			return false;

		const MeshHandle new_mesh = renderer.Create_Mesh(source);
		if (!new_mesh.Is_Valid())
			return false;

		const RenderInstance instance = Make_Instance(new_mesh, material, transform, bounds, flags, visibility_mask);
		if (m_instance.Is_Valid()) {
			if (!renderer.Update_Instance(m_instance, instance)) {
				renderer.Destroy_Mesh(new_mesh);
				return false;
			}
			if (m_mesh.Is_Valid())
				renderer.Destroy_Mesh(m_mesh);
		} else {
			const InstanceHandle new_instance = renderer.Create_Instance(instance);
			if (!new_instance.Is_Valid()) {
				renderer.Destroy_Mesh(new_mesh);
				return false;
			}
			m_instance = new_instance;
		}

		m_mesh = new_mesh;
		m_material = material;
		m_bounds = bounds;
		m_flags = flags;
		m_visibility_mask = visibility_mask;
		m_active = true;
		return true;
	}

	bool Update(StaticMeshRenderer &renderer, const RenderTransform &transform, RenderInstanceFlags flags) noexcept
	{
		if (!renderer.Is_Initialized() || !m_active || !m_instance.Is_Valid() || !m_mesh.Is_Valid() || !m_material.Is_Valid())
			return false;

		const RenderInstance instance = Make_Instance(m_mesh, m_material, transform, m_bounds, flags, m_visibility_mask);
		if (!renderer.Update_Instance(m_instance, instance))
			return false;

		m_flags = flags;
		return true;
	}

	bool Set_Submesh_Visibility(StaticMeshRenderer &renderer, SubmeshVisibilityMask visibility_mask) noexcept
	{
		if (!renderer.Is_Initialized() || !m_instance.Is_Valid() || !m_mesh.Is_Valid() || !m_material.Is_Valid())
			return false;
		if (!renderer.Update_Instance_Visibility(m_instance, visibility_mask))
			return false;

		m_visibility_mask = visibility_mask;
		return true;
	}

	bool Set_Submesh_Visible(StaticMeshRenderer &renderer, ModelPartId part, bool visible) noexcept
	{
		if (part >= renderer.Mesh_Part_Count(m_mesh))
			return false;

		return Set_Submesh_Visibility(renderer, Graphics::Set_Submesh_Visible(m_visibility_mask, part, visible));
	}

	bool Suspend(StaticMeshRenderer &renderer, const RenderTransform &transform) noexcept
	{
		if (!renderer.Is_Initialized() || !m_instance.Is_Valid() || !m_mesh.Is_Valid() || !m_material.Is_Valid())
			return false;

		const RenderInstance instance = Make_Instance(m_mesh, m_material, transform,
			m_bounds, m_flags | RenderInstanceFlags::Hidden, m_visibility_mask);
		if (!renderer.Update_Instance(m_instance, instance))
			return false;

		m_active = false;
		return true;
	}

	void Destroy(StaticMeshRenderer &renderer) noexcept
	{
		if (renderer.Is_Initialized()) {
			if (m_instance.Is_Valid())
				renderer.Destroy_Instance(m_instance);
			if (m_mesh.Is_Valid())
				renderer.Destroy_Mesh(m_mesh);
		}
		Reset();
	}

	void Reset() noexcept
	{
		m_mesh = {};
		m_material = {};
		m_instance = {};
		m_bounds = {};
		m_flags = RenderInstanceFlags::None;
		m_visibility_mask = All_Submeshes_Visible;
		m_active = false;
	}

	bool Is_Active() const noexcept
	{
		return m_active;
	}

	bool Has_Instance() const noexcept
	{
		return m_instance.Is_Valid();
	}

	MeshHandle Mesh() const noexcept
	{
		return m_mesh;
	}

	MaterialHandle Material() const noexcept
	{
		return m_material;
	}

	InstanceHandle Instance() const noexcept
	{
		return m_instance;
	}

	const RenderBounds &Bounds() const noexcept
	{
		return m_bounds;
	}

	SubmeshVisibilityMask Visibility() const noexcept
	{
		return m_visibility_mask;
	}

private:
	static RenderInstance Make_Instance(MeshHandle mesh, MaterialHandle material, const RenderTransform &transform,
		const RenderBounds &bounds, RenderInstanceFlags flags, SubmeshVisibilityMask visibility_mask) noexcept
	{
		RenderInstance instance;
		instance.transform = transform;
		instance.bounds = bounds;
		instance.mesh = mesh;
		instance.material = material;
		instance.flags = flags;
		instance.visibility_mask = visibility_mask;
		return instance;
	}

	MeshHandle m_mesh{};
	MaterialHandle m_material{};
	InstanceHandle m_instance{};
	RenderBounds m_bounds{};
	RenderInstanceFlags m_flags = RenderInstanceFlags::None;
	SubmeshVisibilityMask m_visibility_mask = All_Submeshes_Visible;
	bool m_active = false;
};

export StaticMeshRenderer &GetStaticMeshRenderer() noexcept
{
	static StaticMeshRenderer renderer;
	return renderer;
}

}
