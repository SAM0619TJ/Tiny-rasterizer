#ifndef APP_OPTIONS_H
#define APP_OPTIONS_H

#include "Config.h"
#include "Window.h"

#include <functional>
#include <string>
#include <vector>

// 应用层入口选项：整个程序里唯一读取 argv / 环境变量的地方。
//
// 分层约束（对应 README Phase 1）：
//   - 渲染器只依赖 Config 与每帧参数，不感知“测试模式”；
//   - 入口层把命令行与环境变量归一化为结构体，再投影回配置层。
struct AppOptions {
  std::string configPath = "config/shader_config.yaml";
  int maxFrames = 0;                 // 0 = 不限帧
  bool headlessTest = false;         // 只初始化核心对象，不创建窗口
  bool stressTest = false;           // 自动 resize + 切换后处理/场景
  bool strictValidation = false;     // 有 validation error 时以退出码 2 结束
  bool forceComputeTexture = false;  // 覆盖纹理来源为 Compute

  static AppOptions fromCommandLine(int argc, char **argv);

  bool hideWindow() const { return maxFrames > 0 || headlessTest; }

  // 把入口选项投影到配置层，保持渲染器只读配置。
  void applyTo(PostProcessingConfig &post, ComputeConfig &compute) const;
};

// 压测驱动需要的能力集合。
// 刻意不把这些操作加进 Renderer 抽象接口：它们属于入口层的验收手段，
// 不是渲染后端必须实现的能力。
struct StressActions {
  std::function<void(int, int)> resize;
  std::function<void()> togglePostProcessing;
  std::function<void(const ShaderScene &)> setScene;
};

// 压测驱动：按固定帧间隔触发 resize、后处理开关与场景切换。
class StressDriver {
public:
  explicit StressDriver(std::vector<ShaderScene> scenes);

  // 在渲染循环中调用；frameIndex 从 1 开始递增。
  void tick(const StressActions &actions, Window &window, int frameIndex);

  int resizeCount() const { return resizeCount_; }
  int postToggleCount() const { return postToggleCount_; }

private:
  std::vector<ShaderScene> scenes_;
  size_t sceneIndex_ = 0;
  int resizeCount_ = 0;
  int postToggleCount_ = 0;
};

#endif // APP_OPTIONS_H
