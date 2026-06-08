#version 450 core

layout(set = 0, binding = 0) uniform FrameUniforms {
    float iTime;
    vec2 iResolution;
    vec2 iMouse;
};

layout(location = 0) out vec4 fragColor;

// ---------- 原着色器，变量名/入口已改 ----------
vec3 palette(float d){
    return mix(vec3(0.2,0.7,0.9), vec3(1.0,0.0,1.0), d);
}
vec2 rotate(vec2 p, float a){
    float c = cos(a), s = sin(a);
    return p * mat2(c, -s, s, c);  // 正确的逆时针旋转矩阵（列主序）
}
float map(vec3 p){
    for(int i=0; i<8; ++i){
        float t = iTime * 0.2;
        p.xz = rotate(p.xz, t);
        p.xy = rotate(p.xy, t * 1.89);
        p.xz = abs(p.xz);
        p.xz -= 0.5;
    }
    return dot(sign(p), p) / 5.0;
}
vec4 rm(vec3 ro, vec3 rd){
    float t = 0.0;
    vec3 col = vec3(0.0);
    float d;
    for(float i = 0.0; i < 64.0; ++i){
        vec3 p = ro + rd * t;
        d = map(p) * 0.5;
        if(d < 0.02) break;
        if(d > 100.0) break;
        col += palette(length(p) * 0.1) / (400.0 * d);
        t += d;
    }
    return vec4(col, 1.0 / (d * 100.0));
}
void main(){
    // Vulkan gl_FragCoord 原点在左上；Shadertoy 原点在左下
    vec2 fc = vec2(gl_FragCoord.x, iResolution.y - gl_FragCoord.y);
    vec2 uv = (fc - iResolution.xy * 0.5) / iResolution.x;
    vec3 ro = vec3(0.0, 0.0, -50.0);
    ro.xz = rotate(ro.xz, iTime);
    vec3 cf = normalize(-ro);
    vec3 cs = normalize(cross(cf, vec3(0.0,1.0,0.0)));
    vec3 cu = normalize(cross(cf, cs));
    vec3 uuv = ro + cf*3.0 + uv.x*cs + uv.y*cu;
    vec3 rd  = normalize(uuv - ro);
    fragColor = rm(ro, rd);
}