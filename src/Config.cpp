#include "Config.h"
#include <iostream>
#include <stdexcept>

Config::Config() {
    // 默认配置
    activeScene = "rotation_matrix";
}

Config::Config(const std::string& configPath) {
    if (!load(configPath)) {
        throw std::runtime_error("Failed to load config file: " + configPath);
    }
}

bool Config::load(const std::string& configPath) {
    try {
        YAML::Node config = YAML::LoadFile(configPath);
        
        // 先加载场景定义
        loadScenes(config);
        
        // 加载激活场景
        if (config["active_scene"]) {
            std::string sceneIdentifier = config["active_scene"].as<std::string>();
            
            // 尝试作为 key 查找
            if (scenes.find(sceneIdentifier) != scenes.end()) {
                activeScene = sceneIdentifier;
            } else {
                // 尝试作为 name 查找
                bool found = false;
                for (const auto& pair : scenes) {
                    if (pair.second.name == sceneIdentifier) {
                        activeScene = pair.first;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    std::cerr << "Warning: Scene '" << sceneIdentifier << "' not found, using default" << std::endl;
                    if (!scenes.empty()) {
                        activeScene = scenes.begin()->first;
                    }
                }
            }
        }
        
        loadWindowConfig(config);
        loadPerformanceConfig(config);
        loadGPUConfig(config);
        
        std::cout << "Config loaded successfully from: " << configPath << std::endl;
        std::cout << "Active scene: " << activeScene;
        if (scenes.find(activeScene) != scenes.end()) {
            std::cout << " (" << scenes[activeScene].name << ")";
        }
        std::cout << std::endl;
        
        return true;
    }
    catch (const YAML::Exception& e) {
        std::cerr << "YAML parsing error: " << e.what() << std::endl;
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << "Config loading error: " << e.what() << std::endl;
        return false;
    }
}

void Config::loadScenes(const YAML::Node& config) {
    if (!config["scenes"]) {
        std::cerr << "Warning: No scenes defined in config" << std::endl;
        return;
    }
    
    const YAML::Node& scenesNode = config["scenes"];
    for (YAML::const_iterator it = scenesNode.begin(); it != scenesNode.end(); ++it) {
        std::string sceneName = it->first.as<std::string>();
        const YAML::Node& sceneNode = it->second;
        
        ShaderScene scene;
        scene.name = sceneNode["name"] ? sceneNode["name"].as<std::string>() : sceneName;
        scene.description = sceneNode["description"] ? sceneNode["description"].as<std::string>() : "";
        scene.vertexShader = sceneNode["vertex_shader"] ? sceneNode["vertex_shader"].as<std::string>() : "";
        scene.fragmentShader = sceneNode["fragment_shader"] ? sceneNode["fragment_shader"].as<std::string>() : "";
        
        scenes[sceneName] = scene;
        std::cout << "Loaded scene: " << sceneName << " (" << scene.name << ")" << std::endl;
    }
}

void Config::loadWindowConfig(const YAML::Node& config) {
    if (!config["window"]) {
        return;
    }
    
    const YAML::Node& window = config["window"];
    if (window["width"]) windowConfig.width = window["width"].as<int>();
    if (window["height"]) windowConfig.height = window["height"].as<int>();
    if (window["title"]) windowConfig.title = window["title"].as<std::string>();
    if (window["vsync"]) windowConfig.vsync = window["vsync"].as<bool>();
}

void Config::loadPerformanceConfig(const YAML::Node& config) {
    if (!config["performance"]) {
        return;
    }
    
    const YAML::Node& perf = config["performance"];
    if (perf["fps_update_interval"]) 
        perfConfig.fpsUpdateInterval = perf["fps_update_interval"].as<double>();
    if (perf["show_console_fps"]) 
        perfConfig.showConsoleFps = perf["show_console_fps"].as<bool>();
    if (perf["show_title_fps"]) 
        perfConfig.showTitleFps = perf["show_title_fps"].as<bool>();
}

void Config::loadGPUConfig(const YAML::Node& config) {
    if (!config["gpu"]) {
        return;
    }
    
    const YAML::Node& gpu = config["gpu"];
    if (gpu["opengl_major"]) gpuConfig.openglMajor = gpu["opengl_major"].as<int>();
    if (gpu["opengl_minor"]) gpuConfig.openglMinor = gpu["opengl_minor"].as<int>();
    if (gpu["samples"]) gpuConfig.samples = gpu["samples"].as<int>();
}

ShaderScene Config::getActiveScene() const {
    auto it = scenes.find(activeScene);
    if (it != scenes.end()) {
        return it->second;
    }
    throw std::runtime_error("Active scene not found: " + activeScene);
}

void Config::setActiveScene(const std::string& sceneName) {
    if (scenes.find(sceneName) != scenes.end()) {
        activeScene = sceneName;
        std::cout << "Switched to scene: " << sceneName << std::endl;
    } else {
        std::cerr << "Scene not found: " << sceneName << std::endl;
    }
}
