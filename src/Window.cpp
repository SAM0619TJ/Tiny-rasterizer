#include "Window.h"
#include <iostream>
#include <stdexcept>

// 静态成员初始化
GPUConfig Window::gpuConfig = GPUConfig();

Window::Window(int width, int height, const std::string& title) 
    : window(nullptr), initialized(false) {
    setupWindow(width, height, title);
}

Window::Window(const WindowConfig& config)
    : window(nullptr), initialized(false) {
    setupWindow(config.width, config.height, config.title);
    
    // 设置 VSync
    glfwSwapInterval(config.vsync ? 1 : 0);
}

Window::~Window() {
    if (window) {
        glfwDestroyWindow(window);
    }
}

void Window::setupWindow(int width, int height, const std::string& title) {
    // 使用配置的OpenGL版本
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, gpuConfig.openglMajor);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, gpuConfig.openglMinor);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    
    // GPU加速优化设置
    glfwWindowHint(GLFW_DOUBLEBUFFER, GL_TRUE);      // 双缓冲
    glfwWindowHint(GLFW_SAMPLES, gpuConfig.samples);  // 使用配置的MSAA采样数
    glfwWindowHint(GLFW_REFRESH_RATE, GLFW_DONT_CARE); // 使用最高刷新率

    // 创建窗口
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        throw std::runtime_error("Failed to create GLFW window");
    }

    // 设置当前上下文
    makeContextCurrent();

    // 初始化GLEW
    initGLEW();
    
    // 设置视口
    glViewport(0, 0, width, height);
    
    // GPU加速优化设置
    glDisable(GL_DEPTH_TEST);     // 2D渲染不需要深度测试
    glDisable(GL_STENCIL_TEST);   // 不需要模板测试
    glDisable(GL_BLEND);          // 不需要混合（除非有透明度）
    glDisable(GL_CULL_FACE);      // 不需要面剔除
    
    // 打印 OpenGL 和 GPU 信息
    std::cout << "\n=== GPU 信息 ===" << std::endl;
    std::cout << "OpenGL Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    
    // 检查GPU扩展支持
    GLint maxTextureSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    std::cout << "Max Texture Size: " << maxTextureSize << std::endl;
    
    GLint maxVertexAttribs;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
    std::cout << "Max Vertex Attributes: " << maxVertexAttribs << std::endl;
    std::cout << "=================\n" << std::endl;

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

void Window::setTitle(const std::string& title) {
    glfwSetWindowTitle(window, title.c_str());
}

void Window::setGPUConfig(const GPUConfig& config) {
    gpuConfig = config;
}