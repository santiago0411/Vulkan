#pragma once

#include "Base.h"
#include "Window.h"

#pragma warning(push, 0)
#include <vulkan/vulkan.h>
#pragma warning(pop)

#include <vector>
#include <stdexcept>

#include <glm/glm.hpp>

namespace Utils
{
	static constexpr uint32_t MAX_FRAME_DRAWS = 2;

	static const std::vector<const char*> s_DeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	struct VertexData
	{
		glm::vec3 Position;
		glm::vec4 Color;
	};

	struct QueueFamilyIndices
	{
		int32_t GraphicsFamily = -1;
		int32_t PresentationFamily = -1;

		bool AreValid() const
		{
			return GraphicsFamily >= 0 && PresentationFamily >= 0;
		}
	};

	struct SwapChainDetails
	{
		VkSurfaceCapabilitiesKHR SurfaceCapabilities{};
		std::vector<VkSurfaceFormatKHR> Formats;
		std::vector<VkPresentModeKHR> PresentationModes;

		bool IsValid() const
		{
			return !PresentationModes.empty() && !Formats.empty();
		}
	};

	struct SwapChainImage
	{
		VkImage Image = nullptr;
		VkImageView ImageView = nullptr;
	};

	static bool CheckInstanceExtensionSupport(const std::vector<const char*>& extensions)
	{
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		if (extensionCount == 0)
			return false;

		std::vector<VkExtensionProperties> extensionsProps(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionsProps.data());

		for (const char* extension : extensions)
		{
			bool hasExtension = false;

			for (const auto& extProp : extensionsProps)
			{
				if (strcmp(extension, extProp.extensionName) == 0)
				{
					hasExtension = true;
					break;
				}
			}

			if (!hasExtension)
				return false;
		}

		return true;
	}

	static bool CheckDeviceExtensionSupport(VkPhysicalDevice device)
	{
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		if (extensionCount == 0)
			return false;

		std::vector<VkExtensionProperties> extensionsProps(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensionsProps.data());

		for (const char* extension : s_DeviceExtensions)
		{
			bool hasExtension = false;

			for (const auto& extProp : extensionsProps)
			{
				if (strcmp(extension, extProp.extensionName) == 0)
				{
					hasExtension = true;
					break;
				}
			}

			if (!hasExtension)
				return false;
		}

		return true;
	}

	static QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		for (int32_t i = 0; const auto & queueFamily : queueFamilies)
		{
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				indices.GraphicsFamily = i;

			VkBool32 presentationSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);

			if (queueFamily.queueCount > 0 && presentationSupport)
				indices.PresentationFamily = i;

			if (indices.AreValid())
				break;

