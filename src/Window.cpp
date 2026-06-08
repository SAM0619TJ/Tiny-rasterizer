#include "Window.h"

#include <cstdlib>
#include <stdexcept>

Window::Window(int width, int height, const std::string &title)
    : window(nullptr), initialized(false) {
  setupWindow(width, height, title);
}

Window::Window(const WindowConfig &config)
    : window(nullptr), initialized(false) {
  setupWindow(config.width, config.height, config.title);
}

Window::~Window() {
  if (window) {
    glfwDestroyWindow(window);
  }
}

void Window::setupWindow(int width, int height, const std::string &title) {
  // 创建窗口
  window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
  if (!window) {
    throw std::runtime_error("Failed to create GLFW window");
  }

  initialized = true;
}

void Window::initGLFW() {
  if (std::getenv("TINY_RASTERIZER_MAX_FRAMES") != nullptr) {
#ifdef __APPLE__
    glfwInitHint(GLFW_COCOA_MENUBAR, GLFW_FALSE);
#endif
  }

  if (!glfwInit()) {
    throw std::runtime_error("Failed to initialize GLFW");
  }
}

void Window::terminateGLFW() { glfwTerminate(); }

double Window::getTime() { return glfwGetTime(); }

bool Window::shouldClose() const { return glfwWindowShouldClose(window); }

void Window::pollEvents() { glfwPollEvents(); }

void Window::getFramebufferSize(int &width, int &height) const {
  glfwGetFramebufferSize(window, &width, &height);
}

void Window::getCursorPos(double &xpos, double &ypos) const {
  glfwGetCursorPos(window, &xpos, &ypos);
}

bool Window::getKey(int key) const {
  return window != nullptr && glfwGetKey(window, key) == GLFW_PRESS;
}

void Window::setTitle(const std::string &title) {
  glfwSetWindowTitle(window, title.c_str());
}