#include "VulkanShader.h"

#include <fstream>
#include <filesystem>
#include <iostream>

#include <stdexcept>

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

namespace Utils
{
	static VulkanShader::ShaderType ShaderTypeFromString(const std::string& type)
	{
		if (type == "vertex")
			return VulkanShader::ShaderType::Vertex;
		if (type == "fragment")
			return VulkanShader::ShaderType::Fragment;

		throw std::runtime_error("Invalid shader string type!");
	}

	static shaderc_shader_kind VulkanShaderToShaderC(VulkanShader::ShaderType type)
	{
		switch (type)
		{
			case VulkanShader::ShaderType::Vertex:
				return shaderc_glsl_vertex_shader;
			case VulkanShader::ShaderType::Fragment: 
				return shaderc_glsl_fragment_shader;
		}

		throw std::runtime_error("Invalid shader type to shaderc!");
	}

	static const char* GetCacheDirectory()
	{
		return "shaders/cache/vulkan";
	}

	static void CreateDirectoryIfNeeded()
	{
		const char* cacheDirectory = GetCacheDirectory();
		if (!std::filesystem::exists(cacheDirectory))
			std::filesystem::create_directories(cacheDirectory);
	}

	static const char* ShaderStageCachedVulkanFileExtension(VulkanShader::ShaderType stage)
	{
		switch (stage)
		{
			case VulkanShader::ShaderType::Vertex:
				return ".cached_vulkan.vert";
			case VulkanShader::ShaderType::Fragment:
				return ".cached_vulkan.frag";
		}

		throw std::runtime_error("Invalid shader type to extension!");
	}

	static std::string ReadFile(const std::string& filepath)
	{
		std::string result;
		std::ifstream in(filepath, std::ios::in | std::ios::binary);

		if (in)
		{
			in.seekg(0, std::ios::end);
			size_t size = in.tellg();

			if (size != -1)
			{
				result.resize(size);
				in.seekg(0, std::ios::beg);
				in.read(&result[0], size);
			}
			else
			{
				throw std::runtime_error("Could not read file '" + filepath + "'\n");
			}
		}
		else
		{
			throw std::runtime_error("Could not open shader file '" + filepath + "'\n");
		}

		return result;
	}
}

Ref<VulkanShader> VulkanShader::Create(const std::string& filepath)
{
	auto shader = std::shared_ptr<VulkanShader>();
	shader.reset(new VulkanShader(filepath));
	AddToLibrary(shader);
	return shader;
}

Ref<VulkanShader> VulkanShader::CreateFromSpv(const std::string& name, const std::string& vertexFilepath, const std::string& fragFilepath)
{
	auto shader = std::shared_ptr<VulkanShader>();
	shader.reset(new VulkanShader({ {ShaderType::Vertex, vertexFilepath}, {ShaderType::Fragment, fragFilepath} }));
	AddToLibrary(name, shader);
	return shader;
}

VulkanShader::VulkanShader(const std::string& filepath)
	: m_FilePath(filepath)
{
	Utils::CreateDirectoryIfNeeded();

	std::string source = Utils::ReadFile(filepath);
	auto shaderSources = PreProcess(source);
	CompileOrGetVulkanBinaries(shaderSources);

	auto lastSlash = filepath.find_last_of("/\\");
	lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
	auto lastDot = filepath.rfind('.');
	auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
	m_Name = filepath.substr(lastSlash, count);
}

VulkanShader::VulkanShader(const std::unordered_map<ShaderType, std::string> filepaths)
{
	for (auto&& [stage, path] : filepaths)
	{
		std::ifstream in(path, std::ios::binary | std::ios::ate);

		if (in)
		{
			size_t fileSize = in.tellg();
			std::vector<uint32_t> fileBuffer(fileSize);

			in.seekg(0);
			in.read(reinterpret_cast<char*>(fileBuffer.data()), fileSize);

			m_VulkanSPIRV[stage] = fileBuffer;
		}
		else
		{
			throw std::runtime_error("Failed to open a file!");
		}
	}
}

std::unordered_map<VulkanShader::ShaderType, std::string> VulkanShader::PreProcess(const std::string& source)
{
	std::unordered_map<ShaderType, std::string> shaderSources;

	const char* typeToken = "#type";
	const size_t typeTokenLength = strlen(typeToken);
	size_t pos = source.find(typeToken, 0); // Start of shader type declaration line
	while (pos != std::string::npos)
	{
		size_t eol = source.find_first_of("\r\n", pos); // End of shader type declaration line
		if (eol == std::string::npos)
		{
			std::cerr << "Syntax error\n";
			return {};
		}

		size_t begin = pos + typeTokenLength + 1; // Start of shader type name (after #type keyword)
		std::string type = source.substr(begin, eol - begin);

		size_t nextLinePos = source.find_first_not_of("\r\n", eol); // Start of shader code after shader type declaration line
		if (nextLinePos == std::string::npos)
		{
			std::cerr << "Syntax error\n";
			return {};
		}

		pos = source.find(typeToken, nextLinePos); // Start of next shader type declaration line

		shaderSources[Utils::ShaderTypeFromString(type)] = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
	}

	return shaderSources;
}

void VulkanShader::CompileOrGetVulkanBinaries(const std::unordered_map<ShaderType, std::string>& shaderSources)
{
	shaderc::Compiler compiler;
	shaderc::CompileOptions options;
	options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
	options.SetOptimizationLevel(shaderc_optimization_level_performance);

	std::filesystem::path cacheDirectory = Utils::GetCacheDirectory();

	auto& shaderData = m_VulkanSPIRV;
	shaderData.clear();

	for (auto&& [stage, source] : shaderSources)
	{
		std::filesystem::path shaderFilePath = m_FilePath;
		std::filesystem::path cachedPath = cacheDirectory / (shaderFilePath.filename().string() + Utils::ShaderStageCachedVulkanFileExtension(stage));

		std::ifstream in(cachedPath, std::ios::in | std::ios::binary);
		if (in.is_open())
		{
			in.seekg(0, std::ios::end);
			size_t size = in.tellg();
			in.seekg(0, std::ios::beg);

			auto& data = shaderData[stage];
			data.resize(size / sizeof(uint32_t));
			in.read((char*)data.data(), size);
		}
		else
		{
			shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source, Utils::VulkanShaderToShaderC(stage), m_FilePath.c_str(), options);
			if (result.GetCompilationStatus() != shaderc_compilation_status_success)
			{
				std::cerr << result.GetErrorMessage() << '\n';
			}

			shaderData[stage] = std::vector(result.cbegin(), result.cend());

			std::ofstream out(cachedPath, std::ios::out | std::ios::binary);
			if (out.is_open())
			{
				auto& data = shaderData[stage];
				out.write((char*)data.data(), data.size() * sizeof(uint32_t));
				out.flush();
				out.close();
			}
		}
	}
}

static std::unordered_map<std::string, Ref<VulkanShader>> s_Shaders;

void VulkanShader::AddToLibrary(const Ref<VulkanShader>& shader)
{
	AddToLibrary(shader->m_Name, shader);
}

void VulkanShader::AddToLibrary(const std::string& name, const Ref<VulkanShader>& shader)
{
	s_Shaders[name] = shader;
}

Ref<VulkanShader> VulkanShader::GetFromLibrary(const std::string& name)
{
	return s_Shaders[name];
}

bool VulkanShader::ExistsInLibrary(const std::string& name)
{
	return s_Shaders.find(name) != s_Shaders.end();
}