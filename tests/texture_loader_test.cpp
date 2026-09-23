#include "Config.h"
#include "TextureLoader.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

// 造一张有明确模式的图，便于逐像素校验（灰度 = x + y*2）。
std::vector<uint8_t> makePatternPixels(int width, int height) {
  std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 3u);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t index = (static_cast<size_t>(y) * width + x) * 3u;
      const uint8_t value = static_cast<uint8_t>((x + y * 2) & 0xFF);
      pixels[index + 0] = value;
      pixels[index + 1] = value;
      pixels[index + 2] = value;
    }
  }
  return pixels;
}

void writePpmP6(const fs::path &path, int width, int height) {
  const std::vector<uint8_t> pixels = makePatternPixels(width, height);
  std::ofstream out(path, std::ios::binary);
  require(out.is_open(), "failed to open PPM for writing");
  out << "P6\n# comment line to exercise the parser\n"
      << width << " " << height << "\n255\n";
  out.write(reinterpret_cast<const char *>(pixels.data()),
            static_cast<std::streamsize>(pixels.size()));
  require(out.good(), "failed to write PPM data");
}

void writePpmP3(const fs::path &path, int width, int height) {
  const std::vector<uint8_t> pixels = makePatternPixels(width, height);
  std::ofstream out(path);
  require(out.is_open(), "failed to open ASCII PPM for writing");
  out << "P3\n" << width << " " << height << "\n255\n";
  for (size_t i = 0; i < pixels.size(); i += 3u) {
    out << static_cast<int>(pixels[i]) << ' ' << static_cast<int>(pixels[i + 1])
        << ' ' << static_cast<int>(pixels[i + 2]) << '\n';
  }
  require(out.good(), "failed to write ASCII PPM data");
}

// type=2 未压缩 / type=10 RLE，均为 24bpp、上方原点。
void writeTga(const fs::path &path, int width, int height, bool rle) {
  const std::vector<uint8_t> pixels = makePatternPixels(width, height);
  std::ofstream out(path, std::ios::binary);
  require(out.is_open(), "failed to open TGA for writing");

  std::vector<uint8_t> header(18, 0);
  header[2] = rle ? 10 : 2;
  header[12] = static_cast<uint8_t>(width & 0xFF);
  header[13] = static_cast<uint8_t>((width >> 8) & 0xFF);
  header[14] = static_cast<uint8_t>(height & 0xFF);
  header[15] = static_cast<uint8_t>((height >> 8) & 0xFF);
  header[16] = 24;
  header[17] = 0x20; // top-down
  out.write(reinterpret_cast<const char *>(header.data()),
            static_cast<std::streamsize>(header.size()));

  if (!rle) {
    out.write(reinterpret_cast<const char *>(pixels.data()),
              static_cast<std::streamsize>(pixels.size()));
  } else {
    // 每 4 个像素一个 raw packet（BGR 顺序，符合 TGA 约定）
    const size_t pixelCount = static_cast<size_t>(width) * height;
    for (size_t i = 0; i < pixelCount; i += 4u) {
      const size_t count = std::min<size_t>(4u, pixelCount - i);
      out.put(static_cast<char>(count - 1u));
      for (size_t p = 0; p < count; ++p) {
        const uint8_t *rgb = pixels.data() + (i + p) * 3u;
        out.put(static_cast<char>(rgb[2]));
        out.put(static_cast<char>(rgb[1]));
        out.put(static_cast<char>(rgb[0]));
      }
    }
  }
  require(out.good(), "failed to write TGA data");
}

void verifyPattern(const TextureImage &image, int width, int height,
                   const std::string &label) {
  require(image.width == width, label + ": width mismatch");
  require(image.height == height, label + ": height mismatch");
  require(image.valid(), label + ": image data invalid");

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t index =
          (static_cast<size_t>(y) * width + static_cast<size_t>(x)) * 4u;
      const uint8_t expected = static_cast<uint8_t>((x + y * 2) & 0xFF);
      require(image.pixels[index + 0] == expected,
              label + ": red channel mismatch at " + std::to_string(x) + "," +
                  std::to_string(y));
      require(image.pixels[index + 3] == 255, label + ": alpha must be opaque");
    }
  }
}

