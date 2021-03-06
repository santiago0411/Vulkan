#include "VulkanRenderer.h"

#include "VulkanUtils.h"
#include "VulkanShader.h"
#include "VulkanMesh.h"

#pragma warning(push, 0)
#include <GLFW/glfw3.h>
#pragma warning(pop)

#include <iostream>
#include <stdexcept>
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
	VkCommandPool GraphicsCommandPool = nullptr;

	VkFormat SwapChainImageFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D SwapChainExtent{};
	std::vector<Utils::SwapChainImage> SwapChainImages{};
	std::vector<VkFramebuffer> SwapChainFramebuffers{};
	std::vector<VkCommandBuffer> CommandBuffers{};

	VkSemaphore ImageAvailableSemaphores[Utils::MAX_FRAME_DRAWS]{};
	VkSemaphore RenderFinishedSemaphores[Utils::MAX_FRAME_DRAWS]{};
	VkFence DrawFences[Utils::MAX_FRAME_DRAWS]{};
};

static RendererContext* s_Context = nullptr;
static uint32_t s_CurrentFrame = 0;
static std::vector<VulkanMesh> s_Meshes;

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
		CreateFramebuffers();
		CreateCommandPool();

		constexpr Utils::VertexData meshVertices[] = {
			{ { -0.1f, -0.4f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ { -0.1f,  0.4f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.9f,  0.4f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
			{ { -0.9f, -0.4f, 0.0f }, { 1.0f, 1.0f, 0.0f, 1.0f } },
		};

		constexpr Utils::VertexData meshVertices2[] = {
			{ {  0.9f, -0.3f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ {  0.9f,  0.3f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ {  0.1f,  0.3f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
			{ {  0.1f, -0.3f, 0.0f }, { 1.0f, 1.0f, 0.0f, 1.0f } },
		};

		constexpr uint32_t meshIndices[] = {
			0, 1, 2,
			2, 3, 0
		};

		VulkanMesh::MeshCreateInfo meshCreateInfo = {
			meshCreateInfo.PhysicalDevice = s_Context->PhysicalDevice,
			meshCreateInfo.LogicalDevice = s_Context->LogicalDevice,
			meshCreateInfo.TransferQueue = s_Context->GraphicsQueue,
			meshCreateInfo.TransferCommandPool = s_Context->GraphicsCommandPool,
			meshCreateInfo.Vertices = meshVertices,
			meshCreateInfo.VerticesCount = (uint32_t)std::size(meshVertices),
			meshCreateInfo.Indices = meshIndices,
			meshCreateInfo.IndicesCount = (uint32_t)std::size(meshIndices)
		};

		VulkanMesh::MeshCreateInfo meshCreateInfo2 = meshCreateInfo;
		meshCreateInfo2.Vertices = meshVertices2;
		meshCreateInfo2.VerticesCount = (uint32_t)std::size(meshVertices2);

		s_Meshes.emplace_back(VulkanMesh(meshCreateInfo));
		s_Meshes.emplace_back(VulkanMesh(meshCreateInfo2));

		CreateCommandBuffers();
		RecordCommands();
		CreateSynchronization();

		return true;
	}
	catch (const std::runtime_error& e)
	{
		std::cerr << "Error initializing vulkan context: " << e.what() << '\n';
		Shutdown();
		return false;
	}
}

void VulkanRenderer::Draw()
{
	const auto& imageAvailableSemaphore = s_Context->ImageAvailableSemaphores[s_CurrentFrame];
	const auto& renderFinishedSemaphore = s_Context->RenderFinishedSemaphores[s_CurrentFrame];
	const auto& drawFence = s_Context->DrawFences[s_CurrentFrame];

	vkWaitForFences(s_Context->LogicalDevice, 1, &drawFence, VK_TRUE, UINT64_MAX);
	vkResetFences(s_Context->LogicalDevice, 1, &drawFence);

	uint32_t nextImageIndex = 0;
	vkAcquireNextImageKHR(s_Context->LogicalDevice, s_Context->SwapChain, UINT64_MAX, imageAvailableSemaphore, nullptr, &nextImageIndex);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailableSemaphore;

	constexpr VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};

	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &s_Context->CommandBuffers[nextImageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

	if (vkQueueSubmit(s_Context->GraphicsQueue, 1, &submitInfo, drawFence) != VK_SUCCESS)
		throw std::runtime_error("Failed to submit Command Buffer to Queue!");

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &s_Context->SwapChain;
	presentInfo.pImageIndices = &nextImageIndex;

	if (vkQueuePresentKHR(s_Context->GraphicsQueue, &presentInfo) != VK_SUCCESS)
		throw std::runtime_error("Failed to present Image!");

	s_CurrentFrame = (s_CurrentFrame + 1) % Utils::MAX_FRAME_DRAWS;
}

void VulkanRenderer::Shutdown()
{
	vkDeviceWaitIdle(s_Context->LogicalDevice);

	for (auto& mesh : s_Meshes)
		mesh.Destroy();

	for (uint32_t i = 0; i < Utils::MAX_FRAME_DRAWS; i++)
	{
		vkDestroySemaphore(s_Context->LogicalDevice, s_Context->RenderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(s_Context->LogicalDevice, s_Context->ImageAvailableSemaphores[i], nullptr);
		vkDestroyFence(s_Context->LogicalDevice, s_Context->DrawFences[i], nullptr);
	}

	vkDestroyCommandPool(s_Context->LogicalDevice, s_Context->GraphicsCommandPool, nullptr);

	for (auto& framebuffer : s_Context->SwapChainFramebuffers)
		vkDestroyFramebuffer(s_Context->LogicalDevice, framebuffer, nullptr);

	vkDestroyPipeline(s_Context->LogicalDevice, s_Context->GraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(s_Context->LogicalDevice, s_Context->PipelineLayout, nullptr);
	vkDestroyRenderPass(s_Context->LogicalDevice, s_Context->RenderPass, nullptr);

	for (auto& image : s_Context->SwapChainImages)
		vkDestroyImageView(s_Context->LogicalDevice, image.ImageView, nullptr);

	vkDestroySwapchainKHR(s_Context->LogicalDevice, s_Context->SwapChain, nullptr);
	vkDestroySurfaceKHR(s_Context->VulkanInstance, s_Context->Surface, nullptr);
	vkDestroyDevice(s_Context->LogicalDevice, nullptr);
	DestroyDebugCallback();
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

	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(Utils::VertexData);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attributeDescriptions[2];

	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = offsetof(Utils::VertexData, Position);

	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(Utils::VertexData, Color);

	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = (uint32_t)std::size(attributeDescriptions);
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions;

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

void VulkanRenderer::CreateFramebuffers()
{
	s_Context->SwapChainFramebuffers.resize(s_Context->SwapChainImages.size());

	for (size_t i = 0; i < s_Context->SwapChainImages.size(); i++)
	{
		const VkImageView attachments[] = {
			s_Context->SwapChainImages[i].ImageView
		};

		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = s_Context->RenderPass;
		framebufferCreateInfo.attachmentCount = (uint32_t)std::size(attachments);
		framebufferCreateInfo.pAttachments = attachments;
		framebufferCreateInfo.width = s_Context->SwapChainExtent.width;
		framebufferCreateInfo.height = s_Context->SwapChainExtent.height;
		framebufferCreateInfo.layers = 1;

		if (vkCreateFramebuffer(s_Context->LogicalDevice, &framebufferCreateInfo, nullptr, &s_Context->SwapChainFramebuffers[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create a Framebuffer!");
	}
	
}

void VulkanRenderer::CreateCommandPool()
{
	VkCommandPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.queueFamilyIndex = (uint32_t)s_Context->DeviceQueueFamilyIndices.GraphicsFamily;

	if (vkCreateCommandPool(s_Context->LogicalDevice, &createInfo, nullptr, &s_Context->GraphicsCommandPool) != VK_SUCCESS)
		throw std::runtime_error("Failed to create a Command Pool!");
}

void VulkanRenderer::CreateCommandBuffers()
{
	s_Context->CommandBuffers.resize(s_Context->SwapChainFramebuffers.size());

	VkCommandBufferAllocateInfo cbAllocInfo = {};
	cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.commandPool = s_Context->GraphicsCommandPool;
	cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbAllocInfo.commandBufferCount = (uint32_t)s_Context->CommandBuffers.size();

	if (vkAllocateCommandBuffers(s_Context->LogicalDevice, &cbAllocInfo, s_Context->CommandBuffers.data()) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate Command Buffers!");
}

void VulkanRenderer::RecordCommands()
{
	VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = s_Context->RenderPass;
	renderPassBeginInfo.renderArea.offset = { 0, 0 };
	renderPassBeginInfo.renderArea.extent = s_Context->SwapChainExtent;

	constexpr VkClearValue clearValues[] = {
		{ 0.6f, 0.65f, 0.4f, 1.0f }
	};

	renderPassBeginInfo.clearValueCount = (uint32_t)std::size(clearValues);
	renderPassBeginInfo.pClearValues = clearValues;

	for (size_t i = 0; i < s_Context->CommandBuffers.size(); i++)
	{
		const auto& commandBuffer = s_Context->CommandBuffers[i];
		renderPassBeginInfo.framebuffer = s_Context->SwapChainFramebuffers[i];

		if (vkBeginCommandBuffer(commandBuffer, &bufferBeginInfo) != VK_SUCCESS)
			throw std::runtime_error("Failed to start recording a Command Buffer!");

		{
			vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_Context->GraphicsPipeline);

			for (const auto& mesh : s_Meshes)
			{
				const VkBuffer vertexBuffers[] = { mesh.GetVertexBuffer() };
				constexpr VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, (uint32_t)std::size(vertexBuffers), vertexBuffers, offsets);

				vkCmdBindIndexBuffer(commandBuffer, mesh.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

				vkCmdDrawIndexed(commandBuffer, mesh.GetIndicesCount(), 1, 0, 0, 0);
			}

			vkCmdEndRenderPass(commandBuffer);
		}

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
			throw std::runtime_error("Failed to stop recording a Command Buffer!");
	}
}

void VulkanRenderer::CreateSynchronization()
{
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < Utils::MAX_FRAME_DRAWS; i++)
	{
		if (vkCreateSemaphore(s_Context->LogicalDevice, &semaphoreCreateInfo, nullptr, &s_Context->ImageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(s_Context->LogicalDevice, &semaphoreCreateInfo, nullptr, &s_Context->RenderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(s_Context->LogicalDevice, &fenceCreateInfo, nullptr, &s_Context->DrawFences[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create a Semaphore and/or Fence!");
	}
}
