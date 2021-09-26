#pragma once

#include "Base.h"
#include "Window.h"

class VulkanRenderer
{
public:
	static bool Init(WeakRef<Window> windowContext);
	static void Shutdown();

private:
	static void CreateInstance();
	static void GetPhysicalDevice();
	static void CreateLogicalDevice();
};
