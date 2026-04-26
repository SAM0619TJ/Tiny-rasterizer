#ifndef OPENGL_RENDERER_H
#define OPENGL_RENDERER_H

#include "Renderer.h"

#include <memory>

class Shader;

class OpenGLRenderer : public Renderer {
public:
  OpenGLRenderer();
  ~OpenGLRenderer() override;

  static void configureWindowHints(const GPUConfig &gpuConfig);

  void init(Window &window, const ShaderScene &scene,
            const WindowConfig &windowConfig) override;
  void beginFrame(const FrameParams &params) override;
  void draw(const FrameParams &params) override;
  void endFrame() override;
  void resize(int width, int height) override;
  void shutdown() override;

private:
  Window *window = nullptr;
  std::unique_ptr<Shader> shader;
  bool initialized = false;

  void initGLEW();
  void logDeviceInfo() const;
};

#endif // OPENGL_RENDERER_H
