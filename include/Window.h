#ifndef WINDOW_H
#define WINDOW_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>
#include "Config.h"

class Window {
public:
    Window(int width, int height, const std::string& title);
    Window(const WindowConfig& config);  // 新增：从配置创建
    ~Window();

    // 禁止拷贝
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // 窗口状态检查
    bool shouldClose() const;
    bool isValid() const { return window != nullptr; }

    // 窗口操作
    void makeContextCurrent();
    void swapBuffers();
    void pollEvents();

    // 获取窗口属性
    void getFramebufferSize(int& width, int& height) const;
    void getCursorPos(double& xpos, double& ypos) const;
    GLFWwindow* getGLFWwindow() const { return window; }
    
    // 设置窗口标题
    void setTitle(const std::string& title);

    // 静态方法
    static void initGLFW();
    static void terminateGLFW();
    static void setGPUConfig(const GPUConfig& config);  // 新增：设置GPU配置

private:
    GLFWwindow* window;
    bool initialized;
    static GPUConfig gpuConfig;  // 新增：存储GPU配置

    void setupWindow(int width, int height, const std::string& title);
    void initGLEW();
};

#endif // WINDOW_H