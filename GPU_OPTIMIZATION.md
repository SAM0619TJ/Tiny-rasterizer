# GPU 加速优化说明

## ✅ 已实现的优化

### 1. GPU 硬件加速

- **当前GPU**: Intel(R) UHD Graphics
- **OpenGL版本**: 4.1 Core Profile
- **渲染API**: Direct3D 12 (通过 Mesa)
- **着色器语言**: GLSL 4.10

### 2. 性能优化设置

#### OpenGL 状态优化

```cpp
glDisable(GL_DEPTH_TEST);    // 2D渲染不需要深度测试
glDisable(GL_STENCIL_TEST);  // 不需要模板测试
glDisable(GL_BLEND);         // 不需要混合（除非有透明度）
glDisable(GL_CULL_FACE);     // 不需要面剔除
```

#### 窗口配置优化

- **双缓冲**: 启用（防止画面撕裂）
- **多重采样**: 禁用（提升性能）
- **刷新率**: 使用系统最高刷新率
- **VSync**: 可配置（见下文）

### 3. VSync 控制

当前设置: `glfwSwapInterval(0)` - 关闭VSync

- **VSync 关闭 (0)**: 
  - ✅ 获得最大FPS（可能>1000fps）
  - ❌ 可能出现画面撕裂
  - 适合：性能测试

- **VSync 开启 (1)**:
  - ✅ 稳定60fps（或显示器刷新率）
  - ✅ 无画面撕裂
  - ❌ FPS被限制
  - 适合：正常使用

### 4. 性能监控

窗口标题显示：
```
Tiny Rasterizer [GPU] | FPS: 60.0 | Avg: 16.67ms | Min: 55fps | Max: 65fps
```

- **FPS**: 平均每秒帧数
- **Avg**: 平均帧时间
- **Min/Max**: 最低/最高FPS

## 🎯 GPU 能力

当前系统支持：
- 最大纹理尺寸: 16384x16384
- 最大顶点属性: 16
- GLSL版本: 4.10

## 🚀 进一步优化建议

### 1. Shader优化
```glsl
// 使用更高效的数学函数
- 用 fma() 代替 a*b+c
- 用 inversesqrt() 代替 1.0/sqrt()
- 减少条件分支（if/else）
```

### 2. 几何优化
```cpp
// 当前使用 TRIANGLE_STRIP（已是最优）
glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
```

### 3. 使用独立显卡（如果有）
```bash
# NVIDIA
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia ./Tiny-rasterizer

# AMD
DRI_PRIME=1 ./Tiny-rasterizer
```

### 4. 计算着色器（Compute Shader）
对于复杂的 raymarching，可以考虑使用 Compute Shader 进行并行计算。

## 📊 性能基准

| 分辨率 | 预期FPS (VSync Off) | 预期FPS (VSync On) |
|--------|--------------------|--------------------|
| 800x600 | 500-1000+ fps | 60 fps |
| 1920x1080 | 200-500 fps | 60 fps |
| 2560x1440 | 100-300 fps | 60 fps |

*实际性能取决于GPU性能和shader复杂度*

## 🔧 调试技巧

### 查看GPU使用率
```bash
# Intel GPU
intel_gpu_top

# NVIDIA GPU
nvidia-smi -l 1

# AMD GPU
radeontop
```

### 性能分析工具
- **RenderDoc**: GPU调试和性能分析
- **apitrace**: OpenGL调用跟踪
- **perf**: CPU性能分析

## ⚙️ 配置文件修改

如需切换VSync，修改 `Window.cpp`:
```cpp
glfwSwapInterval(0);  // 0=关闭, 1=开启
```

然后重新编译：
```bash
cd build && make && ./Tiny-rasterizer
```
