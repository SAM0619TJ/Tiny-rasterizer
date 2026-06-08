#ifndef VULKAN_RENDERER_H
#define VULKAN_RENDERER_H

#include "Renderer.h"

#include <memory>

class VulkanRenderer : public Renderer {
public:
  VulkanRenderer();
  ~VulkanRenderer() override;

  static void configureWindowHints();
  static void runHeadlessSmokeTest();

  // 着色器加载选项（运行时编译/热重载/离线 SPIR-V 目录），需在 init 前调用。
  void setShaderOptions(bool runtimeCompile, bool hotReload,
                        const std::string &spirvDir);
  // 运行时切换场景（重建图形管线）。
  void setScene(const ShaderScene &scene);
  // 开发模式下检测着色器源变化并热重载。
  void pollShaderReload();
  // 后处理参数（需在 init 前调用）。
  void setPostProcessingConfig(const PostProcessingConfig &config);
  // 运行时切换后处理开关（对应 composite 中 enabled 参数）。
  void togglePostProcessing();

  void init(Window &window, const ShaderScene &scene,
            const WindowConfig &windowConfig) override;
  void beginFrame(const FrameParams &params) override;
  void draw(const FrameParams &params) override;
  void endFrame() override;
  void resize(int width, int height) override;
  void shutdown() override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};

#endif // VULKAN_RENDERER_H
