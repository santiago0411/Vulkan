#pragma once

#pragma warning(push, 0)
#include <vulkan/vulkan.h>
#pragma warning(pop)

#include <vector>

namespace Utils
{
	static const std::vector<const char*> s_DeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
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
}