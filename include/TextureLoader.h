#ifndef TEXTURE_LOADER_H
#define TEXTURE_LOADER_H

#include <cstdint>
#include <string>
#include <vector>

// CPU 侧纹理数据：统一解码为 RGBA8、行优先、从上到下。
struct TextureImage {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> pixels;

  bool valid() const {
    return width > 0 && height > 0 &&
           pixels.size() == static_cast<size_t>(width) * height * 4u;
  }
};

// Phase 6 纹理加载链路：把磁盘上的图像解码成 RGBA8，供 staging buffer 上传。
// 目前支持 PPM (P3/P6) 与 TGA (type 2/3/10/11)，不引入第三方依赖。
namespace TextureLoader {

// 按文件内容自动识别格式；解析失败抛 std::runtime_error。
TextureImage load(const std::string &path);

// 程序化噪声（LCG），作为纹理文件缺失时的回退，不依赖外部资源。
TextureImage makeNoise(int size, uint32_t seed = 0x12345678u);

} // namespace TextureLoader

#endif // TEXTURE_LOADER_H
