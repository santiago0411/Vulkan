#include "VulkanMesh.h"

VulkanMesh::VulkanMesh(const MeshCreateInfo& meshCreateInfo)
	: m_VertexCount(meshCreateInfo.VerticesCount), m_IndexCount(meshCreateInfo.IndicesCount),
		m_PhysicalDevice(meshCreateInfo.PhysicalDevice), m_Device(meshCreateInfo.LogicalDevice)
{
	CreateVertexBuffer(meshCreateInfo.TransferQueue, meshCreateInfo.TransferCommandPool, meshCreateInfo.Vertices, meshCreateInfo.VerticesCount);
	CreateIndexBuffer(meshCreateInfo.TransferQueue, meshCreateInfo.TransferCommandPool, meshCreateInfo.Indices, meshCreateInfo.IndicesCount);
}

void VulkanMesh::Destroy()
{
	vkDestroyBuffer(m_Device, m_VertexBuffer, nullptr);
	m_VertexBuffer = nullptr;
	vkFreeMemory(m_Device, m_VertexBufferMemory, nullptr);

	vkDestroyBuffer(m_Device, m_IndexBuffer, nullptr);
	m_IndexBuffer = nullptr;
	vkFreeMemory(m_Device, m_IndexBufferMemory, nullptr);
}

void VulkanMesh::CreateVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, Utils::VertexData const* vertices, uint32_t verticesCount)
{
	const VkDeviceSize bufferSize = sizeof(Utils::VertexData) * verticesCount;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	Utils::CreateBufferInfo stagingBufferInfo = {
		stagingBufferInfo.PhysicalDevice = m_PhysicalDevice,
		stagingBufferInfo.LogicalDevice = m_Device,
		stagingBufferInfo.BufferSize = bufferSize,
		stagingBufferInfo.BufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		stagingBufferInfo.BufferProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBufferInfo.Buffer = &stagingBuffer,
		stagingBufferInfo.BufferMemory = &stagingBufferMemory
	};

	Utils::CreateBuffer(stagingBufferInfo);

	void* data;
	vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices, bufferSize);
	vkUnmapMemory(m_Device, stagingBufferMemory);

	Utils::CreateBufferInfo vertexBufferInfo = {
		vertexBufferInfo.PhysicalDevice = m_PhysicalDevice,
		vertexBufferInfo.LogicalDevice = m_Device,
		vertexBufferInfo.BufferSize = bufferSize,
		vertexBufferInfo.BufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		vertexBufferInfo.BufferProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBufferInfo.Buffer = &m_VertexBuffer,
		vertexBufferInfo.BufferMemory = &m_VertexBufferMemory
	};

	Utils::CreateBuffer(vertexBufferInfo);

	Utils::CopyBufferInfo copyBufferInfo = {
		copyBufferInfo.Device = m_Device,
		copyBufferInfo.TransferQueue = transferQueue,
		copyBufferInfo.TransferCommandPool = transferCommandPool,
		copyBufferInfo.SrcBuffer = stagingBuffer,
		copyBufferInfo.DstBuffer = m_VertexBuffer,
		copyBufferInfo.BufferSize = bufferSize
	};

	Utils::CopyBuffer(copyBufferInfo);

	vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
	vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
}

void VulkanMesh::CreateIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, uint32_t const* indices, uint32_t indexCount)
{
	const VkDeviceSize bufferSize = sizeof(uint32_t) * indexCount;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	Utils::CreateBufferInfo stagingBufferInfo = {
		stagingBufferInfo.PhysicalDevice = m_PhysicalDevice,
		stagingBufferInfo.LogicalDevice = m_Device,
		stagingBufferInfo.BufferSize = bufferSize,
		stagingBufferInfo.BufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		stagingBufferInfo.BufferProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBufferInfo.Buffer = &stagingBuffer,
		stagingBufferInfo.BufferMemory = &stagingBufferMemory
	};

	Utils::CreateBuffer(stagingBufferInfo);

	void* data;
	vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices, bufferSize);
	vkUnmapMemory(m_Device, stagingBufferMemory);

	Utils::CreateBufferInfo indexBufferInfo = {
		indexBufferInfo.PhysicalDevice = m_PhysicalDevice,
		indexBufferInfo.LogicalDevice = m_Device,
		indexBufferInfo.BufferSize = bufferSize,
		indexBufferInfo.BufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		indexBufferInfo.BufferProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBufferInfo.Buffer = &m_IndexBuffer,
		indexBufferInfo.BufferMemory = &m_IndexBufferMemory
	};

	Utils::CreateBuffer(indexBufferInfo);

	Utils::CopyBufferInfo copyBufferInfo = {
		copyBufferInfo.Device = m_Device,
		copyBufferInfo.TransferQueue = transferQueue,
		copyBufferInfo.TransferCommandPool = transferCommandPool,
		copyBufferInfo.SrcBuffer = stagingBuffer,
		copyBufferInfo.DstBuffer = m_IndexBuffer,
		copyBufferInfo.BufferSize = bufferSize
	};

	Utils::CopyBuffer(copyBufferInfo);

	vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
	vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
}
