#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <iostream>

int main()
{
	if (!glfwInit())
		return -1;

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Vulkan", nullptr, nullptr);

	if (!window)
	{
		glfwTerminate();
		return -1;
	}

	uint32_t extensionCount = 0;
	VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	if (result != VK_SUCCESS)
	{
		glfwTerminate();
		return -1;
	}

	std::cout << "Extension count: " << extensionCount << '\n';

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	while (!glfwWindowShouldClose(window))
	{
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}