#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
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

