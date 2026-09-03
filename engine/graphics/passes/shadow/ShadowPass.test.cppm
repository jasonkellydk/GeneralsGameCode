module;

#define BOOST_TEST_MODULE GraphicsShadowPassTests

#include <boost/test/included/unit_test.hpp>

#include <array>
#include <cstdint>
#include <span>

export module Graphics.Passes.Shadow.Tests;

import Graphics.Passes.Shadow;

using namespace Graphics;

class RecordingCommandList final : public CommandList
{
public:
	bool Bind_Pipeline(RHIPipelineHandle pipeline) noexcept override
	{
		++pipeline_bind_count;
		return pipeline.Is_Valid();
	}

	bool Set_Bindless_Resources(std::span<const RHIBindlessResource>) noexcept override
	{
		return true;
	}

	bool Set_Render_Targets(RHITextureHandle, RHITextureHandle) noexcept override
	{
		return false;
	}

	bool Set_Depth_Target(RHITextureHandle target) noexcept override
	{
		++depth_target_count;
		last_depth_target = target;
		return target.Is_Valid();
	}

	bool Clear(const std::array<float, 4> &, float) noexcept override
	{
		return false;
	}

	bool Clear_Depth(float) noexcept override
	{
		++clear_depth_count;
		return true;
	}

	bool Set_Viewport(RHIViewport viewport) noexcept override
	{
		viewport_set = viewport.width != 0 && viewport.height != 0;
		return viewport_set;
	}

	bool Set_Vertex_Buffer(std::uint32_t, RHIBufferHandle buffer, std::uint32_t stride, std::uint32_t) noexcept override
	{
		++vertex_buffer_count;
		return buffer.Is_Valid() && stride != 0;
	}

	bool Set_Index_Buffer(RHIBufferHandle buffer, RHIIndexFormat, std::uint32_t) noexcept override
	{
		++index_buffer_count;
		return buffer.Is_Valid();
	}

	bool Draw(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) noexcept override
	{
		return false;
	}

	bool Draw_Indexed(std::uint32_t index_count, std::uint32_t, std::int32_t, std::uint32_t instance_count, std::uint32_t) noexcept override
	{
		++draw_count;
		return index_count != 0 && instance_count != 0;
	}

	std::uint32_t depth_target_count = 0;
	std::uint32_t clear_depth_count = 0;
	std::uint32_t pipeline_bind_count = 0;
	std::uint32_t vertex_buffer_count = 0;
	std::uint32_t index_buffer_count = 0;
	std::uint32_t draw_count = 0;
	RHITextureHandle last_depth_target{};
	bool viewport_set = false;
};

class TestSwapChain final : public SwapChain
{
public:
	bool Is_Valid() const noexcept override { return true; }
	RHIBackbuffer Backbuffer() const noexcept override { return {}; }
	RHIDepthTarget Depth_Target() const noexcept override { return {}; }
	bool Resize(std::uint32_t, std::uint32_t) override { return true; }
	bool Present() noexcept override { return true; }
};

class TestDevice final : public Device
{
public:
	bool Is_Valid() const noexcept override { return true; }
	RHIBufferHandle Create_Buffer(const RHIBuffer &) override { return RHIBufferHandle(++m_buffer_index, 1); }
	RHITextureHandle Create_Texture(const RHITexture &) override { return RHITextureHandle(++m_texture_index, 1); }
	RHIPipelineHandle Create_Pipeline(const RHIPipeline &) override { return RHIPipelineHandle(1, 1); }
	bool Destroy_Buffer(RHIBufferHandle) noexcept override { return true; }
	bool Destroy_Texture(RHITextureHandle) noexcept override { ++destroyed_texture_count; return true; }
	bool Destroy_Pipeline(RHIPipelineHandle) noexcept override { return true; }
	CommandList &Immediate_Command_List() noexcept override { return m_command_list; }
	SwapChain &Get_Swap_Chain() noexcept override { return m_swap_chain; }
	bool Begin_Frame() noexcept override { return true; }
	bool End_Frame() noexcept override { return true; }

	std::uint32_t destroyed_texture_count = 0;

private:
	RecordingCommandList m_command_list;
	TestSwapChain m_swap_chain;
	std::uint32_t m_buffer_index = 0;
	std::uint32_t m_texture_index = 0;
};

