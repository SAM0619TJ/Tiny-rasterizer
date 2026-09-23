# GPU 性能与优化说明（Vulkan 后端）

本文档描述当前 Vulkan 后端的性能相关设置与已知优化点。
（OpenGL 时代的 `glDisable` 之类说明已随 Phase 8 移除。）

## 当前后端概况

| 项目 | 值 | 代码位置 |
| --- | --- | --- |
| API | Vulkan 1.1（MoltenVK on macOS） | `VulkanRenderer::Impl::createInstance` |
| 验证层 | 仅 Debug 构建启用 | `kEnableValidationLayers`（受 `DEBUG` 宏控制） |
| 帧并行度 | 2 帧 in flight | `kMaxFramesInFlight` |
| 呈现模式 | vsync 关闭优先 `MAILBOX`，开启用 `FIFO` | `choosePresentMode` |
| 交换链 image 数 | `minImageCount + 1`（受 maxImageCount 夹取） | `createSwapchain` |
| 采样数 | 固定 `VK_SAMPLE_COUNT_1_BIT`（MSAA 未实现） | `createFullscreenPipeline` |
| 渲染流程 | 离屏 pass → barrier → composite pass | `recordCommandBuffer` |

## 垂直同步与呈现模式

`window.vsync` 直接决定 present mode 的选择顺序：

- `vsync: false` → `MAILBOX`（无撕裂且不锁帧），不可用时退到 `IMMEDIATE`；
- `vsync: true` → `FIFO`（锁定刷新率），不可用时退到 `FIFO_RELAXED`。

启动日志会打印实际模式：`[vulkan] Swapchain created: ... | present=IMMEDIATE | vsync=off`。

## 已实现的性能相关设计

1. **动态视口/裁剪**：`VK_DYNAMIC_STATE_VIEWPORT` + `VK_DYNAMIC_STATE_SCISSOR`，resize 时无需重建管线。
2. **资源复用**：UBO 常驻映射（`uniformMapped`），每帧只做 `memcpy`，不重新分配、不重写描述符集。
3. **纹理上传走 staging**：CPU → staging buffer → `vkCmdCopyBufferToImage`，避免直接写 device-local 内存。
4. **一次性提交批处理**：纹理上传与 compute dispatch 走 `submitOneTimeCommands`，复用命令池并等待 fence。
5. **交换链重建按需**：`resize()` 只在尺寸真正变化时置脏标记，避免启动首帧的无谓重建。
6. **Compute 分担**：噪声纹理改由 GPU dispatch 生成（`shaders/grain.glsl`），省掉 CPU 端逐像素填充。

## 已知瓶颈与优化方向

| 现状 | 影响 | 建议 |
| --- | --- | --- |
| 顶点/UBO 使用 `HOST_VISIBLE \| HOST_COHERENT` 内存 | 每帧 CPU 写入带宽受限 | 改为 staging buffer + 每帧一次传输，或使用 device-local 内存 |
| 未使用 `vkCmdPushConstants` | 小参数也要走 UBO | 后处理参数（exposure/vignette/grain）可迁到 push constant |
| 未使用 RenderPass 缓存/动态渲染 | 管线与 pass 相对刚性 | 评估 `VK_KHR_dynamic_rendering` |
| 每个场景重建管线 | 切换场景有卡顿 | 启动时预创建全部场景管线并缓存 |
| 未启用 MSAA | 边缘锯齿明显 | 增加 `VkSampleCountFlagBits` 与 resolve attachment |
| 单线程录制命令缓冲 | 复杂场景 CPU 成为瓶颈 | 按帧分池并行录制 secondary command buffer |
| 无内存分配器 | 分配次数少、暂不构成问题 | 资源规模上升后引入 VMA 或自研分配器 |

## 测量方式

```bash
# Debug + 严格验证，观察 validation 数量与帧时
TINY_RASTERIZER_MAX_FRAMES=600 TINY_RASTERIZER_STRICT_VALIDATION=1 ./Tiny-rasterizer

# 压测：自动 resize + 切换后处理/场景，验证重建路径的稳定性
TINY_RASTERIZER_MAX_FRAMES=300 TINY_RASTERIZER_STRESS_TEST=1 ./Tiny-rasterizer
```

退出时会打印：

```
=== 验收基线 ===
启动耗时 / 运行时长 / 总帧数 / 平均帧时 / 最差帧时 / resize 次数 / validation 计数
```

## 跨平台注意点

- **macOS**：必须走 MoltenVK；实例需启用 `VK_KHR_portability_enumeration`（连同 `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR`），设备需启用 `VK_KHR_portability_subset`。实例 API 版本请求 1.1 以满足后者的依赖要求。
- **Windows**：使用官方 Vulkan Loader（LunarG SDK 或 `vulkan-1.dll`），无需 portability 扩展；present mode 支持范围通常更宽（`MAILBOX`/`IMMEDIATE` 均可）。
- 窗口层保持 GLFW + `GLFW_CLIENT_API=GLFW_NO_API`，两端一致。
