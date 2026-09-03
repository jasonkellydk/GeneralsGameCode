module;

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

export module Graphics.Shaders.Library;

export import Graphics.Resources.Materials.Material;
export import Graphics.Resources.Pools.ResourcePool;
export import Graphics.Shaders.Pipeline;

namespace Graphics
{

export enum class ShaderStage : std::uint8_t
{
	Vertex,
	Pixel,
	Compute
};

export enum class ShaderStageMask : std::uint8_t
{
	None = 0,
	Vertex = 1u << 0,
	Pixel = 1u << 1,
	Compute = 1u << 2
};

export constexpr ShaderStageMask operator|(ShaderStageMask left, ShaderStageMask right) noexcept
{
	return static_cast<ShaderStageMask>(static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
}

export constexpr bool Has_Shader_Stage(ShaderStageMask stages, ShaderStageMask stage) noexcept
{
	return (static_cast<std::uint8_t>(stages) & static_cast<std::uint8_t>(stage)) != 0;
}

export struct ShaderProgramDesc final
{
	std::uint16_t vertex_shader = 0;
	std::uint16_t fragment_shader = 0;
	std::uint16_t compute_shader = 0;
	ShaderStageMask stages = ShaderStageMask::None;
	ShaderInterfaceLayout interface_layout{};
	std::uint64_t source_key = 0;

	constexpr bool Has_Stage(ShaderStage stage) const noexcept
	{
		switch (stage) {
		case ShaderStage::Vertex:
			return Has_Shader_Stage(stages, ShaderStageMask::Vertex);
		case ShaderStage::Pixel:
			return Has_Shader_Stage(stages, ShaderStageMask::Pixel);
		case ShaderStage::Compute:
			return Has_Shader_Stage(stages, ShaderStageMask::Compute);
		}

		return false;
	}
};

export struct ShaderPrecompiledDesc final
{
	ShaderProgramDesc program{};
	std::filesystem::path vertex_path;
	std::filesystem::path fragment_path;
	std::filesystem::path compute_path;
};

export struct ShaderProgram final
{
	ShaderProgramDesc description{};
	std::vector<std::byte> vertex_bytecode;
	std::vector<std::byte> fragment_bytecode;
	std::vector<std::byte> compute_bytecode;

	bool Is_Graphics_Program() const noexcept
	{
		return description.Has_Stage(ShaderStage::Vertex) && description.Has_Stage(ShaderStage::Pixel) && !vertex_bytecode.empty() && !fragment_bytecode.empty();
	}
};

static_assert(std::is_nothrow_move_constructible_v<ShaderProgram>);
static_assert(std::is_nothrow_move_assignable_v<ShaderProgram>);

export ShaderInterfaceLayout Make_Basic_Opaque_Interface() noexcept
{
	ShaderInterfaceLayout layout;
	for (std::size_t index = 0; index < 4; ++index)
		layout.material.Add_Constant(ShaderValueType::Float4);
	return layout;
}

export ShaderProgramDesc Make_Basic_Opaque_Description() noexcept
{
	ShaderProgramDesc description;
	description.vertex_shader = 1;
	description.fragment_shader = 1;
	description.stages = ShaderStageMask::Vertex | ShaderStageMask::Pixel;
	description.interface_layout = Make_Basic_Opaque_Interface();
	description.source_key = 0x42415349434f5041ull;
	return description;
}

export PipelineDesc Make_Basic_Opaque_Pipeline() noexcept
{
	const ShaderProgramDesc shader = Make_Basic_Opaque_Description();
	PipelineDesc description;
	description.vertex_shader = shader.vertex_shader;
	description.fragment_shader = shader.fragment_shader;
	description.Set_Parameter_Layout(shader.interface_layout);
	return description;
}

export class ShaderLibrary final
{
public:
	ShaderHandle Load_Precompiled(const ShaderPrecompiledDesc &description)
	{
		if (!Is_Valid_Graphics_Description(description) || Find_By_Source_Key(description.program.source_key) != nullptr)
			return {};

		std::vector<std::byte> vertex_bytecode;
		std::vector<std::byte> fragment_bytecode;
		if (!Load_Binary(description.vertex_path, vertex_bytecode) || !Load_Binary(description.fragment_path, fragment_bytecode))
			return {};

		ShaderProgram program;
		program.description = description.program;
		program.vertex_bytecode = std::move(vertex_bytecode);
		program.fragment_bytecode = std::move(fragment_bytecode);
		return m_programs.Create(std::move(program));
	}

	ShaderHandle Load_Basic_Opaque(const std::filesystem::path &directory)
	{
		if (m_basic_opaque.Is_Valid())
			return m_basic_opaque;

		ShaderPrecompiledDesc description;
		description.program = Make_Basic_Opaque_Description();
		description.vertex_path = directory / "basic_opaque.vso";
		description.fragment_path = directory / "basic_opaque.pso";
		m_basic_opaque = Load_Precompiled(description);
		return m_basic_opaque;
	}

	bool Destroy(ShaderHandle handle) noexcept
	{
		if (handle == m_basic_opaque)
			m_basic_opaque = {};
		return m_programs.Destroy(handle);
	}

	bool Is_Loaded(ShaderHandle handle) const noexcept
	{
		return m_programs.Resolve(handle) != nullptr;
	}

	ShaderHandle Basic_Opaque() const noexcept
	{
		return m_basic_opaque;
	}

	ShaderHandle Select_Shader(const Material &material, ShaderHandle default_shader) const noexcept
	{
		if (Is_Loaded(material.shader))
			return material.shader;

		return Is_Loaded(default_shader) ? default_shader : ShaderHandle{};
	}

	ShaderProgramDesc Description(ShaderHandle handle) const noexcept
	{
		const ShaderProgram *program = m_programs.Resolve(handle);
		return program != nullptr ? program->description : ShaderProgramDesc{};
	}

	ShaderInterfaceLayout Interface(ShaderHandle handle) const noexcept
	{
		return Description(handle).interface_layout;
	}

	PipelineDesc Make_Pipeline_Description(ShaderHandle shader, PipelineDesc state) const noexcept
	{
		const ShaderProgramDesc program = Description(shader);
		if (!Is_Loaded(shader) || !program.Has_Stage(ShaderStage::Vertex) || !program.Has_Stage(ShaderStage::Pixel))
			return {};

		state.vertex_shader = program.vertex_shader;
		state.fragment_shader = program.fragment_shader;
		state.Set_Parameter_Layout(program.interface_layout);
		return state;
	}

	PipelineHandle Create_Pipeline(Device &device, ShaderHandle shader, const PipelineDesc &description) const
	{
		const ShaderProgram *program = m_programs.Resolve(shader);
		if (program == nullptr || !program->Is_Graphics_Program() || description.vertex_shader != program->description.vertex_shader || description.fragment_shader != program->description.fragment_shader || description.parameter_layout_key != program->description.interface_layout.Key())
			return {};

		const PipelineKey key = description.Key();
		const RHIPipeline rhi_description{
			key.value,
			description.depth_test,
			description.depth_write,
			description.topology,
			description.vertex_format,
			description.blend_mode
		};
		return device.Create_Pipeline(rhi_description, {program->vertex_bytecode}, {program->fragment_bytecode});
	}

	PipelineHandle Create_Pipeline(Device &device, const Material &material, ShaderHandle default_shader, PipelineDesc state) const
	{
		const ShaderHandle shader = Select_Shader(material, default_shader);
		const PipelineDesc description = Make_Pipeline_Description(shader, state);
		return Create_Pipeline(device, shader, description);
	}

	std::size_t Size() const noexcept
	{
		return m_programs.Size();
	}

private:
	static bool Is_Valid_Graphics_Description(const ShaderPrecompiledDesc &description) noexcept
	{
		const ShaderProgramDesc &program = description.program;
		return program.Has_Stage(ShaderStage::Vertex) && program.Has_Stage(ShaderStage::Pixel) && program.vertex_shader != 0 && program.fragment_shader != 0 && program.source_key != 0 && !description.vertex_path.empty() && !description.fragment_path.empty();
	}

	static bool Load_Binary(const std::filesystem::path &path, std::vector<std::byte> &data)
	{
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file)
			return false;

		const std::streampos end = file.tellg();
		if (end <= 0)
			return false;

		std::vector<std::byte> loaded(static_cast<std::size_t>(end));
		file.seekg(0, std::ios::beg);
		file.read(reinterpret_cast<char *>(loaded.data()), static_cast<std::streamsize>(loaded.size()));
		if (!file.good() && !file.eof())
			return false;

		data = std::move(loaded);
		return true;
	}

	const ShaderProgram *Find_By_Source_Key(std::uint64_t source_key) const noexcept
	{
		if (source_key == 0)
			return nullptr;

		const ShaderProgram *found = nullptr;
		m_programs.For_Each([&found, source_key](ShaderHandle, const ShaderProgram &program) noexcept {
			if (program.description.source_key == source_key)
				found = &program;
		});
		return found;
	}

	ResourcePool<ShaderProgram, ShaderHandle> m_programs;
	ShaderHandle m_basic_opaque{};
};

}
