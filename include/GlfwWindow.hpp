#pragma once

#include <stdexcept>

#include <GLFW/glfw3.h>

#ifndef GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>

struct GlfwWindow
{
    GLFWwindow* _window{nullptr};
    HWND _hwnd{nullptr};

    GlfwWindow(int width, int height, const char* title)
    {
        if (glfwInit() != GLFW_TRUE)
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        _window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!_window)
        {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }
        _hwnd = glfwGetWin32Window(_window);
    }

    ~GlfwWindow()
    {
        glfwDestroyWindow(_window);
        glfwTerminate();
    }
};
