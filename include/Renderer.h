#ifndef RENDERER_H
#define RENDERER_H

#include "Config.h"
#include "RenderTypes.h"

class Window;

class Renderer {
public:
  virtual ~Renderer() = default;

  virtual void init(Window &window, const ShaderScene &scene,
                    const WindowConfig &windowConfig) = 0;
  virtual void beginFrame(const FrameParams &params) = 0;
  virtual void draw(const FrameParams &params) = 0;
  virtual void endFrame() = 0;
  virtual void resize(int width, int height) = 0;
  virtual void shutdown() = 0;
};

#endif // RENDERER_H
