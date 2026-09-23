# 配置系统使用指南（Vulkan 后端）

配置文件位置：`config/shader_config.yaml`（可用命令行参数覆盖：`./Tiny-rasterizer <path>`）。

## 配置项总览

| 配置段 | 作用 | 相关阶段 |
| --- | --- | --- |
| `active_scene` / `scenes` | 场景与着色器路径 | Phase 5 |
| `window` | 窗口尺寸、标题、vsync（映射到 present mode） | Phase 3 |
| `performance` | FPS 输出频率与显示位置 | Phase 0 |
| `shader` | GLSL/SPIR-V 双路径与热重载 | Phase 5 |
| `post_processing` | 离屏合成效果与颗粒纹理来源 | Phase 6 |
| `compute` | 是否用 compute 生成噪声纹理 | Phase 7 |

### 1. 场景切换

```yaml
active_scene: "rotation_matrix"  # 可用 key 或场景 name，也可为 "fractal" / "water"

scenes:
  rotation_matrix:
    name: "Rotation Matrix Effect"
    description: "Box rotation with time-based animation"
    vertex_shader: "shaders/vertex.glsl"
    fragment_shader: "shaders/rotation_matrix.glsl"
```

运行时可直接用数字键 `1..N` 切换场景（会重建图形管线），按 `P` 切换后处理开关。

### 2. 窗口与垂直同步

```yaml
window:
  width: 1000
  height: 600
  title: "Tiny Rasterizer"
  vsync: false     # false -> 优先 MAILBOX（低延迟），true -> FIFO（锁刷新率）
```

`vsync` 现在真正生效：它决定 `VkPresentModeKHR` 的选择顺序，实际选中的模式会打印在启动日志里。

### 3. 性能输出

```yaml
performance:
  fps_update_interval: 0.5  # 统计输出间隔（秒）
  show_console_fps: true
  show_title_fps: true
```

程序退出时会输出验收基线：启动耗时、总帧数、平均/最差帧时、resize 次数、validation error/warning 计数。

### 4. 着色器加载（双路径 + 热重载）

```yaml
shader:
  runtime_compile: false       # true=运行时用 glslc 编译 GLSL，false=加载离线 SPIR-V
  hot_reload: false            # true=源文件变化后自动重建管线
  spirv_dir: "shaders_spirv"   # 离线 SPIR-V 目录（相对可执行文件）
```

- 离线模式：构建期由 `glslc` 生成 `shaders_spirv/<stem>.spv`。
- 运行时模式：`ShaderManager` 调用 `glslc` 产出 `<stem>.dev.spv`，失败自动回退离线 SPIR-V。

### 5. 后处理（离屏 + 全屏合成）

```yaml
post_processing:
  enabled: true     # false 时合成 pass 直通
  exposure: 1.05
  vignette: 0.35
  grain: 0.05
  texture_source: "file"          # file | compute | procedural
  texture: "textures/grain.ppm"   # texture_source=file 时使用
```

颗粒纹理来源由 `texture_source` 显式指定，没有隐式优先级：

| 取值 | 含义 | 不可用时的行为 |
| --- | --- | --- |
| `file` | 从 `texture` 指定的 PPM(P3/P6)/TGA(2/3/10/11) 加载 | 加载失败告警并降级到程序化噪声 |
| `compute` | 用 `shaders/grain.glsl` 在 GPU 生成 | `compute.enabled=false` 或设备无同族 compute 队列时降级 |
| `procedural` | CPU 端 `makeNoise()` 生成 | 不依赖外部资源 |

### 6. Compute（Phase 7）

```yaml
compute:
  enabled: true
  grain_shader: "shaders/grain.glsl"
  texture_size: 256
  seed: 1
```

`texture_source: compute` 与本段的 `enabled` 同时满足时才对 dispatch；否则按上表降级并打印告警。

## 使用示例

### 示例 1：最大性能（关垂直同步、关后处理）

```yaml
window:
  width: 1600
  height: 900
  vsync: false

performance:
  fps_update_interval: 0.2
  show_console_fps: true

post_processing:
  enabled: false
```

### 示例 2：效果演示（曝光 + 暗角 + 颗粒，纹理走 Compute）

```yaml
window:
  vsync: true

post_processing:
  enabled: true
  exposure: 1.1
  vignette: 0.4
  grain: 0.08
  texture_source: "compute"

compute:
  enabled: true
  texture_size: 512
```

### 示例 3：着色器迭代（运行时编译 + 热重载）

```yaml
shader:
  runtime_compile: true
  hot_reload: true
```

## 命令行与入口选项

所有 argv/环境变量都在 `include/AppOptions.h` 里解析，渲染器只读配置，不感知测试模式。

```bash
./Tiny-rasterizer [config-path]                          # 指定配置文件

# 环境变量（自动化验收用）
TINY_RASTERIZER_MAX_FRAMES=180          # 跑固定帧数后退出（超过 0 时窗口自动隐藏）
TINY_RASTERIZER_HEADLESS_TEST=1         # 仅初始化核心对象，不创建窗口
TINY_RASTERIZER_STRESS_TEST=1           # 自动 resize + 切换后处理/场景
TINY_RASTERIZER_STRICT_VALIDATION=1     # 有 validation error 时以退出码 2 结束
TINY_RASTERIZER_FORCE_COMPUTE_TEXTURE=1 # 把 texture_source 覆盖为 compute
```

`FORCE_COMPUTE_TEXTURE` 的语义是“投影到配置层”：它等价于在 yaml 里写
`texture_source: "compute"` 且 `compute.enabled: true`，不会在渲染器里引入测试分支。

## 代码中使用配置

```cpp
#include "Config.h"

Config config("config/shader_config.yaml");

const WindowConfig &window = config.getWindowConfig();
const ShaderConfig &shader = config.getShaderConfig();
const PostProcessingConfig &post = config.getPostProcessingConfig();
const ComputeConfig &compute = config.getComputeConfig();
const ShaderScene scene = config.getActiveScene();
```
