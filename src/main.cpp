#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include "Shader.h"
#include "Window.h"
#include "Config.h"

int main() 
{
    try {
        // 加载配置文件
        std::cout << "Loading configuration..." << std::endl;
        Config config("config/shader_config.yaml");
        
        // 获取配置
        const auto& windowConfig = config.getWindowConfig();
        const auto& perfConfig = config.getPerformanceConfig();
        const auto& gpuConfig = config.getGPUConfig();
        ShaderScene activeScene = config.getActiveScene();
        
        std::cout << "\n=== 配置信息 ===" << std::endl;
        std::cout << "场景: " << activeScene.name << std::endl;
        std::cout << "描述: " << activeScene.description << std::endl;
        std::cout << "顶点着色器: " << activeScene.vertexShader << std::endl;
        std::cout << "片段着色器: " << activeScene.fragmentShader << std::endl;
        std::cout << "窗口大小: " << windowConfig.width << "x" << windowConfig.height << std::endl;
        std::cout << "VSync: " << (windowConfig.vsync ? "开启" : "关闭") << std::endl;
        std::cout << "OpenGL: " << gpuConfig.openglMajor << "." << gpuConfig.openglMinor << std::endl;
        std::cout << "=================\n" << std::endl;
        
        // 初始化GLFW
        Window::initGLFW();
        
        // 设置GPU配置
        Window::setGPUConfig(gpuConfig);

        // 创建窗口（使用配置）
        Window window(windowConfig);

        // 创建着色器程序（使用配置的shader路径）
        std::cout << "Loading shaders..." << std::endl;
        Shader shader(activeScene.vertexShader, activeScene.fragmentShader, true);
        std::cout << "Shaders loaded successfully." << std::endl;
        shader.setupQuad();
        
        std::cout << "GPU acceleration enabled. Starting render loop...\n" << std::endl;

        // FPS 计数器变量（使用配置的更新间隔）
        double lastTime = glfwGetTime();
        double lastFrameTime = lastTime;
        int frameCount = 0;
        double fpsUpdateInterval = perfConfig.fpsUpdateInterval;
        
        // 性能统计
        double minFrameTime = 999999.0;
        double maxFrameTime = 0.0;

        // 主循环
        while (!window.shouldClose()) {
            // 清除颜色缓冲
            glClear(GL_COLOR_BUFFER_BIT);

            // 使用着色器程序
            shader.use();

            // 更新uniform变量
            float currentTime = glfwGetTime();
            int width, height;
            window.getFramebufferSize(width, height);
            
            shader.setFloat("iTime", currentTime);
            shader.setVec2("iResolution", static_cast<float>(width), static_cast<float>(height));

            // 获取鼠标位置
            double xpos, ypos;
            window.getCursorPos(xpos, ypos);
            shader.setVec2("iMouse", static_cast<float>(xpos), static_cast<float>(ypos));

            // 绘制四边形
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            // 交换缓冲并处理事件
            window.swapBuffers();
            window.pollEvents();

            // 计算并更新FPS
            frameCount++;
            double currentFrameTime = glfwGetTime();
            double totalDeltaTime = currentFrameTime - lastTime;
            double frameDeltaTime = currentFrameTime - lastFrameTime;
            
            // 每帧计算帧时间用于统计（毫秒）
            if (frameCount > 1) {  // 跳过第一帧
                double singleFrameMs = frameDeltaTime * 1000.0;
                if (singleFrameMs < minFrameTime) minFrameTime = singleFrameMs;
                if (singleFrameMs > maxFrameTime) maxFrameTime = singleFrameMs;
            }
            
            lastFrameTime = currentFrameTime;
            
            // 定期更新FPS显示
            if (totalDeltaTime >= fpsUpdateInterval) {
                double fps = frameCount / totalDeltaTime;
                double avgMs = (totalDeltaTime * 1000.0) / frameCount;
                
                // 根据配置决定是否更新窗口标题
                if (perfConfig.showTitleFps) {
                    std::ostringstream title;
                    title << windowConfig.title << " [GPU] | FPS: " << std::fixed << std::setprecision(1) 
                          << fps << " | Avg: " << std::setprecision(2) << avgMs << "ms";
                    
                    // 只有当有有效数据时才显示最小/最大值
                    if (minFrameTime < 999999.0 && maxFrameTime > 0.0) {
                        title << " | Min: " << std::setprecision(0) << (1000.0 / maxFrameTime) << "fps"
                              << " | Max: " << std::setprecision(0) << (1000.0 / minFrameTime) << "fps";
                    }
                    
                    window.setTitle(title.str());
                }
                
                // 根据配置决定是否输出到终端
                if (perfConfig.showConsoleFps) {
                    std::cout << "FPS: " << std::fixed << std::setprecision(1) << fps 
                              << " | Avg: " << std::setprecision(2) << avgMs << "ms" << std::endl;
                }
                
                // 重置计数器
                frameCount = 0;
                lastTime = currentFrameTime;
                minFrameTime = 999999.0;
                maxFrameTime = 0.0;
            }
        }

        // Window的析构函数会自动清理窗口资源
        // Shader的析构函数会自动清理VAO和VBO
        Window::terminateGLFW();
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        Window::terminateGLFW();
        return -1;
    }
}

