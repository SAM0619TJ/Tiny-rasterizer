#include "ShaderManager.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {

std::vector<char> readBinaryFile(const std::string &path) {
  std::ifstream file(path, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open SPIR-V file: " + path);
  }
  const auto size = static_cast<size_t>(file.tellg());
  std::vector<char> buffer(size);
  file.seekg(0);
  file.read(buffer.data(), static_cast<std::streamsize>(size));
  return buffer;
}

std::string stemOf(const std::string &path) {
  const size_t slash = path.find_last_of("/\\");
  const std::string fileName =
      slash == std::string::npos ? path : path.substr(slash + 1);
  const size_t dot = fileName.find_last_of('.');
  return dot == std::string::npos ? fileName : fileName.substr(0, dot);
}

const char *stageFlag(ShaderStage stage) {
  return stage == ShaderStage::Vertex ? "vert" : "frag";
}

long long fileMtime(const std::string &path) {
  std::error_code ec;
  const auto time = fs::last_write_time(path, ec);
  if (ec) {
    return 0;
  }
  return time.time_since_epoch().count();
}

} // namespace

ShaderManager::~ShaderManager() { destroy(); }

void ShaderManager::init(VkDevice device, bool runtimeCompile,
                         const std::string &spirvDir) {
  device_ = device;
  runtimeCompile_ = runtimeCompile;
  if (!spirvDir.empty()) {
    spirvDir_ = spirvDir;
  }
}

std::string ShaderManager::offlineSpvPath(const std::string &glslPath) const {
  return spirvDir_ + "/" + stemOf(glslPath) + ".spv";
}

std::string ShaderManager::runtimeSpvPath(const std::string &glslPath) const {
  return spirvDir_ + "/" + stemOf(glslPath) + ".dev.spv";
}

std::string ShaderManager::watchedPath(const std::string &glslPath) const {
  return runtimeCompile_ ? glslPath : offlineSpvPath(glslPath);
}

std::vector<char> ShaderManager::loadSpirv(const std::string &glslPath,
                                           ShaderStage stage) const {
  if (runtimeCompile_) {
    const std::string output = runtimeSpvPath(glslPath);
    std::error_code ec;
    fs::create_directories(spirvDir_, ec);

    const std::string command = std::string("glslc -fshader-stage=") +
                                stageFlag(stage) + " \"" + glslPath +
                                "\" -o \"" + output + "\"";
    const int rc = std::system(command.c_str());
    if (rc == 0 && fs::exists(output)) {
      std::cout << "[shader] Runtime compiled " << glslPath << " -> " << output
                << std::endl;
      return readBinaryFile(output);
    }
    std::cerr << "[shader] Runtime compile failed for " << glslPath
              << " (rc=" << rc << "), falling back to offline SPIR-V"
              << std::endl;
  }
  return readBinaryFile(offlineSpvPath(glslPath));
}

VkShaderModule
ShaderManager::createModule(const std::vector<char> &code) const {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

  VkShaderModule shaderModule = VK_NULL_HANDLE;
  if (vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed creating Vulkan shader module");
  }
  return shaderModule;
}

VkShaderModule ShaderManager::getModule(const std::string &glslPath,
                                        ShaderStage stage) {
  auto it = cache_.find(glslPath);
  if (it != cache_.end() && it->second.module != VK_NULL_HANDLE) {
    return it->second.module;
  }

  const std::vector<char> code = loadSpirv(glslPath, stage);
  VkShaderModule module = createModule(code);
  cache_[glslPath] = Cached{module, fileMtime(watchedPath(glslPath))};
  return module;
}

bool ShaderManager::sourceChanged(const std::string &glslPath,
                                  ShaderStage) const {
  auto it = cache_.find(glslPath);
  if (it == cache_.end()) {
    return false;
  }
  return fileMtime(watchedPath(glslPath)) != it->second.watchedMtime;
}

void ShaderManager::invalidate(const std::string &glslPath) {
  auto it = cache_.find(glslPath);
  if (it == cache_.end()) {
    return;
  }
  if (it->second.module != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
    vkDestroyShaderModule(device_, it->second.module, nullptr);
  }
  cache_.erase(it);
}

void ShaderManager::destroy() {
  if (device_ != VK_NULL_HANDLE) {
    for (auto &entry : cache_) {
      if (entry.second.module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, entry.second.module, nullptr);
      }
    }
  }
  cache_.clear();
  device_ = VK_NULL_HANDLE;
}
