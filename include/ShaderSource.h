#ifndef SHADER_SOURCE_H
#define SHADER_SOURCE_H

#include <string>

struct ShaderSourcePair {
  std::string vertex;
  std::string fragment;
};

class ShaderSource {
public:
  static std::string loadFile(const std::string &path);
  static ShaderSourcePair loadPair(const std::string &vertexPath,
                                   const std::string &fragmentPath);
};

#endif // SHADER_SOURCE_H
