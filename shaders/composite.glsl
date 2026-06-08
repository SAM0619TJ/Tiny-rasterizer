#version 450 core

// 后处理合成：采样离屏场景结果，叠加曝光/暗角/颗粒
layout(set = 0, binding = 0) uniform sampler2D uScene;  // 离屏场景颜色
layout(set = 0, binding = 1) uniform sampler2D uGrain;  // 上传的颗粒纹理

layout(set = 0, binding = 2) uniform PostParams {
    float iTime;
    float enabled;     // 0=直通, 1=启用效果
    vec2 iResolution;
    float exposure;
    float vignette;
    float grain;
    float pad;
};

layout(location = 0) out vec4 FragColor;

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution;
    vec3 color = texture(uScene, uv).rgb;

    if (enabled > 0.5) {
        // 曝光
        color *= exposure;

        // 暗角
        vec2 d = uv - 0.5;
        float vig = 1.0 - vignette * dot(d, d) * 4.0;
        color *= clamp(vig, 0.0, 1.0);

        // 颗粒：采样上传纹理并随时间扰动
        vec2 guv = uv * 3.0 + vec2(iTime * 0.10, iTime * 0.13);
        float g = texture(uGrain, guv).r - 0.5;
        color += g * grain;
    }

    FragColor = vec4(color, 1.0);
}
