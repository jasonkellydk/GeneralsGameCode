module;

#define BOOST_TEST_MODULE GraphicsFrameTests

#include <boost/test/included/unit_test.hpp>

#include <array>
#include <span>

export module Graphics.RHI.Frame.Tests;

import Graphics.RHI.Frame;

class TestCommandList final : public CommandList
{
public:
	bool Bind_Pipeline(RHIPipelineHandle) noexcept override { return true; }
	bool Set_Bindless_Resources(std::span<const RHIBindlessResource>) noexcept override { return true; }
	bool Set_Render_Targets(RHITextureHandle, RHITextureHandle) noexcept override { return true; }
	bool Set_Depth_Target(RHITextureHandle) noexcept override { return true; }
	bool Clear(const std::array<float, 4> &, float) noexcept override { return true; }
	bool Clear_Depth(float) noexcept override { return true; }
	bool Set_Viewport(RHIViewport) noexcept override { return true; }
	bool Set_Vertex_Buffer(std::uint32_t, RHIBufferHandle, std::uint32_t, std::uint32_t) noexcept override { return true; }
	bool Set_Index_Buffer(RHIBufferHandle, RHIIndexFormat, std::uint32_t) noexcept override { return true; }
	bool Draw(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) noexcept override { return true; }
	bool Draw_Indexed(std::uint32_t, std::uint32_t, std::int32_t, std::uint32_t, std::uint32_t) noexcept override { return true; }
};

class TestSwapChain final : public SwapChain
{
public:
	bool Is_Valid() const noexcept override { return m_valid; }
	RHIBackbuffer Backbuffer() const noexcept override { return m_backbuffer; }
	RHIDepthTarget Depth_Target() const noexcept override { return m_depth_target; }

	bool Resize(std::uint32_t width, std::uint32_t height) override
	{
		if (!m_valid || width == 0 || height == 0)
			return false;

		m_backbuffer.width = width;
		m_backbuffer.height = height;
		m_depth_target.width = width;
		m_depth_target.height = height;
		return true;
	}

	bool Present() noexcept override
	{
		++m_present_count;
		return m_valid;
	}

	std::uint32_t Present_Count() const noexcept { return m_present_count; }

private:
	bool m_valid = true;
	RHIBackbuffer m_backbuffer{{1, 1}, 1280, 720};
	RHIDepthTarget m_depth_target{{2, 1}, 1280, 720};
	std::uint32_t m_present_count = 0;
};

class TestDevice final : public Device
{
public:
	bool Is_Valid() const noexcept override { return true; }
	RHIBufferHandle Create_Buffer(const RHIBuffer &) override { return {}; }
	RHITextureHandle Create_Texture(const RHITexture &) override { return {}; }
	RHIPipelineHandle Create_Pipeline(const RHIPipeline &) override { return {}; }
	bool Destroy_Buffer(RHIBufferHandle) noexcept override { return true; }
	bool Destroy_Texture(RHITextureHandle) noexcept override { return true; }
	bool Destroy_Pipeline(RHIPipelineHandle) noexcept override { return true; }
	CommandList &Immediate_Command_List() noexcept override { return m_command_list; }
	SwapChain &Get_Swap_Chain() noexcept override { return m_swap_chain; }

	bool Begin_Frame() noexcept override
	{
		if (m_frame_active)
			return false;

		m_frame_active = true;
		return true;
	}

	bool End_Frame() noexcept override
	{
		if (!m_frame_active)
			return false;

		m_frame_active = false;
		return true;
	}

	bool Is_Frame_Active() const noexcept { return m_frame_active; }
	TestSwapChain &Test_Swap_Chain() noexcept { return m_swap_chain; }

private:
	TestCommandList m_command_list;
	TestSwapChain m_swap_chain;
	bool m_frame_active = false;
};

BOOST_AUTO_TEST_CASE(frame_binds_current_targets_and_controls_lifecycle)
{
	RenderGraph graph;
	const GraphResourceHandle color = graph.Create_Resource({GraphResourceKind::Texture});
	const GraphResourceHandle depth = graph.Create_Resource({GraphResourceKind::Texture});
	std::array<GraphResourceBinding, 2> bindings{};
	TestDevice device;
	Frame frame;

	BOOST_CHECK(!frame.Is_Active());
	BOOST_CHECK(!frame.End(device));
	BOOST_REQUIRE(frame.Begin(graph, device, color, depth, bindings));
	BOOST_CHECK(frame.Is_Active());
	BOOST_CHECK(device.Is_Frame_Active());
	BOOST_CHECK(bindings[0].resource == color);
	BOOST_CHECK(bindings[1].resource == depth);
	BOOST_CHECK(bindings[0].texture == device.Get_Swap_Chain().Backbuffer().texture);
	BOOST_CHECK(bindings[1].texture == device.Get_Swap_Chain().Depth_Target().texture);
	BOOST_CHECK(!frame.Present(device));
	BOOST_REQUIRE(frame.End(device));
	BOOST_CHECK(!frame.Is_Active());
	BOOST_CHECK(!device.Is_Frame_Active());
	BOOST_REQUIRE(frame.Present(device));
	BOOST_CHECK_EQUAL(device.Test_Swap_Chain().Present_Count(), 1);
	BOOST_CHECK(!frame.Present(device));
}

BOOST_AUTO_TEST_CASE(frame_rejects_invalid_graph_targets)
{
	RenderGraph graph;
	const GraphResourceHandle color = graph.Create_Resource({GraphResourceKind::Texture});
	const GraphResourceHandle buffer = graph.Create_Resource({GraphResourceKind::Buffer});
	std::array<GraphResourceBinding, 2> bindings{};
	TestDevice device;
	Frame frame;

	BOOST_CHECK(!frame.Begin(graph, device, color, buffer, bindings));
	BOOST_CHECK(!frame.Begin(graph, device, color, {}, bindings));
}
