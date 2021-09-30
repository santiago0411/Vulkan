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

	Ref<Window> window = Window::Create("Vulkan", 800, 600);
	
	if (!VulkanRenderer::Init(window))
		return -1;

	while (!glfwWindowShouldClose(window->GetGLFWWindow()))
	{
		glfwPollEvents();
		VulkanRenderer::Draw();
	}

	VulkanRenderer::Shutdown();

	return 0;
}