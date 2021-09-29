#include "VulkanRenderer.h"

#include "VulkanRendererUtils.h"
#include "VulkanShader.h"

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
	VkSwapchainKHR SwapChain = nullptr;
	VkPipelineLayout PipelineLayout = nullptr;
	VkRenderPass RenderPass = nullptr;
	VkPipeline GraphicsPipeline = nullptr;

	VkFormat SwapChainImageFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D SwapChainExtent{};
	std::vector<Utils::SwapChainImage> SwapChainImages{};
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
		CreateSwapChain();
		CreateRenderPass();
		CreateGraphicsPipeline();

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

	if (s_Context->GraphicsPipeline != nullptr)
		vkDestroyPipeline(s_Context->LogicalDevice, s_Context->GraphicsPipeline, nullptr);

	if (s_Context->PipelineLayout != nullptr)
		vkDestroyPipelineLayout(s_Context->LogicalDevice, s_Context->PipelineLayout, nullptr);

	if (s_Context->RenderPass != nullptr)
		vkDestroyRenderPass(s_Context->LogicalDevice, s_Context->RenderPass, nullptr);

	for (auto& image : s_Context->SwapChainImages)
		vkDestroyImageView(s_Context->LogicalDevice, image.ImageView, nullptr);

	if (s_Context->SwapChain != nullptr)
		vkDestroySwapchainKHR(s_Context->LogicalDevice, s_Context->SwapChain, nullptr);

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

	extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
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


void VulkanRenderer::CreateSurface()
{
	const auto result = glfwCreateWindowSurface(s_Context->VulkanInstance,
		s_Context->Window->GetGLFWWindow(),
		nullptr, &s_Context->Surface);

	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create a surface!");
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

void VulkanRenderer::CreateSwapChain()
{
	VkSurfaceFormatKHR surfaceFormat = Utils::ChooseBestSurfaceFormat(s_Context->SwapChainDetails.Formats);
	VkPresentModeKHR presentMode = Utils::ChooseBestPresentationMode(s_Context->SwapChainDetails.PresentationModes);
	VkExtent2D extent = Utils::ChooseSwapExtent(s_Context->SwapChainDetails.SurfaceCapabilities, s_Context->Window);

	// Get 1 more image than the minimum to allow triple buffering
	uint32_t imageCount = s_Context->SwapChainDetails.SurfaceCapabilities.minImageCount + 1;
	const uint32_t maxImageCount = s_Context->SwapChainDetails.SurfaceCapabilities.maxImageCount;

	// MaxImageCount can be 0 if it's limitless so check that it is greater than 0
	// Clamp image count to max value if it overflowed adding 1
	if (maxImageCount > 0 && maxImageCount < imageCount)
		imageCount = maxImageCount;

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = s_Context->Surface;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.presentMode = presentMode;
	createInfo.imageExtent = extent;
	createInfo.minImageCount = imageCount;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.preTransform = s_Context->SwapChainDetails.SurfaceCapabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.clipped = VK_TRUE;

	Utils::QueueFamilyIndices& indices = s_Context->DeviceQueueFamilyIndices;
	if (indices.GraphicsFamily != indices.PresentationFamily)
	{
		const uint32_t queueFamilyIndices[] = {
			(uint32_t)indices.GraphicsFamily,
			(uint32_t)indices.PresentationFamily
		};

		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}

	createInfo.oldSwapchain = nullptr;

	if (vkCreateSwapchainKHR(s_Context->LogicalDevice, &createInfo, nullptr, &s_Context->SwapChain) != VK_SUCCESS)
		throw std::runtime_error("Failed to create a Swapchain!");

	s_Context->SwapChainImageFormat = surfaceFormat.format;
	s_Context->SwapChainExtent = extent;
	s_Context->SwapChainImages = Utils::GetSwapChainImages(s_Context->LogicalDevice, s_Context->SwapChain, surfaceFormat.format);
}

void VulkanRenderer::CreateRenderPass()
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = s_Context->SwapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	// Layout of how the image data will be stored in the framebuffer
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;		// Starting layout before the render pass starts
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;	// Final layout after all the sub-passes are done

	VkAttachmentReference colorAttachmentReference;
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Layout used during the sub-passes

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;

	// Need to determine when layout transitions occur using subpass dependencies
	static constexpr uint32_t SUBPASS_DEPENDENCIES_COUNT = 2;
	VkSubpassDependency subpassDependencies[SUBPASS_DEPENDENCIES_COUNT];

	// Conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;	// Which stage of the pipeline has to happen before transitioning onto the next subpass
	subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;

	subpassDependencies[0].dstSubpass = 0;	// First subpass in VkRenderPassCreateInfo.pSubpasses
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	subpassDependencies[0].dependencyFlags = 0;

	// Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	subpassDependencies[1].srcSubpass = 0;
	subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

	subpassDependencies[1].dependencyFlags = 0;

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &colorAttachment;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = SUBPASS_DEPENDENCIES_COUNT;
	renderPassCreateInfo.pDependencies = subpassDependencies;

	if (vkCreateRenderPass(s_Context->LogicalDevice, &renderPassCreateInfo, nullptr, &s_Context->RenderPass) != VK_SUCCESS)
		throw std::runtime_error("Failed to create a Render Pass!");
}

