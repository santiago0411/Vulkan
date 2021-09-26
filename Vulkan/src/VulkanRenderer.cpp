#include "VulkanRenderer.h"

#pragma warning(push, 0)
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#pragma warning(pop)

#include <iostream>
#include <stdexcept>
#include <vector>

namespace Utils
{
	struct QueueFamilyIndices
	{
		int32_t GraphicsFamily = -1;

		bool IsValid() const
		{
			return GraphicsFamily >= 0;
		}
	};

	static bool CheckInstanceExtensionSupport(const char** extensions, const uint32_t count)
	{
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> extensionsProps(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionsProps.data());

		for (uint32_t i = 0; i < count; i++)
		{
			const char* extension = extensions[i];
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

	QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device)
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

			if (indices.IsValid())
				break;

			i++;
		}

		return indices;
	}

	static bool CheckDeviceIsSuitable(VkPhysicalDevice device, QueueFamilyIndices& outIndices)
	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(device, &props);

		VkPhysicalDeviceFeatures features;
		vkGetPhysicalDeviceFeatures(device, &features);

		outIndices = GetQueueFamilies(device);
		return outIndices.IsValid();
	}

#if defined(VULKAN_DEBUG)
	std::vector<const char*> s_ValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	bool CheckValidationLayerSupport()
	{
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const auto* layerName : s_ValidationLayers)
		{
			bool layerFound = false;

			for (const auto& layerProps : availableLayers)
			{
				if (strcmp(layerName, layerProps.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
				return false;
		}

		return true;
	}
#endif
}

struct RendererContext
{
	WeakRef<Window> Window;
	Utils::QueueFamilyIndices DeviceQueueFamilyIndices{};

	VkInstance VulkanInstance = nullptr;
	VkPhysicalDevice PhysicalDevice = nullptr;
	VkDevice LogicalDevice = nullptr;
	VkQueue GraphicsQueue = nullptr;
};

static RendererContext* s_Context = nullptr;

bool VulkanRenderer::Init(WeakRef<Window> windowContext)
{
	if (s_Context != nullptr)
		return true;

	try
	{
		s_Context = new RendererContext{ std::move(windowContext) };
		CreateInstance();
		GetPhysicalDevice();
		CreateLogicalDevice();

		return true;
	}
	catch (const std::runtime_error& e)
	{
		std::cerr << "Error initializing vulkan context: " << e.what() << '\n';
		Shutdown();
		return false;
	}
}

void VulkanRenderer::Shutdown()
{
	if (s_Context->LogicalDevice != nullptr)
		vkDestroyDevice(s_Context->LogicalDevice, nullptr);

	if (s_Context->VulkanInstance != nullptr)
		vkDestroyInstance(s_Context->VulkanInstance, nullptr);

	delete s_Context;
}

void VulkanRenderer::CreateInstance()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	if (!Utils::CheckInstanceExtensionSupport(glfwExtensions, glfwExtensionCount))
		throw std::runtime_error("VkInstance does not support required extensions!");

	VkApplicationInfo appInfo;
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = nullptr;
	appInfo.pApplicationName = "Vulkan";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_2;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = glfwExtensionCount;
	createInfo.ppEnabledExtensionNames = glfwExtensions;

#if defined(VULKAN_DEBUG)
	if (!Utils::CheckValidationLayerSupport())
		throw std::runtime_error("Requested validation layers are not available!");
	createInfo.enabledLayerCount = (uint32_t)Utils::s_ValidationLayers.size();
	createInfo.ppEnabledLayerNames = Utils::s_ValidationLayers.data();
#else
	createInfo.enabledLayerCount = 0;
#endif

	if (vkCreateInstance(&createInfo, nullptr, &s_Context->VulkanInstance) != VK_SUCCESS)
		throw std::runtime_error("Failed to created a Vulkan Instance!");
}

void VulkanRenderer::GetPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(s_Context->VulkanInstance, &deviceCount, nullptr);

	if (deviceCount == 0)
		throw std::runtime_error("Couldn't find any GPUs that support Vulkan!");

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(s_Context->VulkanInstance, &deviceCount, devices.data());

	for (const auto& device : devices)
	{
		Utils::QueueFamilyIndices indices;
		if (Utils::CheckDeviceIsSuitable(device, indices))
		{
			s_Context->PhysicalDevice = device;
			s_Context->DeviceQueueFamilyIndices = indices;
			break;
		}
	}
}

void VulkanRenderer::CreateLogicalDevice()
{
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = s_Context->DeviceQueueFamilyIndices.GraphicsFamily;
	queueCreateInfo.queueCount = 1;
	constexpr float priority = 1.0f;
	queueCreateInfo.pQueuePriorities = &priority;

	VkPhysicalDeviceFeatures deviceFeatures = {};

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.enabledExtensionCount = 0;
	deviceCreateInfo.ppEnabledExtensionNames = nullptr;
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	if (vkCreateDevice(s_Context->PhysicalDevice, &deviceCreateInfo, nullptr, &s_Context->LogicalDevice) != VK_SUCCESS)
		throw std::runtime_error("Failed to create a Vulkan Logical Device!");

	vkGetDeviceQueue(s_Context->LogicalDevice, s_Context->DeviceQueueFamilyIndices.GraphicsFamily, 0, &s_Context->GraphicsQueue);
}
