#include "Window.h"

Window::Window(const char* name, const uint32_t width, const uint32_t height)
{
	m_Window = glfwCreateWindow((int)width, (int)height, name, nullptr, nullptr);
}

Window::~Window()
{
	glfwDestroyWindow(m_Window);
}

void Window::Update() const
{
	glfwSwapBuffers(m_Window);
}
