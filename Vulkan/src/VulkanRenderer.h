#pragma once

#include "Base.h"
#include "Window.h"

#include <vector>

class VulkanRenderer
{
public:
	static bool Init(Ref<Window> windowContext);
	static void Shutdown();

private:
	static std::vector<const char*> ValidateExtensions();
	static void CreateInstance(const std::vector<const char*>& extensions);
	static void CreateSurface();
	static void GetPhysicalDevice();
	static void CreateLogicalDevice();
	static void CreateSwapChain();
};