void VulkanRenderer::CreateGraphicsPipeline()
{
	const auto shader = VulkanShader::CreateFromSpv("Shader", "shaders/cache/vert.spv", "shaders/cache/frag.spv");

	VkShaderModule vertexShaderModule;
	Utils::CreateShaderModule(shader->GetShaderBinary(VulkanShader::ShaderType::Vertex), s_Context->LogicalDevice, vertexShaderModule);

	VkShaderModule fragShaderModule;
	Utils::CreateShaderModule(shader->GetShaderBinary(VulkanShader::ShaderType::Fragment), s_Context->LogicalDevice, fragShaderModule);

	VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
	vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderCreateInfo.module = vertexShaderModule;
	vertexShaderCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
	fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderCreateInfo.module = fragShaderModule;
	fragmentShaderCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)s_Context->SwapChainExtent.width;
	viewport.height = (float)s_Context->SwapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = s_Context->SwapChainExtent;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
	rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerCreateInfo.lineWidth = 1.0f;
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = {};
	multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState blendAttachmentState = {};
	blendAttachmentState.blendEnable = VK_TRUE;
	blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	// Blending equation: (srcColorBlendFactor * newColor) colorBlendOp (dstColorBlendFaction * oldColor)
	blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	// (VK_BLEND_FACTOR_SRC_ALPHA * newColor) + (VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA * oldColor)
	// (newColorAlpha * newColor) + ((1- newColorAlpha) * oldColor)

	blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	// (1 * newAlpha) + (0 * oldAlpha) = newAlpha

	VkPipelineColorBlendStateCreateInfo blendingCreateInfo = {};
	blendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendingCreateInfo.logicOpEnable = VK_FALSE;
	blendingCreateInfo.attachmentCount = 1;
	blendingCreateInfo.pAttachments = &blendAttachmentState;

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 0;
	pipelineLayoutCreateInfo.pSetLayouts = nullptr;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(s_Context->LogicalDevice, &pipelineLayoutCreateInfo, nullptr, &s_Context->PipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("Failed to create Pipeline Layout!");

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStages;
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pDynamicState = nullptr;
	pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
	pipelineCreateInfo.pColorBlendState = &blendingCreateInfo;
	pipelineCreateInfo.pDepthStencilState = nullptr;
	pipelineCreateInfo.layout = s_Context->PipelineLayout;
	pipelineCreateInfo.renderPass = s_Context->RenderPass;
	pipelineCreateInfo.subpass = 0;

	pipelineCreateInfo.basePipelineHandle = nullptr;
	pipelineCreateInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(s_Context->LogicalDevice, nullptr, 1, &pipelineCreateInfo, nullptr, &s_Context->GraphicsPipeline) != VK_SUCCESS)
		throw std::runtime_error("Failed to create a Graphics Pipeline");

	vkDestroyShaderModule(s_Context->LogicalDevice, fragShaderModule, nullptr);
	vkDestroyShaderModule(s_Context->LogicalDevice, vertexShaderModule, nullptr);
}