BOOST_AUTO_TEST_CASE(shadow_passes_have_deterministic_cascade_order)
{
	TestDevice device;
	RenderGraph graph;
	ShadowMapResources shadow_maps;
	BOOST_REQUIRE(shadow_maps.Initialize(device, graph, 3, 256));

	std::array<GraphPassHandle, 3> passes{};
	BOOST_REQUIRE(ShadowPass::Add_Cascades_To_Graph(graph, shadow_maps, passes, 40));

	ExecutionPlan plan;
	const std::array<GraphResourceBinding, 3> bindings = {
		GraphResourceBinding::Texture(shadow_maps.Target(0), shadow_maps.Texture(0)),
		GraphResourceBinding::Texture(shadow_maps.Target(1), shadow_maps.Texture(1)),
		GraphResourceBinding::Texture(shadow_maps.Target(2), shadow_maps.Texture(2))
	};
	BOOST_REQUIRE(plan.Compile(graph, bindings));
	BOOST_REQUIRE(plan.Passes().size() == passes.size());
	for (std::size_t index = 0; index < passes.size(); ++index)
		BOOST_CHECK(plan.Passes()[index] == passes[index]);

	shadow_maps.Shutdown(device);
	BOOST_CHECK(device.destroyed_texture_count == 3);
}

BOOST_AUTO_TEST_CASE(shadow_pass_records_depth_only_geometry_through_the_graph)
{
	TestDevice device;
	RenderGraph graph;
	ShadowMapResources shadow_maps;
	BOOST_REQUIRE(shadow_maps.Initialize(device, graph, 1, 128));

	std::array<GraphPassHandle, 1> passes{};
	BOOST_REQUIRE(ShadowPass::Add_Cascades_To_Graph(graph, shadow_maps, passes));
	const std::array<GraphResourceBinding, 1> bindings = {
		GraphResourceBinding::Texture(shadow_maps.Target(0), shadow_maps.Texture(0))
	};
	ExecutionPlan plan;
	BOOST_REQUIRE(plan.Compile(graph, bindings));

	std::array<GPUInstanceData, 2> instances{};
	instances[0].flags = static_cast<std::uint32_t>(RenderInstanceFlags::CastsShadow);
	const std::array<DrawData, 2> draws = {
		DrawData{0, 0, 0, 1, PipelineHandle(3, 1), 0},
		DrawData{0, 0, 1, 1, PipelineHandle(3, 1), 0}
	};
	const std::array<OpaqueMeshBinding, 1> meshes = {
		OpaqueMeshBinding{RHIBufferHandle(4, 1), RHIBufferHandle(5, 1), RHIIndexFormat::UInt32, 32, 6, 0, 0}
	};
	ShadowPassInput input{
		draws,
		meshes,
		instances,
		PipelineHandle(3, 1),
		shadow_maps.Target(0),
		{0, 0, 128, 128, 0.0f, 1.0f},
		1.0f
	};

	RecordingCommandList command_list;
	BOOST_REQUIRE(plan.Execute(graph, command_list, [&](GraphPassHandle pass, CommandList &commands, const PassResources &resources) noexcept {
		return pass == passes[0] && ShadowPass::Execute(commands, resources, input);
	}));
	BOOST_CHECK(command_list.depth_target_count == 1);
	BOOST_CHECK(command_list.clear_depth_count == 1);
	BOOST_CHECK(command_list.pipeline_bind_count == 1);
	BOOST_CHECK(command_list.vertex_buffer_count == 1);
	BOOST_CHECK(command_list.index_buffer_count == 1);
	BOOST_CHECK(command_list.draw_count == 1);
	BOOST_CHECK(command_list.last_depth_target == shadow_maps.Texture(0));

	shadow_maps.Shutdown(device);
}

BOOST_AUTO_TEST_CASE(shadow_pass_rejects_invalid_declared_depth_resource)
{
	RenderGraph graph;
	const GraphResourceHandle depth_target = graph.Create_Resource({GraphResourceKind::Texture});
	const std::array<GraphResourceUse, 1> uses = {GraphResourceUse::Write(depth_target)};
	const GraphPassHandle pass = graph.Add_Pass({}, uses);
	const std::array<GraphResourceBinding, 1> bindings = {
		GraphResourceBinding::Texture(depth_target, RHITextureHandle(1, 1))
	};
	ExecutionPlan plan;
	BOOST_REQUIRE(plan.Compile(graph, bindings));
	ShadowPassInput input{};
	RecordingCommandList command_list;
	BOOST_CHECK(!plan.Execute(graph, command_list, [&](GraphPassHandle current_pass, CommandList &commands, const PassResources &resources) noexcept {
		return current_pass == pass && ShadowPass::Execute(commands, resources, input);
	}));
}
