module;

#define BOOST_TEST_MODULE GraphicsBeamTests

#include <boost/test/included/unit_test.hpp>

#include <array>
#include <cstdint>
#include <type_traits>

export module Graphics.Scene.Beams.Tests;

import Graphics.Scene.Beams;

using namespace Graphics;

static_assert(std::is_nothrow_move_constructible_v<BeamDescription>);
static_assert(std::is_nothrow_move_assignable_v<BeamDescription>);

BOOST_AUTO_TEST_CASE(beam_storage_uses_typed_handles_and_rejects_stale_handles)
{
	BeamSet beams;
	beams.Reserve(2);
	const BeamHandle first = beams.Create();
	const BeamHandle second = beams.Create();
	BOOST_REQUIRE(first.Is_Valid());
	BOOST_REQUIRE(second.Is_Valid());
	BOOST_CHECK(first != second);

	BeamDescription updated;
	updated.start = {1.0f, 2.0f, 3.0f};
	updated.end = {4.0f, 5.0f, 6.0f};
	updated.width = 2.0f;
	BOOST_REQUIRE(beams.Update(first, updated));
	const BeamData data = beams.Data();
	BOOST_CHECK_EQUAL(data.start_x[beams.Dense_Index(first)], 1.0f);
	BOOST_CHECK_EQUAL(data.end_z[beams.Dense_Index(first)], 6.0f);
	BOOST_CHECK_EQUAL(data.widths[beams.Dense_Index(first)], 2.0f);

	BOOST_REQUIRE(beams.Destroy(first));
	BOOST_CHECK(!beams.Is_Valid(first));
	BOOST_CHECK(!beams.Update(first, updated));
	const BeamHandle reused = beams.Create(updated);
	BOOST_CHECK_EQUAL(reused.Get_Index(), first.Get_Index());
	BOOST_CHECK(reused.Get_Generation() != first.Get_Generation());
	BOOST_CHECK(!beams.Is_Valid(first));
	BOOST_CHECK(beams.Is_Valid(second));
}

BOOST_AUTO_TEST_CASE(beam_vertex_generation_is_batched_and_deterministic)
{
	BeamSet beams;
	BeamDescription first;
	first.start = {-0.75f, 0.0f, 0.0f};
	first.end = {0.75f, 0.0f, 0.0f};
	first.width = 0.10f;
	first.color = {1.0f, 0.25f, 0.5f, 1.0f};
	first.opacity = 0.5f;
	beams.Create(first);
	BeamDescription disabled = first;
	disabled.flags = BeamFlags::None;
	beams.Create(disabled);

	std::array<BeamVertex, 12> vertices{};
	const std::size_t count = Build_Beam_Vertices(beams.Data(), {}, vertices);
	BOOST_CHECK_EQUAL(count, 6);
	BOOST_CHECK_EQUAL(vertices[0].position[0], -0.75f);
	BOOST_CHECK_EQUAL(vertices[0].position[1], -0.05f);
	BOOST_CHECK_EQUAL(vertices[0].color[3], 0.5f);

	std::array<BeamVertex, 12> repeat{};
	BOOST_REQUIRE_EQUAL(Build_Beam_Vertices(beams.Data(), {}, repeat), count);
	for (std::size_t index = 0; index < count; ++index) {
		BOOST_CHECK_EQUAL(vertices[index].position[0], repeat[index].position[0]);
		BOOST_CHECK_EQUAL(vertices[index].position[1], repeat[index].position[1]);
		BOOST_CHECK_EQUAL(vertices[index].color[3], repeat[index].color[3]);
	}
}

BOOST_AUTO_TEST_CASE(beam_pass_declares_color_and_depth_writes)
{
	RenderGraph graph;
	const GraphResourceHandle color = graph.Create_Resource({GraphResourceKind::Texture});
	const GraphResourceHandle depth = graph.Create_Resource({GraphResourceKind::Texture});
	const GraphPassHandle pass = BeamPass::Add_To_Graph(graph, color, depth, 30);
	BOOST_REQUIRE(pass.Is_Valid());
	BOOST_REQUIRE(graph.Compile());
	BOOST_REQUIRE_EQUAL(graph.Execution_Order().size(), 1);
	BOOST_CHECK(graph.Pass_Resources(pass)[0].resource == color);
	BOOST_CHECK(graph.Pass_Resources(pass)[1].resource == depth);
}
