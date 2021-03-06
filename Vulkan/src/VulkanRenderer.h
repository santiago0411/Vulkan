#pragma once

#include "Base.h"
#include "Window.h"

#include <vector>

class VulkanRenderer
{
public:
	static bool Init(Ref<Window> windowContext);
	static void Draw();
	static void Shutdown();

private:
	static std::vector<const char*> ValidateExtensions();
	static void CreateInstance(const std::vector<const char*>& extensions);
	static void CreateSurface();
	static void GetPhysicalDevice();
	static void CreateLogicalDevice();
	static void CreateSwapChain();
	static void CreateRenderPass();
	static void CreateGraphicsPipeline();
	static void CreateFramebuffers();
	static void CreateCommandPool();
	static void CreateCommandBuffers();
	static void RecordCommands();
	static void CreateSynchronization();
};
