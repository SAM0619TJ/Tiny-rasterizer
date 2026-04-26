#include "OpenGLRenderer.h"

#include "Shader.h"
#include "ShaderSource.h"
#include "Window.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>

OpenGLRenderer::OpenGLRenderer() = default;

OpenGLRenderer::~OpenGLRenderer() { shutdown(); }

void OpenGLRenderer::configureWindowHints(const GPUConfig &gpuConfig) {
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, gpuConfig.openglMajor);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, gpuConfig.openglMinor);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
  glfwWindowHint(GLFW_SAMPLES, gpuConfig.samples);
  glfwWindowHint(GLFW_REFRESH_RATE, GLFW_DONT_CARE);
}

void OpenGLRenderer::init(Window &targetWindow, const ShaderScene &scene,
                          const WindowConfig &windowConfig) {
  window = &targetWindow;

  glfwMakeContextCurrent(window->getGLFWwindow());
  initGLEW();
  glfwSwapInterval(windowConfig.vsync ? 1 : 0);

  int width = 0;
  int height = 0;
  window->getFramebufferSize(width, height);
  resize(width, height);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);

  logDeviceInfo();

  std::cout << "[shader] Loading shaders..." << std::endl;
  const ShaderSourcePair sources =
      ShaderSource::loadPair(scene.vertexShader, scene.fragmentShader);
  shader = std::make_unique<Shader>(sources.vertex, sources.fragment);
  shader->setupQuad();
  std::cout << "[shader] Shaders loaded successfully." << std::endl;

  initialized = true;
}

void OpenGLRenderer::beginFrame(const FrameParams &) {
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLRenderer::draw(const FrameParams &params) {
  if (!shader) {
    throw std::runtime_error("OpenGLRenderer draw called before init");
  }

  shader->use();
  shader->setFloat("iTime", params.time);
  shader->setVec2("iResolution", static_cast<float>(params.width),
                  static_cast<float>(params.height));
  shader->setVec2("iMouse", params.mouseX, params.mouseY);

  glBindVertexArray(shader->getVAO());
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void OpenGLRenderer::endFrame() {
  if (window) {
    glfwSwapBuffers(window->getGLFWwindow());
  }
}

void OpenGLRenderer::resize(int width, int height) {
  glViewport(0, 0, width, height);
}

void OpenGLRenderer::shutdown() {
  if (!initialized) {
    return;
  }

  shader.reset();
  window = nullptr;
  initialized = false;
}

void OpenGLRenderer::initGLEW() {
  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    throw std::runtime_error("Failed to initialize GLEW");
  }
}

void OpenGLRenderer::logDeviceInfo() const {
  std::cout << "\n=== GPU 信息 ===" << std::endl;
  std::cout << "OpenGL Vendor: " << glGetString(GL_VENDOR) << std::endl;
  std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
  std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
  std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION)
            << std::endl;

  GLint maxTextureSize = 0;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
  std::cout << "Max Texture Size: " << maxTextureSize << std::endl;

  GLint maxVertexAttribs = 0;
  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
  std::cout << "Max Vertex Attributes: " << maxVertexAttribs << std::endl;
  std::cout << "=================\n" << std::endl;
}
