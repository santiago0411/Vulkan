#pragma once

#include <GLFW/glfw3.h>

int main();

class Window
{
public:
	Window(const char* name, const uint32_t width, const uint32_t height);
	Window() = delete;
	Window(const Window&) = delete;
	~Window();

	void Update() const;
private:
	friend int main();
	GLFWwindow* m_Window = nullptr;
};

