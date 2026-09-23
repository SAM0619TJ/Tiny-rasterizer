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

// 着色器加载配置（双路径 + 热重载）
struct ShaderConfig {
    bool runtimeCompile = false;          // 开发模式：运行时用 glslc 编译 GLSL
    bool hotReload = false;               // 开发模式：检测源文件改动自动重载
    std::string spirvDir = "shaders_spirv"; // 离线 SPIR-V 目录
};

// 颗粒纹理来源：配置即规则，不在渲染器内做隐式优先级判断。
enum class TextureSource {
    File,       // 从 post_processing.texture 指定的 PPM/TGA 文件加载
    Compute,    // 用 compute shader 在 GPU 生成
    Procedural, // CPU 端程序化生成
};

// 后处理配置（离屏渲染 + 全屏合成）
struct PostProcessingConfig {
    bool enabled = true;     // 是否启用后处理效果（关闭则直通）
    float exposure = 1.0f;   // 曝光倍数
    float vignette = 0.3f;   // 暗角强度
    float grain = 0.05f;     // 颗粒强度
    TextureSource textureSource = TextureSource::File;
    std::string texturePath; // textureSource=File 时的 PPM/TGA 路径
};

// Compute 配置（Phase 7 预留：首个任务为 GPU 生成噪声纹理）
struct ComputeConfig {
    bool enabled = true;                            // 关闭则不用 compute 路径
    std::string grainShader = "shaders/grain.glsl";  // compute 着色器源
    int textureSize = 256;                          // 生成纹理边长
    int seed = 1;                                   // 噪声种子
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
    const ShaderConfig& getShaderConfig() const { return shaderConfig; }
    const PostProcessingConfig& getPostProcessingConfig() const { return postConfig; }
    const ComputeConfig& getComputeConfig() const { return computeConfig; }
    
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
    ShaderConfig shaderConfig;
    PostProcessingConfig postConfig;
    ComputeConfig computeConfig;
    
    void loadScenes(const YAML::Node& config);
    void loadWindowConfig(const YAML::Node& config);
    void loadPerformanceConfig(const YAML::Node& config);
    void loadGPUConfig(const YAML::Node& config);
    void loadShaderConfig(const YAML::Node& config);
    void loadPostProcessingConfig(const YAML::Node& config);
    void loadComputeConfig(const YAML::Node& config);
};

#endif // CONFIG_H
