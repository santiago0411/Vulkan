#pragma once

#include "VulkanUtils.h"

class VulkanMesh
{
public:
	struct MeshCreateInfo
	{
		VkPhysicalDevice PhysicalDevice;
		VkDevice LogicalDevice;
		VkQueue TransferQueue;
		VkCommandPool TransferCommandPool;
		Utils::VertexData const* Vertices;
		uint32_t VerticesCount;
		uint32_t const* Indices;
		uint32_t IndicesCount;
	};

public:
	VulkanMesh() = default;
	VulkanMesh(const MeshCreateInfo& meshCreateInfo);
	void Destroy();

	uint32_t GetVertexCount() const { return m_VertexCount; }
	VkBuffer GetVertexBuffer() const { return m_VertexBuffer; }

	uint32_t GetIndicesCount() const { return m_IndexCount; }
	VkBuffer GetIndexBuffer() const { return m_IndexBuffer; }

private:
	void CreateVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, Utils::VertexData const* vertices, uint32_t verticesCount);
	void CreateIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, uint32_t const* indices, uint32_t indexCount);

private:
	uint32_t m_VertexCount = 0;
	VkBuffer m_VertexBuffer =  nullptr;
	VkDeviceMemory m_VertexBufferMemory = nullptr;

	uint32_t m_IndexCount = 0;
	VkBuffer m_IndexBuffer = nullptr;
	VkDeviceMemory m_IndexBufferMemory = nullptr;

	VkPhysicalDevice m_PhysicalDevice = nullptr;
	VkDevice m_Device = nullptr;
};
