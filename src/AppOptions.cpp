#include "AppOptions.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <utility>

namespace {

int readEnvInt(const char *name, int fallback) {
  const char *value = std::getenv(name);
  if (value == nullptr) {
    return fallback;
  }
  return std::max(0, std::atoi(value));
}

bool hasEnv(const char *name) { return std::getenv(name) != nullptr; }

} // namespace

AppOptions AppOptions::fromCommandLine(int argc, char **argv) {
  AppOptions options;
  if (argc > 1 && argv[1] != nullptr && argv[1][0] != '\0') {
    options.configPath = argv[1];
  }

  options.maxFrames = readEnvInt("TINY_RASTERIZER_MAX_FRAMES", 0);
  options.headlessTest = hasEnv("TINY_RASTERIZER_HEADLESS_TEST");
  options.stressTest = hasEnv("TINY_RASTERIZER_STRESS_TEST");
  options.strictValidation = hasEnv("TINY_RASTERIZER_STRICT_VALIDATION");
  options.forceComputeTexture = hasEnv("TINY_RASTERIZER_FORCE_COMPUTE_TEXTURE");
  return options;
}

void AppOptions::applyTo(PostProcessingConfig &post,
                         ComputeConfig &compute) const {
  if (!forceComputeTexture) {
    return;
  }
  // 强制走 compute 路径：改的是配置意图，而不是在渲染器里插测试分支。
  post.textureSource = TextureSource::Compute;
  compute.enabled = true;
}

StressDriver::StressDriver(std::vector<ShaderScene> scenes)
    : scenes_(std::move(scenes)) {}

void StressDriver::tick(const StressActions &actions, Window &window,
                        int frameIndex) {
  if (frameIndex <= 0) {
    return;
  }

  static const int kSizes[4][2] = {
      {1000, 600}, {640, 480}, {1280, 720}, {800, 500}};

  if (frameIndex % 48 == 0 && actions.resize) {
    const int(*size)[2] = &kSizes[(frameIndex / 48) % 4];
    window.setWindowSize((*size)[0], (*size)[1]);
    actions.resize((*size)[0], (*size)[1]);
    ++resizeCount_;
    std::cout << "[stress] resize -> " << (*size)[0] << "x" << (*size)[1]
              << std::endl;
  }

  if (frameIndex % 32 == 0 && actions.togglePostProcessing) {
    actions.togglePostProcessing();
    ++postToggleCount_;
  }

  if (frameIndex % 64 == 0 && !scenes_.empty() && actions.setScene) {
    sceneIndex_ = (sceneIndex_ + 1) % scenes_.size();
    actions.setScene(scenes_[sceneIndex_]);
  }
}
