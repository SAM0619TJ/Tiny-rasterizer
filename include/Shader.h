#ifndef SHADER_H
#define SHADER_H

#include <GL/glew.h>
#include <map>
#include <string>

class Shader {
public:
  // 构造函数，接受顶点着色器和片段着色器的源码
  Shader(const std::string &vertexSource, const std::string &fragmentSource);
  ~Shader();

  // 顶点数据设置
  void setupQuad(); // 设置四边形的顶点数据
  GLuint getVAO() const { return vao; }
  GLuint getVBO() const { return vbo; }

  // 使用/激活程序
  void use() const;

  // uniform工具函数
  void setFloat(const std::string &name, float value);
  void setInt(const std::string &name, int value);
  void setVec2(const std::string &name, float x, float y);
  void setVec3(const std::string &name, float x, float y, float z);
  void setVec4(const std::string &name, float x, float y, float z, float w);

  // 获取程序ID
  GLuint getID() const { return programID; }

private:
  GLuint programID;
  GLuint vao; // 顶点数组对象
  GLuint vbo; // 顶点缓冲对象
  std::map<std::string, GLint> uniformLocationCache;

  // 编译shader
  GLuint compileShader(GLenum type, const std::string &source);
  // 检查shader编译错误
  void checkCompileErrors(GLuint shader, const std::string &type);
  // 检查程序链接错误
  void checkLinkErrors();
  // 获取uniform位置（使用缓存）
  GLint getUniformLocation(const std::string &name);
};

#endif // SHADER_H
