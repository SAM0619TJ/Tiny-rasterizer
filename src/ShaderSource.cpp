#include "ShaderSource.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

std::string ShaderSource::loadFile(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open shader file: " + path);
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  std::string content = buffer.str();
  if (content.empty()) {
    throw std::runtime_error("Shader file is empty: " + path);
  }

  std::cout << "[resource] Read shader file: " << path << " ("
            << content.length() << " bytes)" << std::endl;
  return content;
}

ShaderSourcePair ShaderSource::loadPair(const std::string &vertexPath,
                                        const std::string &fragmentPath) {
  return {loadFile(vertexPath), loadFile(fragmentPath)};
}
