# Debugging Information

## 2026-09-23

按 README 的 Phase 推进，新增验收门后立即暴露的问题：

- **VUID-vkCreateDevice-ppEnabledExtensionNames-01387**：启用 `VK_KHR_portability_subset` 时
  未同时启用 `VK_KHR_get_physical_device_properties2`。原因是实例 API 版本请求 1.0，
  该扩展未提升为核心；MoltenVK 也不把它列为设备扩展，因此原来的 `hasDeviceExtension` 判断失效。
  修复：用 `vkEnumerateInstanceVersion` 探测后请求 1.1（上限 1.1），使该功能由核心提供。
  顺带消除了 MoltenVK 的 `vkGetPhysicalDeviceProperties2KHR: Emulation found unrecognized structure type` 警告。
- **启动首帧多余重建交换链**：主循环的 `lastWidth/lastHeight` 初值为 0，首帧必然触发一次
  `resize()` → `recreateSwapchain()`。修复：`resize()` 在尺寸与当前 extent 相同时直接返回。
- 结论：把「验证层 error 计数 + 严格退出码」做成常规验收门，比人工看日志有效得多。

## 2025-11-07

bug修复和调试信息：

- 修复了着色器加载时的错误处理。main中两个嵌套的try块导致逻辑错误，Shader的加载和渲染被跳过。(重构异常处理结构)  
- 优化了窗口类的初始化流程  
- 添加了OpenGL版本和GLSL版本的打印信息，方便调试环境问题
- 在Shader类的问题setupQuad()后解绑了VAO,但渲染时没有重新绑定
- 新加了几个有意思的shader。
- 新增了旋转矩阵着色器，演示了如何在GLSL中实现2D旋转效果。

XOU
