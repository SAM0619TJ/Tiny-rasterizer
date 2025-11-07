# Debugging Information

## 2025-11-07

bug修复和调试信息：

- 修复了着色器加载时的错误处理。main中两个嵌套的try块导致逻辑错误，Shader的加载和渲染被跳过。(重构异常处理结构)  
- 优化了窗口类的初始化流程  
- 添加了OpenGL版本和GLSL版本的打印信息，方便调试环境问题
- 在Shader类的问题setupQuad()后解绑了VAO,但渲染时没有重新绑定

