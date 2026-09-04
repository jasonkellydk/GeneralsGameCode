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
