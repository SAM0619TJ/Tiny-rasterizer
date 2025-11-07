# YAML 配置系统使用指南

## ✅ 已实现功能

项目现在支持通过 YAML 配置文件管理所有关键设置，包括：

- ✅ Shader 路径配置
- ✅ 多场景切换
- ✅ 窗口设置
- ✅ 性能选项
- ✅ GPU 配置

## 📁 配置文件结构

配置文件位置：`config/shader_config.yaml`

### 1. 场景切换

```yaml
# 当前激活的着色器场景
active_scene: "rotation_matrix"  # 可改为: "fractal", "water"

# 着色器场景配置
scenes:
  rotation_matrix:
    name: "Rotation Matrix Effect"
    description: "Box rotation with time-based animation"
    vertex_shader: "shaders/vertex.glsl"
    fragment_shader: "shaders/rotation_matrix.glsl"
    
  fractal:
    name: "Fractal Raymarch"
    description: "3D fractal raymarching effect"
    vertex_shader: "shaders/vertex.glsl"
    fragment_shader: "shaders/fragment.glsl"
```

**切换场景**：修改 `active_scene` 的值即可

### 2. 窗口配置

```yaml
window:
  width: 1000      # 窗口宽度
  height: 600      # 窗口高度
  title: "Tiny Rasterizer"  # 窗口标题
  vsync: false     # true=锁定60fps, false=最大性能
```

### 3. 性能配置

```yaml
performance:
  fps_update_interval: 0.5  # FPS更新间隔(秒)
  show_console_fps: true    # 在终端显示FPS
  show_title_fps: true      # 在窗口标题显示FPS
```

### 4. GPU 配置

```yaml
gpu:
  opengl_major: 4   # OpenGL 主版本号
  opengl_minor: 1   # OpenGL 次版本号
  samples: 0        # MSAA采样数，0=禁用，4/8/16=启用
```

## 🚀 使用方法

### 方式1：修改配置文件

1. 编辑 `config/shader_config.yaml`
2. 修改你想要的设置
3. 重新运行程序

```bash
cd build
./Tiny-rasterizer
```

### 方式2：添加新场景

在 `shader_config.yaml` 中添加新场景：

```yaml
scenes:
  my_new_scene:
    name: "My Custom Effect"
    description: "My awesome shader"
    vertex_shader: "shaders/vertex.glsl"
    fragment_shader: "shaders/my_shader.glsl"
```

然后设置为激活场景：

```yaml
active_scene: "my_new_scene"
```

## 📝 配置示例

### 示例1：高性能模式

```yaml
window:
  width: 1920
  height: 1080
  vsync: false  # 关闭VSync获得最大FPS

performance:
  fps_update_interval: 0.2  # 更频繁地更新FPS
  show_console_fps: false   # 减少终端输出
  show_title_fps: true

gpu:
  samples: 0  # 禁用MSAA提升性能
```

### 示例2：质量优先模式

```yaml
window:
  width: 1920
  height: 1080
  vsync: true  # 启用VSync防止撕裂

gpu:
  samples: 4  # 4x MSAA抗锯齿
```

### 示例3：调试模式

```yaml
window:
  width: 800
  height: 600
  vsync: false

performance:
  fps_update_interval: 0.1  # 快速更新
  show_console_fps: true    # 显示详细信息
  show_title_fps: true
```

## 🔧 代码使用

如果你想在代码中使用配置：

```cpp
#include "Config.h"

// 加载配置
Config config("config/shader_config.yaml");

// 获取配置
auto windowConfig = config.getWindowConfig();
auto perfConfig = config.getPerformanceConfig();
auto gpuConfig = config.getGPUConfig();

// 获取当前场景
ShaderScene scene = config.getActiveScene();
std::cout << "Vertex shader: " << scene.vertexShader << std::endl;
std::cout << "Fragment shader: " << scene.fragmentShader << std::endl;

// 切换场景（运行时）
config.setActiveScene("fractal");
```

## 📋 可用场景列表

当前配置的场景：

| 场景名 | 描述 | Fragment Shader |
|--------|------|----------------|
| rotation_matrix | 旋转矩阵盒子动画 | rotation_matrix.glsl |
| fractal | 3D分形光线追踪 | fragment.glsl |
| water | 水面模拟效果 | water.glsl |

## 🎯 最佳实践

1. **性能测试**：先用 `vsync: false` 测试最大性能
2. **正常使用**：使用 `vsync: true` 获得稳定体验
3. **调试**：启用 `show_console_fps` 查看详细信息
4. **发布**：禁用 `show_console_fps` 减少输出

## 🐛 故障排除

### 配置文件未找到

确保配置文件在正确位置：
```bash
ls build/config/shader_config.yaml
```

如果不存在，重新编译：
```bash
cd build && make
```

### Shader 未找到

确保 shader 路径正确：
- 路径相对于可执行文件目录
- 使用 `shaders/` 前缀
- 检查文件是否存在：`ls build/shaders/`

### YAML 解析错误

检查 YAML 语法：
- 使用空格缩进（不要用Tab）
- 确保冒号后有空格
- 字符串可以加引号或不加

## 📚 技术细节

### 依赖库

- **yaml-cpp**: YAML 解析库
  - 包: `libyaml-cpp-dev`
  - 安装: `sudo apt install libyaml-cpp-dev`

### 文件结构

```
Tiny-rasterizer/
├── config/
│   └── shader_config.yaml    # 主配置文件
├── include/
│   └── Config.h              # 配置类头文件
├── src/
│   ├── Config.cpp            # 配置类实现
│   └── main.cpp              # 使用配置
└── CMakeLists.txt            # 链接 yaml-cpp
```

### CMake 配置

在 `CMakeLists.txt` 中：
```cmake
find_package(yaml-cpp REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE yaml-cpp)
```

## 🎨 快速开始

1. **查看当前配置**:
   ```bash
   cat config/shader_config.yaml
   ```

2. **切换到分形场景**:
   编辑 `config/shader_config.yaml`，修改:
   ```yaml
   active_scene: "fractal"
   ```

3. **运行程序**:
   ```bash
   cd build && ./Tiny-rasterizer
   ```

4. **调整窗口大小**:
   修改配置文件中的 `width` 和 `height`

就这么简单！🎉
