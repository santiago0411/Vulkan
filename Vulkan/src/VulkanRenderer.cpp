#include "VulkanRenderer.h"

#include "VulkanRendererUtils.h"

#pragma warning(push, 0)
#include <GLFW/glfw3.h>
#pragma warning(pop)

#include <iostream>
#include <stdexcept>
#include <vector>
#include <unordered_set>


struct RendererContext
{
	Ref<Window> Window;
	Utils::QueueFamilyIndices DeviceQueueFamilyIndices{};
	Utils::SwapChainDetails SwapChainDetails{};

	VkInstance VulkanInstance = nullptr;
	VkSurfaceKHR Surface = nullptr;
	VkPhysicalDevice PhysicalDevice = nullptr;
	VkDevice LogicalDevice = nullptr;
	VkQueue GraphicsQueue = nullptr;
	VkQueue PresentationQueue = nullptr;
};

static RendererContext* s_Context = nullptr;

#if defined(VULKAN_DEBUG)
	#include "VulkanDebug.h"

	static void CreateDebugCallback()
	{
		VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = Debug::DebugCallback;
		createInfo.pUserData = nullptr;

		if (Debug::CreateDebugUtilsMessengerEXT(s_Context->VulkanInstance, &createInfo, nullptr, &Debug::s_DebugMessenger) != VK_SUCCESS)
			throw std::runtime_error("Failed to set up debug messenger!");
	}

	static void DestroyDebugCallback()
	{
		Debug::DestroyDebugUtilsMessengerEXT(s_Context->VulkanInstance, Debug::s_DebugMessenger, nullptr);
	}
#else
	#define CreateDebugCallback()
	#define DestroyDebugCallback()
#endif

bool VulkanRenderer::Init(Ref<Window> windowContext)
{
	if (s_Context != nullptr)
		return true;

	try
	{
		s_Context = new RendererContext{ std::move(windowContext) };
		CreateInstance(ValidateExtensions());
		CreateDebugCallback();
		CreateSurface();
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
	DestroyDebugCallback();

	if (s_Context->Surface != nullptr)
		vkDestroySurfaceKHR(s_Context->VulkanInstance, s_Context->Surface, nullptr);

	if (s_Context->LogicalDevice != nullptr)
		vkDestroyDevice(s_Context->LogicalDevice, nullptr);

	if (s_Context->VulkanInstance != nullptr)
		vkDestroyInstance(s_Context->VulkanInstance, nullptr);

	delete s_Context;
}

std::vector<const char*> VulkanRenderer::ValidateExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

#if defined(VULKAN_DEBUG)
	if (!Debug::CheckValidationLayerSupport())
		throw std::runtime_error("Requested validation layers are not available!");

	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

	if (!Utils::CheckInstanceExtensionSupport(extensions))
		throw std::runtime_error("VkInstance does not support required extensions!");

	return extensions;
}

void VulkanRenderer::CreateInstance(const std::vector<const char*>& extensions)
{
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
	createInfo.enabledExtensionCount = (uint32_t)extensions.size();
	createInfo.ppEnabledExtensionNames = extensions.data();
#if defined(VULKAN_DEBUG)
	createInfo.enabledLayerCount = (uint32_t)Debug::s_ValidationLayers.size();
	createInfo.ppEnabledLayerNames = Debug::s_ValidationLayers.data();
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
		Utils::SwapChainDetails swapChainDetails;
		if (Utils::CheckDeviceIsSuitable(device, s_Context->Surface, indices, swapChainDetails))
		{
			s_Context->PhysicalDevice = device;
			s_Context->DeviceQueueFamilyIndices = indices;
			s_Context->SwapChainDetails = std::move(swapChainDetails);
			break;
		}
	}
}

void VulkanRenderer::CreateLogicalDevice()
{
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::unordered_set<int32_t> queueFamilyIndices {
		s_Context->DeviceQueueFamilyIndices.GraphicsFamily,
		s_Context->DeviceQueueFamilyIndices.PresentationFamily
	};

	for (int32_t queueFamilyIndex : queueFamilyIndices)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
		queueCreateInfo.queueCount = 1;
		constexpr float priority = 1.0f;
		queueCreateInfo.pQueuePriorities = &priority;

		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures = {};

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.enabledExtensionCount = (uint32_t)Utils::s_DeviceExtensions.size();
	deviceCreateInfo.ppEnabledExtensionNames = Utils::s_DeviceExtensions.data();
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	if (vkCreateDevice(s_Context->PhysicalDevice, &deviceCreateInfo, nullptr, &s_Context->LogicalDevice) != VK_SUCCESS)
		throw std::runtime_error("Failed to create a Vulkan Logical Device!");

	vkGetDeviceQueue(s_Context->LogicalDevice, s_Context->DeviceQueueFamilyIndices.GraphicsFamily, 0, &s_Context->GraphicsQueue);
	vkGetDeviceQueue(s_Context->LogicalDevice, s_Context->DeviceQueueFamilyIndices.PresentationFamily, 0, &s_Context->PresentationQueue);
}

void VulkanRenderer::CreateSurface()
{
	const auto result = glfwCreateWindowSurface(s_Context->VulkanInstance, 
											s_Context->Window->GetGLFWWindow(), 
											nullptr, &s_Context->Surface);

	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create a surface!");
}
