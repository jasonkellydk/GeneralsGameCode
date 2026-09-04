module;

#define BOOST_TEST_MODULE GraphicsStaticMeshRendererTests

#include <boost/test/included/unit_test.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#if defined(_WIN32)
#include <filesystem>
#endif

export module Graphics.Scene.StaticMeshes.Tests;

import Graphics.Scene.StaticMeshes;

using namespace Graphics;

BOOST_AUTO_TEST_CASE(static_mesh_source_contract)
{
	const std::array<StaticMeshVertex, 3> vertices = {{
		{{-1.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
		{{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.5f, 0.0f}},
		{{1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
	}};
	const std::array<std::uint16_t, 3> indices = {0, 1, 2};
	const StaticMeshSource source{
		3,
		3,
		static_cast<std::uint32_t>(sizeof(StaticMeshVertex)),
		MeshIndexFormat::UInt16,
		std::as_bytes(std::span<const StaticMeshVertex>(vertices)),
		std::as_bytes(std::span<const std::uint16_t>(indices)),
		{0.0f, 0.0f, 0.0f},
		1.0f
	};

	BOOST_TEST(Validate_Static_Mesh_Source(source));
	BOOST_TEST(source.vertex_count == 3u);
	BOOST_TEST(source.index_count == 3u);
	BOOST_TEST(MeshHandle(0, 1).Is_Valid());
}

BOOST_AUTO_TEST_CASE(static_mesh_source_rejects_incompatible_layout)
{
	StaticMeshSource source;
	source.vertex_count = 3;
	source.index_count = 3;
	source.vertex_stride = 16;
	source.index_format = MeshIndexFormat::UInt16;
	BOOST_TEST(!Validate_Static_Mesh_Source(source));
}

#if defined(_WIN32)

import Graphics.Backends.DX11;
import Graphics.Testing.VisualRegression;

#ifndef GRAPHICS_STATIC_MESH_REFERENCE_DIRECTORY
#define GRAPHICS_STATIC_MESH_REFERENCE_DIRECTORY "."
#endif

#ifndef GRAPHICS_STATIC_MESH_FAILURE_DIRECTORY
#define GRAPHICS_STATIC_MESH_FAILURE_DIRECTORY "."
#endif

#ifndef GRAPHICS_STATIC_MESH_SHADER_DIRECTORY
#define GRAPHICS_STATIC_MESH_SHADER_DIRECTORY "."
#endif

namespace
{
bool Render_Static_Mesh(Device &, CommandList &commands, RHITextureHandle color_target, RHITextureHandle depth_target, RHIViewport viewport, void *context) noexcept
{
	StaticMeshRenderer &renderer = *static_cast<StaticMeshRenderer *>(context);
	return renderer.Render(commands, {{color_target, viewport.width, viewport.height}, {depth_target, viewport.width, viewport.height}});
}
}

BOOST_AUTO_TEST_CASE(static_mesh_variant_switch_keeps_instance_handle)
{
	DX11Device device({true});
	BOOST_REQUIRE(device.Is_Valid());

	StaticMeshRenderer renderer;
	BOOST_REQUIRE(renderer.Initialize(device, std::filesystem::path(GRAPHICS_STATIC_MESH_SHADER_DIRECTORY), 2, 1));

	const std::array<StaticMeshVertex, 3> vertices = {{
		{{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
		{{0.0f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.5f, 0.0f}},
		{{0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
	}};
	const std::array<std::uint16_t, 3> indices = {0, 1, 2};
	const StaticMeshSource source{
		3,
		3,
		static_cast<std::uint32_t>(sizeof(StaticMeshVertex)),
		MeshIndexFormat::UInt16,
		std::as_bytes(std::span<const StaticMeshVertex>(vertices)),
		std::as_bytes(std::span<const std::uint16_t>(indices)),
		{0.0f, 0.0f, 0.0f},
		1.0f
	};
	const MeshHandle first_mesh = renderer.Create_Mesh(source);
	const MeshHandle second_mesh = renderer.Create_Mesh(source);
	BOOST_REQUIRE(first_mesh.Is_Valid());
	BOOST_REQUIRE(second_mesh.Is_Valid());

	StaticMeshBinding binding;
	RenderTransform transform;
	transform.matrix = Matrix4x4::Identity().values;
	const RenderBounds bounds{{0.0f, 0.0f, 0.0f}, 1.0f};
	const RenderInstanceFlags flags = RenderInstanceFlags::CastsShadow;
	const StaticMeshSource invalid_source{};
	BOOST_CHECK(!binding.Replace(renderer, invalid_source, transform, bounds, renderer.Default_Material(), flags));
	BOOST_CHECK(!binding.Is_Active());

	BOOST_REQUIRE(binding.Replace(renderer, source, transform, bounds, renderer.Default_Material(), flags));
	BOOST_REQUIRE(binding.Is_Active());
	BOOST_REQUIRE(binding.Instance().Is_Valid());
	const InstanceHandle stable_instance = binding.Instance();
	const MeshHandle stable_mesh = binding.Mesh();
	BOOST_CHECK(!binding.Replace(renderer, invalid_source, transform, bounds, renderer.Default_Material(), flags));
	BOOST_CHECK(binding.Is_Active());
	BOOST_CHECK(binding.Instance() == stable_instance);
	BOOST_CHECK(binding.Mesh() == stable_mesh);
	BOOST_REQUIRE(binding.Replace(renderer, source, transform, bounds, renderer.Default_Material(), flags));
	BOOST_CHECK(binding.Instance() == stable_instance);
	BOOST_CHECK(binding.Mesh() != first_mesh);
	BOOST_CHECK(binding.Mesh() != second_mesh);

	BOOST_REQUIRE(binding.Suspend(renderer, transform));
	BOOST_CHECK(!binding.Is_Active());
	BOOST_CHECK(binding.Instance() == stable_instance);
	BOOST_CHECK(!binding.Replace(renderer, invalid_source, transform, bounds, renderer.Default_Material(), flags));
	BOOST_CHECK(binding.Instance() == stable_instance);

	binding.Destroy(renderer);
	BOOST_CHECK(!binding.Has_Instance());
	BOOST_CHECK(!binding.Is_Active());
	BOOST_CHECK(!binding.Instance().Is_Valid());

	BOOST_CHECK(renderer.Destroy_Mesh(second_mesh));
	BOOST_CHECK(renderer.Destroy_Mesh(first_mesh));
	renderer.Shutdown();
}

BOOST_AUTO_TEST_CASE(static_mesh_submesh_visibility_preserves_instance_state)
{
	DX11Device device({true});
	BOOST_REQUIRE(device.Is_Valid());

	StaticMeshRenderer renderer;
	BOOST_REQUIRE(renderer.Initialize(device, std::filesystem::path(GRAPHICS_STATIC_MESH_SHADER_DIRECTORY), 1, 1));

	const std::array<StaticMeshVertex, 6> vertices = {{
		{{0.0f, 0.78f, 0.35f}, {1.0f, 0.20f, 0.10f, 1.0f}, {0.5f, 0.0f}},
		{{-0.72f, -0.62f, 0.35f}, {0.10f, 0.85f, 0.20f, 1.0f}, {0.0f, 1.0f}},
		{{0.72f, -0.62f, 0.35f}, {0.10f, 0.20f, 0.95f, 1.0f}, {1.0f, 1.0f}},
		{{-0.25f, 0.25f, 0.35f}, {1.0f, 1.0f, 0.10f, 1.0f}, {0.0f, 0.0f}},
		{{-0.65f, -0.25f, 0.35f}, {0.95f, 0.10f, 1.0f, 1.0f}, {0.0f, 1.0f}},
		{{0.15f, -0.25f, 0.35f}, {0.10f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
	}};
	const std::array<std::uint16_t, 6> indices = {0, 2, 1, 3, 5, 4};
	const std::array<MeshPart, 2> parts = {{{0, 3, 0}, {3, 3, 0}}};
	const StaticMeshSource source{
		6,
		6,
		static_cast<std::uint32_t>(sizeof(StaticMeshVertex)),
		MeshIndexFormat::UInt16,
		std::as_bytes(std::span<const StaticMeshVertex>(vertices)),
		std::as_bytes(std::span<const std::uint16_t>(indices)),
		{0.0f, 0.0f, 0.35f},
		1.0f,
		parts
	};

	StaticMeshBinding binding;
	RenderTransform transform;
	transform.matrix = Matrix4x4::Identity().values;
	const RenderBounds bounds{{0.0f, 0.0f, 0.35f}, 1.0f};
	const RenderInstanceFlags flags = RenderInstanceFlags::CastsShadow;
	BOOST_REQUIRE(binding.Replace(renderer, source, transform, bounds, renderer.Default_Material(), flags));
	const InstanceHandle stable_instance = binding.Instance();
	BOOST_REQUIRE(binding.Set_Submesh_Visible(renderer, 1, false));
	BOOST_CHECK(binding.Instance() == stable_instance);
	BOOST_CHECK(!Is_Submesh_Visible(binding.Visibility(), 1));
	BOOST_CHECK(Is_Submesh_Visible(binding.Visibility(), 0));
	BOOST_CHECK(binding.Mesh().Is_Valid());
	BOOST_CHECK(!binding.Set_Submesh_Visible(renderer, 2, false));

	renderer.Set_View({
		Matrix4x4::Identity(),
		Matrix4x4::Identity(),
		{},
		{0.0f, 0.0f, 128.0f, 72.0f, 0.0f, 1.0f}
	});
	VisualRegressionHarness harness({
		128,
		72,
		2,
		std::filesystem::path(GRAPHICS_STATIC_MESH_REFERENCE_DIRECTORY),
		std::filesystem::path(GRAPHICS_STATIC_MESH_FAILURE_DIRECTORY)
	});
	const VisualComparisonResult result = harness.Run(device, "StaticMeshRenderer.SubmeshVisibility", Render_Static_Mesh, &renderer);
	BOOST_CHECK_MESSAGE(result.expected_loaded, "missing colocated static mesh submesh reference image");
	BOOST_CHECK_MESSAGE(result.matched, "static mesh submesh visibility visual regression mismatch");

	binding.Destroy(renderer);
	renderer.Shutdown();
}

BOOST_AUTO_TEST_CASE(static_mesh_renderer_matches_colocated_production_golden_image)
{
	DX11Device device({true});
	BOOST_REQUIRE(device.Is_Valid());

	StaticMeshRenderer renderer;
	BOOST_REQUIRE(renderer.Initialize(device, std::filesystem::path(GRAPHICS_STATIC_MESH_SHADER_DIRECTORY), 1, 1));
	const std::array<StaticMeshVertex, 3> vertices = {{
		{{0.0f, 0.78f, 0.35f}, {1.0f, 0.20f, 0.10f, 1.0f}, {0.5f, 0.0f}},
		{{-0.72f, -0.62f, 0.35f}, {0.10f, 0.85f, 0.20f, 1.0f}, {0.0f, 1.0f}},
		{{0.72f, -0.62f, 0.35f}, {0.10f, 0.20f, 0.95f, 1.0f}, {1.0f, 1.0f}}
	}};
	const std::array<std::uint16_t, 3> indices = {0, 2, 1};
	const MeshHandle mesh = renderer.Create_Mesh({
		3,
		3,
		static_cast<std::uint32_t>(sizeof(StaticMeshVertex)),
		MeshIndexFormat::UInt16,
		std::as_bytes(std::span<const StaticMeshVertex>(vertices)),
		std::as_bytes(std::span<const std::uint16_t>(indices)),
		{0.0f, 0.0f, 0.35f},
		1.0f
	});
	BOOST_REQUIRE(mesh.Is_Valid());

	RenderInstance instance;
	instance.transform.matrix = Matrix4x4::Identity().values;
	instance.bounds = {{0.0f, 0.0f, 0.35f}, 1.0f};
	instance.mesh = mesh;
	instance.material = renderer.Default_Material();
	const InstanceHandle instance_handle = renderer.Create_Instance(instance);
	BOOST_REQUIRE(instance_handle.Is_Valid());
	renderer.Set_View({
		Matrix4x4::Identity(),
		Matrix4x4::Identity(),
		{},
		{0.0f, 0.0f, 128.0f, 72.0f, 0.0f, 1.0f}
	});

	VisualRegressionHarness harness({
		128,
		72,
		2,
		std::filesystem::path(GRAPHICS_STATIC_MESH_REFERENCE_DIRECTORY),
		std::filesystem::path(GRAPHICS_STATIC_MESH_FAILURE_DIRECTORY)
	});
	const VisualComparisonResult result = harness.Run(device, "StaticMeshRenderer", Render_Static_Mesh, &renderer);
	BOOST_CHECK_MESSAGE(result.expected_loaded, "missing colocated static mesh renderer reference image");
	BOOST_CHECK_MESSAGE(result.matched, "static mesh renderer visual regression mismatch");

	BOOST_REQUIRE(renderer.Destroy_Instance(instance_handle));
	BOOST_REQUIRE(renderer.Destroy_Mesh(mesh));
	renderer.Shutdown();
}

#endif
