#pragma once

#include "Base.h"

#include <string>
#include <unordered_map>

class VulkanShader
{
public:
	enum class ShaderType { Vertex, Fragment };

public:
	VulkanShader() = delete;
	VulkanShader(const VulkanShader&) = delete;

	const std::vector<uint32_t>& GetShaderBinary(ShaderType type) { return m_VulkanSPIRV[type]; }

	static Ref<VulkanShader> Create(const std::string& filepath);
	static Ref<VulkanShader> CreateFromSpv(const std::string& name, const std::string& vertexFilepath, const std::string& fragFilepath);

private:
	VulkanShader(const std::string& filepath);
	VulkanShader(const std::unordered_map<ShaderType, std::string> filepaths);

	void CompileOrGetVulkanBinaries(const std::unordered_map<ShaderType, std::string>& shaderSources);
	static std::unordered_map<ShaderType, std::string> PreProcess(const std::string& source);

private:
	std::string m_Name;
	std::string m_FilePath;

	std::unordered_map<ShaderType, std::vector<uint32_t>> m_VulkanSPIRV;


public:
	static Ref<VulkanShader> GetFromLibrary(const std::string& name);
	static bool ExistsInLibrary(const std::string& name);

private:
	static void AddToLibrary(const Ref<VulkanShader>& shader);
	static void AddToLibrary(const std::string& name, const Ref<VulkanShader>& shader);
};

