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
