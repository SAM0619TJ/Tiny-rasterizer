#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>
#include <yaml-cpp/yaml.h>

// 着色器场景配置
struct ShaderScene {
    std::string name;
    std::string description;
    std::string vertexShader;
    std::string fragmentShader;
};

// 窗口配置
struct WindowConfig {
    int width = 1000;
    int height = 600;
    std::string title = "Tiny Rasterizer";
    bool vsync = false;
};

// 性能配置
struct PerformanceConfig {
    double fpsUpdateInterval = 0.5;
    bool showConsoleFps = true;
    bool showTitleFps = true;
};

// GPU配置
struct GPUConfig {
    int openglMajor = 4;
    int openglMinor = 1;
    int samples = 0;
};

// 主配置类
class Config {
public:
    Config();
    explicit Config(const std::string& configPath);
    
    // 加载配置文件
    bool load(const std::string& configPath);
    
    // 获取当前激活的场景
    ShaderScene getActiveScene() const;
    
    // 获取配置
    const WindowConfig& getWindowConfig() const { return windowConfig; }
    const PerformanceConfig& getPerformanceConfig() const { return perfConfig; }
    const GPUConfig& getGPUConfig() const { return gpuConfig; }
    
    // 获取所有场景
    const std::map<std::string, ShaderScene>& getAllScenes() const { return scenes; }
    
    // 设置激活场景
    void setActiveScene(const std::string& sceneName);
    
    // 获取激活场景名称
    std::string getActiveSceneName() const { return activeScene; }
    
private:
    std::string activeScene;
    std::map<std::string, ShaderScene> scenes;
    WindowConfig windowConfig;
    PerformanceConfig perfConfig;
    GPUConfig gpuConfig;
    
    void loadScenes(const YAML::Node& config);
    void loadWindowConfig(const YAML::Node& config);
    void loadPerformanceConfig(const YAML::Node& config);
    void loadGPUConfig(const YAML::Node& config);
};

#endif // CONFIG_H
