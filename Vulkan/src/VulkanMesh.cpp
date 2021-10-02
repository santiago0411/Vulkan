#include "VulkanMesh.h"

VulkanMesh::VulkanMesh(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, const std::vector<Utils::VertexData>& vertices)
	: m_VertexCount((uint32_t)vertices.size()), m_PhysicalDevice(physicalDevice), m_Device(logicalDevice)
{
	CreateVertexBuffer(vertices);
}

void VulkanMesh::Destroy()
{
	vkDestroyBuffer(m_Device, m_VertexBuffer, nullptr);
	m_VertexBuffer = nullptr;
	vkFreeMemory(m_Device, m_VertexBufferMemory, nullptr);
}

void VulkanMesh::CreateVertexBuffer(const std::vector<Utils::VertexData>& vertices)
{
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = sizeof(Utils::VertexData) * vertices.size();
	bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(m_Device, &bufferCreateInfo, nullptr, &m_VertexBuffer) != VK_SUCCESS)
	{
		m_VertexBuffer = nullptr;
		return;
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(m_Device, m_VertexBuffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryTypeIndex(memRequirements.memoryTypeBits, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |	// CPU can interact with memory
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);	// Allows placement of data straight into buffer after mapping

	if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &m_VertexBufferMemory) != VK_SUCCESS)
	{
		Destroy();
		return;
	}

	vkBindBufferMemory(m_Device, m_VertexBuffer, m_VertexBufferMemory, 0);

	void* data;
	vkMapMemory(m_Device, m_VertexBufferMemory, 0, bufferCreateInfo.size, 0, &data);
	memcpy(data, vertices.data(), bufferCreateInfo.size);
	vkUnmapMemory(m_Device, m_VertexBufferMemory);
}

uint32_t VulkanMesh::FindMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags flags) const
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		VkMemoryType memType = memProperties.memoryTypes[i];
		if (allowedTypes & (1 << i) && (memType.propertyFlags & flags) == flags)
			return i;
	}

	return UINT32_MAX;
}
