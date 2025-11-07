#!/bin/bash
# 快速场景切换脚本

# 使用方法:
# ./quick_switch.sh rotation_matrix
# ./quick_switch.sh fractal
# ./quick_switch.sh water

if [ $# -eq 0 ]; then
    echo "用法: $0 <场景名>"
    echo ""
    echo "可用场景:"
    echo "  rotation_matrix  - 旋转矩阵效果"
    echo "  fractal          - 分形光线追踪"
    echo "  water            - 水面效果"
    echo ""
    echo "示例:"
    echo "  $0 fractal"
    exit 1
fi

SCENE=$1
CONFIG_FILE="build/config/shader_config.yaml"

if [ ! -f "$CONFIG_FILE" ]; then
    echo "错误: 配置文件不存在，请先编译项目"
    echo "运行: cd build && cmake .. && make"
    exit 1
fi

# 场景映射
case $SCENE in
    rotation_matrix|rotation|rm)
        SCENE_NAME="rotation_matrix"
        DISPLAY_NAME="Rotation Matrix Effect"
        ;;
    fractal|frac|f)
        SCENE_NAME="fractal"
        DISPLAY_NAME="Fractal Raymarch"
        ;;
    water|w)
        SCENE_NAME="water"
        DISPLAY_NAME="Water Effect"
        ;;
    *)
        echo "错误: 未知场景 '$SCENE'"
        exit 1
        ;;
esac

# 更新配置文件
sed -i "s/^active_scene: .*/active_scene: \"$SCENE_NAME\"/" "$CONFIG_FILE"

echo "✓ 场景已切换到: $DISPLAY_NAME"
echo ""
echo "运行程序:"
echo "  cd build && ./Tiny-rasterizer"
