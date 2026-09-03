module;

#include <cstdint>
#include <cstddef>
#include <span>

export module Graphics.RHI.Frame;

export import Graphics.RenderGraph.Execution;
export import Graphics.RHI;

namespace Graphics
{

export class Frame final
{
public:
	bool Begin(RenderGraph &graph, Device &device, GraphResourceHandle color_target, GraphResourceHandle depth_target, std::span<GraphResourceBinding> bindings) noexcept
	{
		if (m_active || bindings.size() < 2 || color_target == depth_target || !graph.Is_Resource_Valid(color_target) || !graph.Is_Resource_Valid(depth_target) || graph.Resource_Kind(color_target) != GraphResourceKind::Texture || graph.Resource_Kind(depth_target) != GraphResourceKind::Texture)
			return false;

		const RHIBackbuffer backbuffer = device.Get_Swap_Chain().Backbuffer();
		const RHIDepthTarget depth = device.Get_Swap_Chain().Depth_Target();
		if (!device.Get_Swap_Chain().Is_Valid() || !backbuffer.texture.Is_Valid() || !depth.texture.Is_Valid())
			return false;

		bindings[0] = GraphResourceBinding::Texture(color_target, backbuffer.texture);
		bindings[1] = GraphResourceBinding::Texture(depth_target, depth.texture);
		if (!device.Begin_Frame())
			return false;

		m_active = true;
		m_ready_to_present = false;
		return true;
	}

	bool End(Device &device) noexcept
	{
		if (!m_active || !device.End_Frame())
			return false;

		m_active = false;
		m_ready_to_present = true;
		return true;
	}

	bool Present(Device &device) noexcept
	{
		if (m_active || !m_ready_to_present)
			return false;

		if (!device.Get_Swap_Chain().Present())
			return false;

		m_ready_to_present = false;
		return true;
	}

	bool Is_Active() const noexcept
	{
		return m_active;
	}

private:
	bool m_active = false;
	bool m_ready_to_present = false;
};

export struct FrameTargets final
{
	RHIBackbuffer backbuffer{};
	RHIDepthTarget depth{};
};

export enum class FrameOwnerPhase : std::uint8_t
{
	Idle,
	Legacy,
	Modern,
	ReadyToPresent
};

export class FrameOwner final
{
public:
	bool Begin_Frame(Device &device) noexcept
	{
		if (m_phase != FrameOwnerPhase::Idle || !device.Is_Valid())
			return Reject();

		SwapChain &swap_chain = device.Get_Swap_Chain();
		if (!swap_chain.Is_Valid())
			return Reject();

		const RHIBackbuffer backbuffer = swap_chain.Backbuffer();
		const RHIDepthTarget depth = swap_chain.Depth_Target();
		if (!backbuffer.texture.Is_Valid() || !depth.texture.Is_Valid())
			return Reject();

		if (!device.Begin_Frame())
			return Reject();

		m_device = &device;
		m_targets = {backbuffer, depth};
		m_phase = FrameOwnerPhase::Legacy;
		return true;
	}

	bool Begin_Modern_Phase(Device &device) noexcept
	{
		if (m_phase != FrameOwnerPhase::Legacy || m_device != &device)
			return Reject();

		CommandList &command_list = device.Immediate_Command_List();
		if (!command_list.Reset_State() || !command_list.Set_Render_Targets(m_targets.backbuffer.texture, m_targets.depth.texture))
			return Reject();

		m_phase = FrameOwnerPhase::Modern;
		return true;
	}

	CommandList *Modern_Commands(Device &device) noexcept
	{
		if (m_phase != FrameOwnerPhase::Modern || m_device != &device)
		{
			Reject();
			return nullptr;
		}

		return &device.Immediate_Command_List();
	}

	bool End_Frame(Device &device) noexcept
	{
		if ((m_phase != FrameOwnerPhase::Legacy && m_phase != FrameOwnerPhase::Modern) || m_device != &device || !device.End_Frame())
			return Reject();

		m_phase = FrameOwnerPhase::ReadyToPresent;
		return true;
	}

	bool Present(Device &device) noexcept
	{
		if (m_phase != FrameOwnerPhase::ReadyToPresent || m_device != &device || !device.Get_Swap_Chain().Present())
			return Reject();

		m_phase = FrameOwnerPhase::Idle;
		m_device = nullptr;
		m_targets = {};
		return true;
	}

	void Abort(Device &device) noexcept
	{
		if ((m_phase == FrameOwnerPhase::Legacy || m_phase == FrameOwnerPhase::Modern) && m_device == &device)
			device.End_Frame();

		m_phase = FrameOwnerPhase::Idle;
		m_device = nullptr;
		m_targets = {};
	}

	FrameOwnerPhase Phase() const noexcept
	{
		return m_phase;
	}

	FrameTargets Targets() const noexcept
	{
		return m_targets;
	}

	std::uint32_t Invalid_Operation_Count() const noexcept
	{
		return m_invalid_operation_count;
	}

private:
	bool Reject() noexcept
	{
		++m_invalid_operation_count;
		return false;
	}

	FrameOwnerPhase m_phase = FrameOwnerPhase::Idle;
	Device *m_device = nullptr;
	FrameTargets m_targets{};
	std::uint32_t m_invalid_operation_count = 0;
};

export using FrameHandoffPhase = FrameOwnerPhase;
export using FrameHandoff = FrameOwner;

}
