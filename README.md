# Tiny-rasterizer

迷你光栅化渲染器学习项目，当前实现基于 OpenGL。

## Vulkan 重构路线图

本项目将从 OpenGL 渐进迁移到 Vulkan，目标是最终仅保留 Vulkan 渲染路径。

### 重构目标

1. 仅保留 Vulkan 后端，不再维护 OpenGL 双后端。
2. 分阶段迁移，每个阶段都可编译、可运行、可验收。
3. 先稳定 macOS (MoltenVK)，再收敛 Windows。
4. 保留现有场景能力，并补齐纹理与后处理。
5. 预留 Compute 扩展入口。

### 技术决策

1. 窗口系统继续使用 GLFW。
2. Vulkan 实现尽量使用原生 Vulkan C API（暂不引入 Volk/VMA）。
3. Shader 采用双模式：
	- 离线编译：GLSL -> SPIR-V（构建阶段）
	- 开发模式：运行时编译（便于迭代）

## 分阶段实施

### Phase 0 - 基线冻结与可观测性

1. 冻结当前 OpenGL 行为作为迁移对照基线。
2. 定义验收指标：启动成功率、平均帧时、resize 稳定性、场景一致性。
3. 增加渲染日志分层（init/frame/resource/shader）。

### Phase 1 - 架构解耦（后续阶段前置条件）

1. 从主循环抽离渲染后端接口（init/beginFrame/draw/endFrame/resize/shutdown）。
2. Window 仅保留 GLFW 生命周期与输入/窗口事件，移除 OpenGL context 职责。
3. Shader 拆分为资源加载与参数描述层，解除对 OpenGL Program 的直接耦合。
4. 统一每帧参数结构（time/resolution/mouse），业务层禁止直接调用图形 API。

### Phase 2 - 构建系统切换 Vulkan

1. CMake 依赖改为 Vulkan：`find_package(Vulkan REQUIRED)`。
2. 移除 OpenGL/GLEW 链接。
3. macOS 使用 MoltenVK，Windows 使用标准 Vulkan Loader。
4. 保留 GLFW，但改为 Vulkan Surface 模式。
5. 增加 shader 编译任务（离线 SPIR-V + 开发期运行时编译）。

### Phase 3 - Vulkan 核心初始化

1. 建立 VulkanContext：Instance / Validation Layers / Debug Messenger。
2. 建立 Device 模块：PhysicalDevice / Queue Family / Logical Device。
3. 建立 Surface + Swapchain：格式选择、present mode、image views、resize 重建。
4. 建立帧同步：fence + acquire/present semaphores（至少双缓冲）。
5. 阶段验收：可稳定清屏并 present。

### Phase 4 - 最小可绘制管线（全屏四边形）

1. 建立 RenderPass + Framebuffer + GraphicsPipeline。
2. 用 Vulkan VertexBuffer 替换 VAO/VBO。
3. 完整命令缓冲录制：begin -> bind -> draw -> end。
4. 引入 UBO + DescriptorSet，替换 glUniform 直写。

### Phase 5 - 场景与材质兼容

1. 升级 `config/shader_config.yaml`，支持 GLSL/SPIR-V 双路径。
2. 新增 ShaderManager，缓存 `VkShaderModule` 并支持开发期热重载。
3. 保持 rotation_matrix/fractal/water 三场景可切换且效果对齐。

### Phase 6 - 纹理与后处理

1. 增加纹理上传链路：staging buffer -> image -> layout transition。
2. 新增后处理 pass：离屏渲染 + 全屏合成。
3. 在配置层提供后处理开关与参数。

### Phase 7 - Compute 扩展预留（长期）

1. 预留 ComputeContext（descriptor/pipeline/dispatch）。
2. 将高开销效果抽象为可迁移 compute 的任务接口。
3. 首个里程碑不强制启用 compute，仅保证同步点和架构扩展位。

### Phase 8 - 跨平台收敛与 OpenGL 移除

1. macOS 先稳定：重点验证 swapchain 重建与 validation 清零。
2. 再收敛 Windows：驱动差异、present mode 差异、shader 链路一致性。
3. 移除 OpenGL/GLEW 代码和配置遗留项。

## 验收标准

1. 每阶段均可 configure + build，并保持可运行。
2. Phase 3：可稳定 present 清屏 5 分钟，无 validation error。
3. Phase 4：全屏四边形绘制成功，time/resolution/mouse 参数生效。
4. Phase 5：rotation_matrix/fractal/water 可切换且稳定。
5. Phase 6：纹理加载正确，后处理开关生效，resize 后无错帧。
6. Phase 8：移除 OpenGL 后，macOS 与 Windows 启动/渲染/退出全流程通过。

## 迁移里程碑建议

1. Milestone A：完成 Phase 1-3（Vulkan 可初始化并 present）。
2. Milestone B：完成 Phase 4-5（可绘制并恢复核心场景）。
3. Milestone C：完成 Phase 6-8（纹理后处理、平台收敛、彻底移除 OpenGL）。
