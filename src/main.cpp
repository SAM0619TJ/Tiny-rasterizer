#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include "Shader.h"
#include "Window.h"

int main() 
{
    try {
        // 初始化GLFW
        Window::initGLFW();

        // 创建窗口
        Window window(800, 600, "Tiny Rasterizer");

        // 创建着色器程序并设置顶点数据
        std::cout << "Loading shaders..." << std::endl;
        Shader shader("shaders/vertex.glsl", "shaders/rotation_matrix.glsl", true);
        std::cout << "Shaders loaded successfully." << std::endl;
        shader.setupQuad();
        
        std::cout << "GPU acceleration enabled. Starting render loop...\n" << std::endl;

        // FPS 计数器变量
        double lastTime = glfwGetTime();
        double lastFrameTime = lastTime;
        int frameCount = 0;
        double fpsUpdateInterval = 0.5; // 每0.5秒更新一次FPS显示
        
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
                
                std::ostringstream title;
                title << "Tiny Rasterizer [GPU] | FPS: " << std::fixed << std::setprecision(1) 
                      << fps << " | Avg: " << std::setprecision(2) << avgMs << "ms";
                
                // 只有当有有效数据时才显示最小/最大值
                if (minFrameTime < 999999.0 && maxFrameTime > 0.0) {
                    title << " | Min: " << std::setprecision(0) << (1000.0 / maxFrameTime) << "fps"
                          << " | Max: " << std::setprecision(0) << (1000.0 / minFrameTime) << "fps";
                }
                
                window.setTitle(title.str());
                
                // 调试输出到终端
                // std::cout << "FPS: " << std::fixed << std::setprecision(1) << fps 
                //           << " | Avg: " << std::setprecision(2) << avgMs << "ms" << std::endl;
                
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

