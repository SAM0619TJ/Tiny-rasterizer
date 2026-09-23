#version 450 core

// Phase 7 首个 compute 任务：在 GPU 侧生成后处理使用的噪声纹理。
// 与 CPU 回退路径（TextureLoader::makeNoise）产出等价的灰度噪声，
// 但走 storage image 写入 + compute -> fragment 的显式同步点。

layout(local_size_x = 8, local_size_y = 8) in;

layout(set = 0, binding = 0, rgba8) uniform writeonly image2D uOutput;

layout(push_constant) uniform PushConstants {
    uint width;
    uint height;
    uint seed;
    uint padding;
} pc;

uint hash(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

void main() {
    const ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    if (coord.x >= int(pc.width) || coord.y >= int(pc.height)) {
        return;
    }

    const uint index = uint(coord.y) * pc.width + uint(coord.x);
    const float gray = float(hash(index + pc.seed) & 0xFFU) / 255.0;
    imageStore(uOutput, coord, vec4(gray, gray, gray, 1.0));
}
