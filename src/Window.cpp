#include "Window.h"
#include <iostream>
#include <stdexcept>

Window::Window(int width, int height, const std::string& title) 
    : window(nullptr), initialized(false) {
    setupWindow(width, height, title);
}

Window::~Window() {
    if (window) {
        glfwDestroyWindow(window);
    }
}

void Window::setupWindow(int width, int height, const std::string& title) {
    // 设置OpenGL版本和配置
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 创建窗口
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        throw std::runtime_error("Failed to create GLFW window");
    }

    // 设置当前上下文
    makeContextCurrent();

    // 初始化GLEW
    initGLEW();

    initialized = true;
}

void Window::initGLEW() {
    if (glewInit() != GLEW_OK) {
        throw std::runtime_error("Failed to initialize GLEW");
    }
}

void Window::initGLFW() {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
}

void Window::terminateGLFW() {
    glfwTerminate();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window);
}

void Window::makeContextCurrent() {
    glfwMakeContextCurrent(window);
}

void Window::swapBuffers() {
    glfwSwapBuffers(window);
}

void Window::pollEvents() {
    glfwPollEvents();
}

void Window::getFramebufferSize(int& width, int& height) const {
    glfwGetFramebufferSize(window, &width, &height);
}

void Window::getCursorPos(double& xpos, double& ypos) const {
    glfwGetCursorPos(window, &xpos, &ypos);
}