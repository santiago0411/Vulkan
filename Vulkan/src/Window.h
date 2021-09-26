#pragma once

#include <GLFW/glfw3.h>

class Window
{
public:
	Window(const char* name, const uint32_t width, const uint32_t height);
	Window() = delete;
	Window(const Window&) = delete;
	~Window();

	void Update() const;

	GLFWwindow* GetGLFWWindow() const { return m_Window; }
private:
	GLFWwindow* m_Window = nullptr;
};

