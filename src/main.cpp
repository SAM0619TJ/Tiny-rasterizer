#include "AppOptions.h"
#include "Config.h"
#include "RenderTypes.h"
#include "VulkanRenderer.h"
#include "Window.h"

#include <algorithm>
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

void printAcceptanceBaseline(double startupMs, double lifetimeSeconds,
                             int totalFrames, double averageFrameMs,
                             double worstFrameMs, int resizeCount) {
  std::cout << "\n=== 验收基线 ===" << std::endl;
  std::cout << "启动耗时: " << std::fixed << std::setprecision(1) << startupMs
            << " ms" << std::endl;
  std::cout << "运行时长: " << std::setprecision(2) << lifetimeSeconds << " s"
            << std::endl;
  std::cout << "总帧数: " << totalFrames << std::endl;
  std::cout << "平均帧时: " << std::setprecision(3) << averageFrameMs
            << " ms" << std::endl;
  std::cout << "最差帧时: " << worstFrameMs << " ms" << std::endl;
  std::cout << "resize 次数: " << resizeCount << std::endl;
  std::cout << "validation: errors=" << VulkanRenderer::validationErrorCount()
            << " warnings=" << VulkanRenderer::validationWarningCount()
            << std::endl;
  std::cout << "================\n" << std::endl;
}

int runApplication(const AppOptions &options) {
  std::cout << "[init] Loading configuration from " << options.configPath
            << "..." << std::endl;
  Config config(options.configPath);

  const ShaderScene activeScene = config.getActiveScene();
  const WindowConfig &windowConfig = config.getWindowConfig();
  const PerformanceConfig &perfConfig = config.getPerformanceConfig();
  const ShaderConfig &shaderConfig = config.getShaderConfig();

  // 入口选项投影到配置层：渲染器只看到最终的配置值。
  PostProcessingConfig postConfig = config.getPostProcessingConfig();
  ComputeConfig computeConfig = config.getComputeConfig();
  options.applyTo(postConfig, computeConfig);

  printStartupSummary(activeScene, windowConfig);

  if (options.headlessTest) {
    VulkanRenderer::runHeadlessSmokeTest();
    return 0;
  }

  Window::initGLFW();
  VulkanRenderer::configureWindowHints(options.hideWindow());

  const double startupBeginTime = Window::getTime();

  {
    Window window(windowConfig);
    VulkanRenderer renderer;
    renderer.setShaderOptions(shaderConfig.runtimeCompile,
                              shaderConfig.hotReload, shaderConfig.spirvDir);
    renderer.setPostProcessingConfig(postConfig);
    renderer.setComputeConfig(computeConfig);
    renderer.init(window, activeScene, windowConfig);
    const double startupMs = (Window::getTime() - startupBeginTime) * 1000.0;

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
    std::cout << "[compute] "
              << (computeConfig.enabled ? "启用" : "关闭")
              << " (texture_size=" << computeConfig.textureSize
              << ", shader=" << computeConfig.grainShader << ")" << std::endl;
    if (shaderConfig.hotReload) {
      std::cout << "[shader] 热重载已启用 ("
                << (shaderConfig.runtimeCompile ? "监视 GLSL 源"
                                                : "监视 SPIR-V 输出")
                << ")" << std::endl;
    }

    std::cout << "[frame] Starting render loop..." << std::endl;

    StressDriver stress({});
    StressActions stressActions;
    if (options.stressTest) {
      std::vector<ShaderScene> stressScenes;
      stressScenes.reserve(sceneList.size());
      for (const auto &entry : sceneList) {
        stressScenes.push_back(entry.second);
      }
      stress = StressDriver(std::move(stressScenes));
      stressActions.resize = [&renderer](int w, int h) {
        renderer.resize(w, h);
      };
      stressActions.togglePostProcessing = [&renderer] {
        renderer.togglePostProcessing();
      };
      stressActions.setScene = [&renderer](const ShaderScene &scene) {
        renderer.setScene(scene);
      };
    }

    double lastStatsTime = Window::getTime();
    double lastFrameTime = lastStatsTime;
    int frameCount = 0;
    int totalFrameCount = 0;
    const int maxFrames = options.maxFrames;
    double minFrameTime = 999999.0;
    double maxFrameTime = 0.0;
    int lastWidth = 0;
    int lastHeight = 0;
    int resizeCount = 0;
    double totalFrameTimeMs = 0.0;

    while (!window.shouldClose()) {
      const double currentFrameTime = Window::getTime();
      const double frameDelta = currentFrameTime - lastFrameTime;
      lastFrameTime = currentFrameTime;
      minFrameTime = std::min(minFrameTime, frameDelta);
      maxFrameTime = std::max(maxFrameTime, frameDelta);
      totalFrameTimeMs += frameDelta * 1000.0;

      int width = 0;
      int height = 0;
      window.getFramebufferSize(width, height);
      if (width != lastWidth || height != lastHeight) {
        renderer.resize(width, height);
        lastWidth = width;
        lastHeight = height;
        ++resizeCount;
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

      if (options.stressTest) {
        stress.tick(stressActions, window, totalFrameCount);
      }

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

    const double lifetimeSeconds = lastFrameTime - startupBeginTime;
    const double averageFrameMs =
        totalFrameCount > 0 ? totalFrameTimeMs / totalFrameCount : 0.0;
    printAcceptanceBaseline(startupMs, lifetimeSeconds, totalFrameCount,
                            averageFrameMs, maxFrameTime * 1000.0, resizeCount);
    if (options.stressTest) {
      std::cout << "[stress] 后处理切换次数: " << stress.postToggleCount()
                << std::endl;
    }
  }

  if (options.strictValidation && VulkanRenderer::validationErrorCount() > 0) {
    std::cerr << "[validation] 严格模式失败: 检测到 "
              << VulkanRenderer::validationErrorCount() << " 个 validation error"
              << std::endl;
    Window::terminateGLFW();
    return 2;
  }

  Window::terminateGLFW();
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  const AppOptions options = AppOptions::fromCommandLine(argc, argv);

  try {
    return runApplication(options);
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    const std::string message = e.what();
    if (options.headlessTest &&
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

