#include "Config.h"
#include "RenderTypes.h"
#include "VulkanRenderer.h"
#include "Window.h"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

void printStartupSummary(const ShaderScene &scene,
                         const WindowConfig &windowConfig) {
  std::cout << "\n=== 配置信息 ===" << std::endl;
  std::cout << "场景: " << scene.name << std::endl;
  std::cout << "描述: " << scene.description << std::endl;
  std::cout << "顶点着色器: " << scene.vertexShader << std::endl;
  std::cout << "片段着色器: " << scene.fragmentShader << std::endl;
  std::cout << "窗口大小: " << windowConfig.width << "x" << windowConfig.height
            << std::endl;
  std::cout << "VSync: " << (windowConfig.vsync ? "开启" : "关闭")
            << std::endl;
  std::cout << "Renderer: Vulkan" << std::endl;
  std::cout << "=================\n" << std::endl;
}

std::string makeFpsTitle(const WindowConfig &windowConfig, double fps,
                         double avgMs) {
  std::ostringstream title;
  title << windowConfig.title << " | FPS: " << std::fixed
        << std::setprecision(1) << fps << " | Avg: " << std::setprecision(2)
        << avgMs << "ms";
  return title.str();
}

int readMaxFrames() {
  const char *value = std::getenv("TINY_RASTERIZER_MAX_FRAMES");
  if (value == nullptr) {
    return 0;
  }
  return std::max(0, std::atoi(value));
}

bool isHeadlessTestRun() {
  return std::getenv("TINY_RASTERIZER_HEADLESS_TEST") != nullptr;
}

int runApplication() {
  std::cout << "[init] Loading configuration..." << std::endl;
  Config config("config/shader_config.yaml");

  const ShaderScene activeScene = config.getActiveScene();
  const WindowConfig &windowConfig = config.getWindowConfig();
  const PerformanceConfig &perfConfig = config.getPerformanceConfig();
  const ShaderConfig &shaderConfig = config.getShaderConfig();
  const PostProcessingConfig &postConfig = config.getPostProcessingConfig();
  printStartupSummary(activeScene, windowConfig);

  if (isHeadlessTestRun()) {
    VulkanRenderer::runHeadlessSmokeTest();
    return 0;
  }

  Window::initGLFW();
  VulkanRenderer::configureWindowHints();

  {
    Window window(windowConfig);
    VulkanRenderer renderer;
    renderer.setShaderOptions(shaderConfig.runtimeCompile,
                              shaderConfig.hotReload, shaderConfig.spirvDir);
    renderer.setPostProcessingConfig(postConfig);
    renderer.init(window, activeScene, windowConfig);

    // 收集场景列表，支持运行时用数字键 1..N 切换
    std::vector<std::pair<std::string, ShaderScene>> sceneList;
    for (const auto &entry : config.getAllScenes()) {
      sceneList.emplace_back(entry.first, entry.second);
    }
    std::vector<char> sceneKeyDown(sceneList.size(), 0);
    char postKeyDown = 0;

    std::cout << "[input] 按数字键切换场景:";
    for (size_t i = 0; i < sceneList.size() && i < 9; ++i) {
      std::cout << " [" << (i + 1) << "]=" << sceneList[i].first;
    }
    std::cout << " | [P]=后处理开关" << std::endl;
    std::cout << "[post] 后处理: "
              << (postConfig.enabled ? "开启" : "关闭")
              << " (exposure=" << postConfig.exposure
              << ", vignette=" << postConfig.vignette
              << ", grain=" << postConfig.grain << ")" << std::endl;
    if (shaderConfig.hotReload) {
      std::cout << "[shader] 热重载已启用 ("
                << (shaderConfig.runtimeCompile ? "监视 GLSL 源"
                                                : "监视 SPIR-V 输出")
                << ")" << std::endl;
    }

    std::cout << "[frame] Starting render loop..." << std::endl;

    double lastStatsTime = Window::getTime();
    double lastFrameTime = lastStatsTime;
    int frameCount = 0;
    int totalFrameCount = 0;
    const int maxFrames = readMaxFrames();
    double minFrameTime = 999999.0;
    double maxFrameTime = 0.0;
    int lastWidth = 0;
    int lastHeight = 0;

    while (!window.shouldClose()) {
      const double currentFrameTime = Window::getTime();
      const double frameDelta = currentFrameTime - lastFrameTime;
      lastFrameTime = currentFrameTime;
      minFrameTime = std::min(minFrameTime, frameDelta);
      maxFrameTime = std::max(maxFrameTime, frameDelta);

      int width = 0;
      int height = 0;
      window.getFramebufferSize(width, height);
      if (width != lastWidth || height != lastHeight) {
        renderer.resize(width, height);
        lastWidth = width;
        lastHeight = height;
      }

      for (size_t i = 0; i < sceneList.size() && i < 9; ++i) {
        const bool down = window.getKey(GLFW_KEY_1 + static_cast<int>(i));
        if (down && !sceneKeyDown[i]) {
          renderer.setScene(sceneList[i].second);
          std::cout << "[scene] -> " << sceneList[i].first << " ("
                    << sceneList[i].second.name << ")" << std::endl;
        }
        sceneKeyDown[i] = down ? 1 : 0;
      }

      renderer.pollShaderReload();

      const bool postDown = window.getKey(GLFW_KEY_P);
      if (postDown && !postKeyDown) {
        renderer.togglePostProcessing();
      }
      postKeyDown = postDown ? 1 : 0;

      double mouseX = 0.0;
      double mouseY = 0.0;
      window.getCursorPos(mouseX, mouseY);

      const FrameParams params{static_cast<float>(currentFrameTime), width,
                               height, static_cast<float>(mouseX),
                               static_cast<float>(mouseY)};

      renderer.beginFrame(params);
      renderer.draw(params);
      renderer.endFrame();
      window.pollEvents();

      ++frameCount;
      ++totalFrameCount;
      if (maxFrames > 0 && totalFrameCount >= maxFrames) {
        std::cout << "[test] Reached frame limit: " << maxFrames << std::endl;
        break;
      }

      const double elapsed = currentFrameTime - lastStatsTime;
      if (elapsed >= perfConfig.fpsUpdateInterval && frameCount > 0) {
        const double fps = static_cast<double>(frameCount) / elapsed;
        const double avgMs = elapsed * 1000.0 / static_cast<double>(frameCount);

        if (perfConfig.showConsoleFps) {
          std::cout << "FPS: " << std::fixed << std::setprecision(1) << fps
                    << " | Avg: " << std::setprecision(2) << avgMs
                    << "ms | Min: " << minFrameTime * 1000.0
                    << "ms | Max: " << maxFrameTime * 1000.0 << "ms"
                    << std::endl;
        }

        if (perfConfig.showTitleFps) {
          window.setTitle(makeFpsTitle(windowConfig, fps, avgMs));
        }

        frameCount = 0;
        lastStatsTime = currentFrameTime;
        minFrameTime = 999999.0;
        maxFrameTime = 0.0;
      }
    }

    renderer.shutdown();
  }

  Window::terminateGLFW();
  return 0;
}

} // namespace

int main() {
  try {
    return runApplication();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    const std::string message = e.what();
    if (isHeadlessTestRun() &&
        (message.find("VK_ERROR_INCOMPATIBLE_DRIVER") != std::string::npos ||
         message.find("VkResult -9") != std::string::npos)) {
      std::cerr << "[test] Skipping Vulkan runtime test: no compatible Vulkan "
                   "device in this environment"
                << std::endl;
      Window::terminateGLFW();
      return 77;
    }
    Window::terminateGLFW();
    return -1;
  }
}