			i++;
		}

		return indices;
	}

	static SwapChainDetails GetSwapChainDetails(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		SwapChainDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.SurfaceCapabilities);

		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if (formatCount != 0)
		{
			details.Formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.Formats.data());
		}

		uint32_t presentationCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, nullptr);

		if (presentationCount != 0)
		{
			details.PresentationModes.resize(presentationCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, details.PresentationModes.data());
		}

		return details;
	}

	static bool CheckDeviceIsSuitable(VkPhysicalDevice device, VkSurfaceKHR surface, QueueFamilyIndices& outIndices, SwapChainDetails& outSwapChainDetails)
	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(device, &props);

		VkPhysicalDeviceFeatures features;
		vkGetPhysicalDeviceFeatures(device, &features);

		outIndices = GetQueueFamilies(device, surface);
		if (!outIndices.AreValid())
			return false;

		if (!CheckDeviceExtensionSupport(device))
			return false;

		outSwapChainDetails = GetSwapChainDetails(device, surface);

		return outSwapChainDetails.IsValid();
	}

	static VkSurfaceFormatKHR ChooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
	{
		// This means that every format possible is supported
		if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
			return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR };

		for (const auto& [format, colorSpace] : formats)
		{
			if (format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_B8G8R8A8_UNORM)
				if (colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
					return { format, colorSpace };
		}

		return formats[0];
	}

	static VkPresentModeKHR ChooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes)
	{
		for (const auto& presentationMode : presentationModes)
			if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
				return presentationMode;

		// This format is always available according to Vulkan spec
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	static VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities, const Ref<Window>& window)
	{
		if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
			return surfaceCapabilities.currentExtent;

		int32_t width, height;
		window->GetWidthAndHeight(width, height);

		VkExtent2D newExtent{ (uint32_t)width, (uint32_t)height };

		newExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
		newExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));

		return newExtent;
	}

	static VkImageView CreateImageView(VkDevice logicalDevice, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
	{
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = image;
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = format;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		createInfo.subresourceRange.aspectMask = aspectFlags;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		VkImageView imageView;
		if (vkCreateImageView(logicalDevice, &createInfo, nullptr, &imageView) != VK_SUCCESS)
			throw std::runtime_error("Failed to create an ImageView!");

		return imageView;
	}

	static std::vector<SwapChainImage> GetSwapChainImages(VkDevice logicalDevice, VkSwapchainKHR swapChain, VkFormat imageFormat)
	{
		uint32_t swapChainImageCount;
		vkGetSwapchainImagesKHR(logicalDevice, swapChain, &swapChainImageCount, nullptr);

		std::vector<VkImage> images(swapChainImageCount);
		vkGetSwapchainImagesKHR(logicalDevice, swapChain, &swapChainImageCount, images.data());

		std::vector<SwapChainImage> swapChainImages;
		swapChainImages.reserve(images.size());

		for (const auto& image: images)
		{
			swapChainImages.emplace_back(SwapChainImage{
				image,
				CreateImageView(logicalDevice, image, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			});
		}

		return swapChainImages;
	}

	static void CreateShaderModule(const std::vector<uint32_t>& buffer, VkDevice logicalDevice, VkShaderModule& shaderModule)
	{
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = buffer.size();
		createInfo.pCode = buffer.data();

		if (vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
			throw std::runtime_error("Error creating shader module!");
	}

	struct CreateBufferInfo
	{
		VkPhysicalDevice PhysicalDevice;
		VkDevice LogicalDevice;
		VkDeviceSize BufferSize;
		VkBufferUsageFlags BufferUsage;
		VkMemoryPropertyFlags BufferProperties;
		VkBuffer* Buffer;
		VkDeviceMemory* BufferMemory;
	};

	static uint32_t FindMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags flags)
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			VkMemoryType& memType = memProperties.memoryTypes[i];
			if (allowedTypes & (1 << i) && (memType.propertyFlags & flags) == flags)
				return i;
		}

		return UINT32_MAX;
	}

	static void CreateBuffer(const CreateBufferInfo& createBufferInfo)
	{
		VkBufferCreateInfo bufferCreateInfo = {};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = createBufferInfo.BufferSize;
		bufferCreateInfo.usage = createBufferInfo.BufferUsage;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(createBufferInfo.LogicalDevice, &bufferCreateInfo, nullptr, createBufferInfo.Buffer) != VK_SUCCESS)
			throw std::runtime_error("Failed to create a Buffer");

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(createBufferInfo.LogicalDevice, *createBufferInfo.Buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryTypeIndex(createBufferInfo.PhysicalDevice, memRequirements.memoryTypeBits, createBufferInfo.BufferProperties);

		if (vkAllocateMemory(createBufferInfo.LogicalDevice, &allocInfo, nullptr, createBufferInfo.BufferMemory) != VK_SUCCESS)
			throw std::runtime_error("Failed to allocate Buffer Memory!");

		vkBindBufferMemory(createBufferInfo.LogicalDevice, *createBufferInfo.Buffer, *createBufferInfo.BufferMemory, 0);
	}

	struct CopyBufferInfo
	{
		VkDevice Device;
		VkQueue TransferQueue;
		VkCommandPool TransferCommandPool;
		VkBuffer SrcBuffer;
		VkBuffer DstBuffer;
		VkDeviceSize BufferSize;
	};

	static void CopyBuffer(const CopyBufferInfo& copyBufferInfo)
	{
		VkCommandBuffer transferCommandBuffer;

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = copyBufferInfo.TransferCommandPool;
		allocInfo.commandBufferCount = 1;

		if (vkAllocateCommandBuffers(copyBufferInfo.Device, &allocInfo, &transferCommandBuffer) != VK_SUCCESS)
			throw std::runtime_error("Failed to allocate a Command Buffer!");

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

		VkBufferCopy bufferCopyRegion = {
			bufferCopyRegion.srcOffset = 0,
			bufferCopyRegion.dstOffset = 0,
			bufferCopyRegion.size = copyBufferInfo.BufferSize
		};

		vkCmdCopyBuffer(transferCommandBuffer, copyBufferInfo.SrcBuffer, copyBufferInfo.DstBuffer, 1, &bufferCopyRegion);

		vkEndCommandBuffer(transferCommandBuffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &transferCommandBuffer;

		vkQueueSubmit(copyBufferInfo.TransferQueue, 1, &submitInfo, nullptr);
		vkQueueWaitIdle(copyBufferInfo.TransferQueue);

		vkFreeCommandBuffers(copyBufferInfo.Device, copyBufferInfo.TransferCommandPool, 1, &transferCommandBuffer);
	}
}