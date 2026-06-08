#ifndef SHADER_MANAGER_H
#define SHADER_MANAGER_H

#include <vulkan/vulkan.h>

#include <string>
#include <unordered_map>
#include <vector>

enum class ShaderStage { Vertex, Fragment };

// 负责把场景的 GLSL 路径解析为 VkShaderModule：
//   - 离线模式：读取构建期生成的 shaders_spirv/<stem>.spv
//   - 运行时模式：调用 glslc 即时编译 GLSL -> SPIR-V（便于迭代）
// 同时缓存 VkShaderModule，并跟踪源文件修改时间以支持热重载。
class ShaderManager {
public:
  ShaderManager() = default;
  ~ShaderManager();

  ShaderManager(const ShaderManager &) = delete;
  ShaderManager &operator=(const ShaderManager &) = delete;
  ShaderManager(ShaderManager &&) noexcept = default;
  ShaderManager &operator=(ShaderManager &&) noexcept = default;

  void init(VkDevice device, bool runtimeCompile, const std::string &spirvDir);

  // 获取（必要时构建并缓存）GLSL 源对应的 shader module。
  VkShaderModule getModule(const std::string &glslPath, ShaderStage stage);

  // 自上次加载以来被监视的文件是否发生变化（热重载判定）。
  bool sourceChanged(const std::string &glslPath, ShaderStage stage) const;

  // 丢弃指定源的缓存 module，下次 getModule 会重新构建。
  void invalidate(const std::string &glslPath);

  void destroy();

private:
  struct Cached {
    VkShaderModule module = VK_NULL_HANDLE;
    long long watchedMtime = 0;
  };

  VkDevice device_ = VK_NULL_HANDLE;
  bool runtimeCompile_ = false;
  std::string spirvDir_ = "shaders_spirv";
  std::unordered_map<std::string, Cached> cache_;

  std::vector<char> loadSpirv(const std::string &glslPath,
                              ShaderStage stage) const;
  // 离线模式监视 .spv，运行时模式监视 GLSL 源
  std::string watchedPath(const std::string &glslPath) const;
  std::string offlineSpvPath(const std::string &glslPath) const;
  std::string runtimeSpvPath(const std::string &glslPath) const;
  VkShaderModule createModule(const std::vector<char> &code) const;
};

#endif // SHADER_MANAGER_H
