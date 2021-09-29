#include "Window.h"

Ref<Window> Window::Create(const char* name, const uint32_t width, const uint32_t height)
{
	auto window = std::shared_ptr<Window>();
	window.reset(new Window(name, width, height));
	return window;
}

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

void Window::GetWidthAndHeight(int32_t& outWidth, int32_t& outHeight) const
{
	glfwGetFramebufferSize(m_Window, &outWidth, &outHeight);
}
