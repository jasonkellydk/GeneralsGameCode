/* Backend-neutral compiled shader program. */

#include "ProgrammableShader.h"

#include <fstream>
#include <limits>
#include <vector>

#include "WW3D2/WW3D.h"
#include "WWLib/WWFILE.h"
#include "WWLib/ffactory.h"

namespace
{
	bool Load_Shader_Binary(const char *path, std::vector<unsigned char> &binary)
	{
		binary.clear();
		if (path == nullptr)
		{
			return false;
		}

		if (_TheFileFactory != nullptr)
		{
			file_auto_ptr file(_TheFileFactory, path);
			if (file.get() != nullptr && file->Is_Available())
			{
				const int size = file->Size();
				if (size > 0)
				{
					binary.resize(static_cast<std::size_t>(size));
					if (file->Read(binary.data(), size) == size)
					{
						return true;
					}
					binary.clear();
				}
			}
		}

		std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
		if (!file.is_open())
		{
			return false;
		}
		const std::streamoff size = file.tellg();
		if (size <= 0 || size > static_cast<std::streamoff>(
			std::numeric_limits<int>::max()))
		{
			return false;
		}
		binary.resize(static_cast<std::size_t>(size));
		file.seekg(0, std::ios::beg);
		file.read(reinterpret_cast<char *>(binary.data()), size);
		if (!file)
		{
			binary.clear();
			return false;
		}
		return true;
	}
}

ShaderProgramClass::ShaderProgramClass() :
	m_backend(nullptr),
	m_vertex_shader(0),
	m_pixel_shader(0)
{
}

ShaderProgramClass::~ShaderProgramClass()
{
	Shutdown();
}

bool ShaderProgramClass::Initialize(const char *vertex_shader_path,
	const char *pixel_shader_path,
	const RenderBackendVertexShaderInputLayout &vertex_input_layout)
{
	Shutdown();

	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend == nullptr)
	{
		return false;
	}

	std::vector<unsigned char> vertex_binary;
	std::vector<unsigned char> pixel_binary;
	if (!Load_Shader_Binary(vertex_shader_path, vertex_binary) ||
		!Load_Shader_Binary(pixel_shader_path, pixel_binary))
	{
		return false;
	}

	uintptr_t vertex_shader = 0;
	if (!backend->Create_Vertex_Shader(vertex_binary.data(),
		static_cast<unsigned>(vertex_binary.size()), &vertex_shader,
		&vertex_input_layout))
	{
		return false;
	}

	uintptr_t pixel_shader = 0;
	if (!backend->Create_Pixel_Shader(pixel_binary.data(),
		static_cast<unsigned>(pixel_binary.size()), &pixel_shader))
	{
		backend->Release_Vertex_Shader(vertex_shader);
		return false;
	}

	m_backend = backend;
	m_vertex_shader = vertex_shader;
	m_pixel_shader = pixel_shader;
	m_vertex_input_layout = vertex_input_layout;
	return true;
}

void ShaderProgramClass::Shutdown()
{
	if (m_backend != nullptr)
	{
		if (m_vertex_shader != 0)
		{
			m_backend->Release_Vertex_Shader(m_vertex_shader);
		}
		if (m_pixel_shader != 0)
		{
			m_backend->Release_Pixel_Shader(m_pixel_shader);
		}
	}
	m_backend = nullptr;
	m_vertex_shader = 0;
	m_pixel_shader = 0;
}

bool ShaderProgramClass::Apply() const
{
	if (m_backend == nullptr || !Is_Initialized())
	{
		return false;
	}
	m_backend->Set_Vertex_Shader(m_vertex_shader, &m_vertex_input_layout);
	m_backend->Set_Pixel_Shader(m_pixel_shader);
	return true;
}

void ShaderProgramClass::Reset() const
{
	if (m_backend != nullptr)
	{
		m_backend->Set_Vertex_Shader(0);
		m_backend->Set_Pixel_Shader(0);
	}
}
