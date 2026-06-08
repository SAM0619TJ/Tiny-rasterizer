#include "Config.h"

#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::string joinPath(const std::string &base, const std::string &relative) {
  if (base.empty()) {
    return relative;
  }
  if (base.back() == '/') {
    return base + relative;
  }
  return base + "/" + relative;
}

void requireReadableFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  require(file.is_open(), "file must be readable: " + path);
  require(file.tellg() > 0, "file must not be empty: " + path);
}

void testActiveScene(const Config &config) {
  const ShaderScene scene = config.getActiveScene();
  require(config.getActiveSceneName() == "rotation_matrix",
          "active scene key should be rotation_matrix");
  require(scene.name == "Rotation Matrix Effect",
          "active scene display name mismatch");
  require(scene.vertexShader == "shaders/vertex.glsl",
          "active vertex shader path mismatch");
  require(scene.fragmentShader == "shaders/rotation_matrix.glsl",
          "active fragment shader path mismatch");
}

void testSceneCatalog(const Config &config) {
  const std::map<std::string, ShaderScene> &scenes = config.getAllScenes();
  require(scenes.size() == 3, "expected exactly three configured scenes");
  require(scenes.count("rotation_matrix") == 1, "rotation_matrix scene missing");
  require(scenes.count("fractal") == 1, "fractal scene missing");
  require(scenes.count("water") == 1, "water scene missing");
}

void testShaderSources(const Config &config, const std::string &sourceRoot) {
  for (const auto &entry : config.getAllScenes()) {
    requireReadableFile(joinPath(sourceRoot, entry.second.vertexShader));
    requireReadableFile(joinPath(sourceRoot, entry.second.fragmentShader));
  }
}

void testWindowConfig(const Config &config) {
  const WindowConfig &window = config.getWindowConfig();
  require(window.width == 1000, "window width mismatch");
  require(window.height == 600, "window height mismatch");
  require(window.title == "Tiny Rasterizer", "window title mismatch");
  require(!window.vsync, "window vsync should be disabled by default");
}

void testPerformanceConfig(const Config &config) {
  const PerformanceConfig &performance = config.getPerformanceConfig();
  require(performance.fpsUpdateInterval == 0.5,
          "fps update interval mismatch");
  require(performance.showConsoleFps, "console fps should be enabled");
  require(performance.showTitleFps, "title fps should be enabled");
}

void testSpirvOutputs(const std::string &spirvRoot) {
  const std::vector<std::string> outputs = {
      "vertex.spv", "fragment.spv", "rotation_matrix.spv", "water.spv",
      "composite.spv"};

  for (const std::string &output : outputs) {
    requireReadableFile(joinPath(spirvRoot, output));
  }
}

} // namespace

int main(int argc, char **argv) {
  try {
    require(argc >= 3,
            "usage: config_smoke_test <mode> <config-path> [path]");

    const std::string mode = argv[1];
    Config config(argv[2]);

    if (mode == "active_scene") {
      testActiveScene(config);
    } else if (mode == "scene_catalog") {
      testSceneCatalog(config);
    } else if (mode == "shader_sources") {
      require(argc == 4, "shader_sources requires <source-root>");
      testShaderSources(config, argv[3]);
    } else if (mode == "window_config") {
      testWindowConfig(config);
    } else if (mode == "performance_config") {
      testPerformanceConfig(config);
    } else if (mode == "spirv_outputs") {
      require(argc == 4, "spirv_outputs requires <spirv-root>");
      testSpirvOutputs(argv[3]);
    } else {
      throw std::runtime_error("unknown test mode: " + mode);
    }

    std::cout << "Test passed: " << mode << std::endl;
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Test failed: " << e.what() << std::endl;
    return 1;
  }
}
