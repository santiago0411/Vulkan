#pragma once

#include "VulkanRendererUtils.h"

class VulkanMesh
{
public:
	VulkanMesh() = default;
	VulkanMesh(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, const std::vector<Utils::VertexData>& vertices);
	void Destroy();

	uint32_t GetVertexCount() const { return m_VertexCount; }
	VkBuffer GetVertexBuffer() const { return m_VertexBuffer; }

private:
	void CreateVertexBuffer(const std::vector<Utils::VertexData>& vertices);
	uint32_t FindMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags flags) const;

private:
	uint32_t m_VertexCount = 0;
	VkBuffer m_VertexBuffer =  nullptr;
	VkDeviceMemory m_VertexBufferMemory = nullptr;

	VkPhysicalDevice m_PhysicalDevice = nullptr;
	VkDevice m_Device = nullptr;
};

