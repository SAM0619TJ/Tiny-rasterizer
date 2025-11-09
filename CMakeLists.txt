cmake_minimum_required(VERSION 3.10)
project(Tiny-rasterizer VERSION 1.0)


# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找必要的包
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(yaml-cpp REQUIRED)

# 设置源文件目录
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

# 收集源文件
file(GLOB_RECURSE SOURCE_FILES 
    "${SRC_DIR}/*.cpp"
    "${SRC_DIR}/*.c"
)

# 收集头文件
file(GLOB_RECURSE HEADER_FILES 
    "${INCLUDE_DIR}/*.h"
    "${INCLUDE_DIR}/*.hpp"
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

# 添加头文件搜索路径
target_include_directories(${PROJECT_NAME} PRIVATE
    ${INCLUDE_DIR}
    ${OPENGL_INCLUDE_DIR}
    ${GLEW_INCLUDE_DIRS}
)

# 链接依赖库
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${OPENGL_LIBRARIES}
    GLEW::GLEW
    glfw
    yaml-cpp
)

# 复制着色器文件到构建目录
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${SHADER_DIR}
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
    COMMENT "Copying shader files to build directory"
)

# 复制配置文件到构建目录
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/config
    COMMENT "Copying config files to build directory"
)

# 设置IDE中的源文件分组
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE_FILES} ${HEADER_FILES})

# 启用调试信息
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
endif()

# 设置编译选项
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 安装规则
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(DIRECTORY ${SHADER_DIR}/ DESTINATION bin/shaders)
