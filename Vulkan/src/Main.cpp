#include "Window.h"
#include "VulkanRenderer.h"

#include <GLFW/glfw3.h>

#include <iostream>

static bool InitGLFW()
{
	if (!glfwInit())
		return false;

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	return true;
}

int main()
{
	if (!InitGLFW())
		return -1;

	const auto window = CreateRef<Window>("Vulkan", 1280, 720);
	
	if (!VulkanRenderer::Init(window))
		return -1;

	while (!glfwWindowShouldClose(window->m_Window))
	{
		window->Update();
		glfwPollEvents();
	}

	VulkanRenderer::Shutdown();

	return 0;
}