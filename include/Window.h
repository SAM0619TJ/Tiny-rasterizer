#ifndef WINDOW_H
#define WINDOW_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>

class Window {
public:
    Window(int width, int height, const std::string& title);
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

    // 静态方法
    static void initGLFW();
    static void terminateGLFW();

private:
    GLFWwindow* window;
    bool initialized;

    void setupWindow(int width, int height, const std::string& title);
    void initGLEW();
};

#endif // WINDOW_H