void testDecoderRoundTrip(const fs::path &workDir) {
  const int width = 7;
  const int height = 5;

  const fs::path ppmBinary = workDir / "pattern_pp6.ppm";
  const fs::path ppmAscii = workDir / "pattern_pp3.ppm";
  const fs::path tgaPlain = workDir / "pattern_plain.tga";
  const fs::path tgaRle = workDir / "pattern_rle.tga";

  writePpmP6(ppmBinary, width, height);
  writePpmP3(ppmAscii, width, height);
  writeTga(tgaPlain, width, height, false);
  writeTga(tgaRle, width, height, true);

  verifyPattern(TextureLoader::load(ppmBinary.string()), width, height, "P6");
  verifyPattern(TextureLoader::load(ppmAscii.string()), width, height, "P3");
  verifyPattern(TextureLoader::load(tgaPlain.string()), width, height, "TGA");
  verifyPattern(TextureLoader::load(tgaRle.string()), width, height, "TGA-RLE");
}

void testProceduralNoise() {
  const TextureImage noise = TextureLoader::makeNoise(16);
  require(noise.valid(), "procedural noise must be valid");
  require(noise.width == 16 && noise.height == 16,
          "procedural noise size mismatch");

  bool hasVariation = false;
  for (size_t i = 4; i < noise.pixels.size(); i += 4u) {
    if (noise.pixels[i] != noise.pixels[0]) {
      hasVariation = true;
      break;
    }
  }
  require(hasVariation, "procedural noise must not be a flat color");
}

void testBrokenInputs(const fs::path &workDir) {
  const fs::path missing = workDir / "does_not_exist.ppm";
  bool threw = false;
  try {
    TextureLoader::load(missing.string());
  } catch (const std::exception &) {
    threw = true;
  }
  require(threw, "loading a missing texture must throw");

  const fs::path truncated = workDir / "truncated.ppm";
  std::ofstream out(truncated, std::ios::binary);
  out << "P6\n4 4\n255\n";
  out.put('\x01');
  out.close();
  threw = false;
  try {
    TextureLoader::load(truncated.string());
  } catch (const std::exception &) {
    threw = true;
  }
  require(threw, "loading a truncated PPM must throw");
}

void testConfiguredTexture(const Config &config, const std::string &root) {
  const PostProcessingConfig &post = config.getPostProcessingConfig();
  require(post.enabled, "post processing should be enabled by default");
  require(std::fabs(post.exposure - 1.05f) < 1e-5f, "exposure mismatch");
  require(std::fabs(post.vignette - 0.35f) < 1e-5f, "vignette mismatch");
  require(std::fabs(post.grain - 0.05f) < 1e-5f, "grain mismatch");
  require(!post.texturePath.empty(),
          "post_processing.texture should point at a texture file");

  const fs::path texturePath = fs::path(root) / post.texturePath;
  const TextureImage image = TextureLoader::load(texturePath.string());
  require(image.valid(), "configured texture must decode");
  std::cout << "Configured texture: " << post.texturePath << " (" << image.width
            << "x" << image.height << ")" << std::endl;
}

} // namespace

int main(int argc, char **argv) {
  try {
    require(argc >= 3,
            "usage: texture_loader_test <mode> <arg> [source-root]");
    const std::string mode = argv[1];

    if (mode == "roundtrip") {
      const fs::path workDir(argv[2]);
      fs::create_directories(workDir);
      testDecoderRoundTrip(workDir);
      testBrokenInputs(workDir);
      testProceduralNoise();
    } else if (mode == "config_texture") {
      require(argc == 4, "config_texture requires <source-root>");
      const Config config(argv[2]);
      testConfiguredTexture(config, argv[3]);
    } else {
      throw std::runtime_error("unknown test mode: " + mode);
    }

    std::cout << "Test passed: " << mode << std::endl;
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Test failed: " << e.what() << std::endl;
    return 1;
  }
}
