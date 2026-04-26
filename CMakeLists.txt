cmake_minimum_required(VERSION 3.10)

# 如果缓存里残留了 Windows 工具链路径，在非 Windows 平台上先清理掉
if(NOT WIN32 AND DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "^[A-Za-z]:/")
    message(STATUS "Detected Windows toolchain in cache on non-Windows host. Clearing CMAKE_TOOLCHAIN_FILE.")
    unset(CMAKE_TOOLCHAIN_FILE CACHE)
endif()

# Windows 下如果配置了 VCPKG_ROOT，则自动使用 vcpkg 工具链
if(WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "vcpkg toolchain file")
endif()

project(Tiny-rasterizer VERSION 1.0)

# macOS 下追加 Homebrew 前缀，避免覆盖用户已有的 CMAKE_PREFIX_PATH
if(APPLE AND EXISTS "/opt/homebrew")
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
endif()

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 查找必要的包
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(yaml-cpp REQUIRED)

# 设置源文件目录
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SPIRV_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders_spirv)
get_filename_component(VULKAN_LIBRARY_DIR "${Vulkan_LIBRARY}" DIRECTORY)
get_filename_component(VULKAN_SDK_ROOT "${VULKAN_LIBRARY_DIR}" DIRECTORY)
get_property(VULKAN_INCLUDE_DIR_FROM_CACHE CACHE Vulkan_INCLUDE_DIR PROPERTY VALUE)

set(SOURCE_FILES
    "${SRC_DIR}/Config.cpp"
    "${SRC_DIR}/main.cpp"
    "${SRC_DIR}/VulkanRenderer.cpp"
    "${SRC_DIR}/Window.cpp"
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
    ${Vulkan_INCLUDE_DIR}
    ${Vulkan_INCLUDE_DIRS}
    ${VULKAN_INCLUDE_DIR_FROM_CACHE}
    "${VULKAN_SDK_ROOT}/include"
)

# 链接依赖库
set(YAML_CPP_TARGET yaml-cpp)
if(TARGET yaml-cpp::yaml-cpp)
    set(YAML_CPP_TARGET yaml-cpp::yaml-cpp)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    ${YAML_CPP_TARGET}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE)

find_program(GLSLC_EXECUTABLE
    NAMES glslc
    HINTS ${Vulkan_GLSLC_EXECUTABLE}
)

if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found. Install the Vulkan SDK to enable GLSL -> SPIR-V compilation.")
endif()

function(add_spirv_shader INPUT_FILE SHADER_STAGE OUTPUT_NAME)
    set(OUTPUT_FILE "${SPIRV_DIR}/${OUTPUT_NAME}.spv")
    add_custom_command(
        OUTPUT "${OUTPUT_FILE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_DIR}"
        COMMAND ${GLSLC_EXECUTABLE}
            -fshader-stage=${SHADER_STAGE}
            "${INPUT_FILE}"
            -o "${OUTPUT_FILE}"
        DEPENDS "${INPUT_FILE}"
        COMMENT "Compiling ${OUTPUT_NAME}.spv"
        VERBATIM
    )
    set(SPIRV_OUTPUTS ${SPIRV_OUTPUTS} "${OUTPUT_FILE}" PARENT_SCOPE)
endfunction()

set(SPIRV_OUTPUTS)
add_spirv_shader("${SHADER_DIR}/vertex.glsl" vert vertex)
add_spirv_shader("${SHADER_DIR}/fragment.glsl" frag fragment)
add_spirv_shader("${SHADER_DIR}/rotation_matrix.glsl" frag rotation_matrix)
add_spirv_shader("${SHADER_DIR}/water.glsl" frag water)

add_custom_target(compile_shaders ALL DEPENDS ${SPIRV_OUTPUTS})
add_dependencies(${PROJECT_NAME} compile_shaders)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${SPIRV_DIR}
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders_spirv
    COMMENT "Copying SPIR-V shader files to build directory"
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

enable_testing()

add_executable(config_smoke_test
    tests/config_smoke.cpp
    ${SRC_DIR}/Config.cpp
)

target_include_directories(config_smoke_test PRIVATE ${INCLUDE_DIR})
target_link_libraries(config_smoke_test PRIVATE ${YAML_CPP_TARGET})

add_test(
    NAME config_active_scene
    COMMAND config_smoke_test
        active_scene
        ${CMAKE_CURRENT_SOURCE_DIR}/config/shader_config.yaml
)

add_test(
    NAME config_scene_catalog
    COMMAND config_smoke_test
        scene_catalog
        ${CMAKE_CURRENT_SOURCE_DIR}/config/shader_config.yaml
)

add_test(
    NAME config_shader_sources
    COMMAND config_smoke_test
        shader_sources
        ${CMAKE_CURRENT_SOURCE_DIR}/config/shader_config.yaml
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(
    NAME config_window
    COMMAND config_smoke_test
        window_config
        ${CMAKE_CURRENT_SOURCE_DIR}/config/shader_config.yaml
)

add_test(
    NAME config_performance
    COMMAND config_smoke_test
        performance_config
        ${CMAKE_CURRENT_SOURCE_DIR}/config/shader_config.yaml
)

add_test(
    NAME shader_spirv_outputs
    COMMAND config_smoke_test
        spirv_outputs
        ${CMAKE_CURRENT_SOURCE_DIR}/config/shader_config.yaml
        ${SPIRV_DIR}
)

add_test(
    NAME vulkan_headless_core_init
    COMMAND ${PROJECT_NAME}
)
set_tests_properties(vulkan_headless_core_init PROPERTIES
    WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>
    ENVIRONMENT "TINY_RASTERIZER_HEADLESS_TEST=1"
    SKIP_RETURN_CODE 77
    TIMEOUT 10
)
