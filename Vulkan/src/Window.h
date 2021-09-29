#pragma once

#include "Base.h"

#include <GLFW/glfw3.h>

class Window
{
public:
	Window() = delete;
	Window(const Window&) = delete;
	~Window();

	void Update() const;

	GLFWwindow* GetGLFWWindow() const { return m_Window; }

	void GetWidthAndHeight(int32_t& outWidth, int32_t& outHeight) const;

	static Ref<Window> Create(const char* name, const uint32_t width, const uint32_t height);

private:
	Window(const char* name, const uint32_t width, const uint32_t height);

private:
	GLFWwindow* m_Window = nullptr;
};

