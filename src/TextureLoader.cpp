#include "TextureLoader.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace {

std::vector<uint8_t> readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open texture file: " + path);
  }

  const std::streamsize size = file.tellg();
  if (size <= 0) {
    throw std::runtime_error("Texture file is empty: " + path);
  }

  std::vector<uint8_t> data(static_cast<size_t>(size));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(data.data()), size);
  if (!file) {
    throw std::runtime_error("Failed to read texture file: " + path);
  }
  return data;
}

bool looksLikePpm(const std::vector<uint8_t> &data) {
  return data.size() >= 2 && data[0] == 'P' &&
         (data[1] == '3' || data[1] == '6');
}

uint8_t scaleToByte(long value, long maxValue) {
  if (maxValue <= 0) {
    return 0;
  }
  return static_cast<uint8_t>((value * 255L) / maxValue);
}

// ---------------------------------------------------------------- PPM (P3/P6)

struct PpmCursor {
  const std::vector<uint8_t> &data;
  size_t pos = 0;

  void skipSeparators() {
    while (pos < data.size()) {
      const uint8_t c = data[pos];
      if (c == '#') {
        while (pos < data.size() && data[pos] != '\n') {
          ++pos;
        }
      } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        ++pos;
      } else {
        break;
      }
    }
  }

  long readInt() {
    skipSeparators();
    if (pos >= data.size() || data[pos] < '0' || data[pos] > '9') {
      throw std::runtime_error("Invalid PPM header: expected an integer");
    }
    long value = 0;
    while (pos < data.size() && data[pos] >= '0' && data[pos] <= '9') {
      value = value * 10L + static_cast<long>(data[pos] - '0');
      ++pos;
    }
    return value;
  }
};

TextureImage parsePpm(const std::vector<uint8_t> &data) {
  const bool binary = data[1] == '6';

  PpmCursor cursor{data, 2};
  const long width = cursor.readInt();
  const long height = cursor.readInt();
  const long maxValue = cursor.readInt();
  if (width <= 0 || height <= 0 || maxValue <= 0 || maxValue > 65535) {
    throw std::runtime_error("Invalid PPM header: bad size or maxval");
  }

  TextureImage image;
  image.width = static_cast<int>(width);
  image.height = static_cast<int>(height);
  image.pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) *
                      4u);

  const size_t pixelCount =
      static_cast<size_t>(width) * static_cast<size_t>(height);
  const int bytesPerSample = maxValue < 256 ? 1 : 2;

  if (binary) {
    // 二进制格式：maxval 之后恰好一个空白字符，之后就是原始数据。
    if (cursor.pos < data.size() &&
        (data[cursor.pos] == ' ' || data[cursor.pos] == '\t' ||
         data[cursor.pos] == '\r' || data[cursor.pos] == '\n')) {
      ++cursor.pos;
    }
    const size_t needed = pixelCount * 3u * static_cast<size_t>(bytesPerSample);
    if (data.size() < cursor.pos + needed) {
      throw std::runtime_error("Truncated binary PPM pixel data");
    }

    size_t offset = cursor.pos;
    for (size_t i = 0; i < pixelCount; ++i) {
      uint8_t *dst = image.pixels.data() + i * 4u;
      for (int channel = 0; channel < 3; ++channel) {
        long sample = 0;
        if (bytesPerSample == 1) {
          sample = data[offset++];
        } else {
          sample = (static_cast<long>(data[offset]) << 8) |
                   static_cast<long>(data[offset + 1]);
          offset += 2;
        }
        dst[channel] = scaleToByte(sample, maxValue);
      }
      dst[3] = 255;
    }
  } else {
    for (size_t i = 0; i < pixelCount; ++i) {
      uint8_t *dst = image.pixels.data() + i * 4u;
      for (int channel = 0; channel < 3; ++channel) {
        dst[channel] = scaleToByte(cursor.readInt(), maxValue);
      }
      dst[3] = 255;
    }
  }

  return image;
}

// ---------------------------------------------------------------- TGA

TextureImage parseTga(const std::vector<uint8_t> &data) {
  if (data.size() < 18) {
    throw std::runtime_error("Invalid TGA file: header truncated");
  }

  const uint8_t idLength = data[0];
  const uint8_t colorMapType = data[1];
  const uint8_t imageType = data[2];
  if (colorMapType != 0) {
    throw std::runtime_error("Unsupported TGA: color-mapped images");
  }

  const bool rle = imageType == 10 || imageType == 11;
  const bool grayscale = imageType == 3 || imageType == 11;
  const bool trueColor = imageType == 2 || imageType == 10;
  if (!grayscale && !trueColor) {
    throw std::runtime_error("Unsupported TGA image type: " +
                             std::to_string(imageType));
  }

  const int width = static_cast<int>(data[12]) | (data[13] << 8);
  const int height = static_cast<int>(data[14]) | (data[15] << 8);
  if (width <= 0 || height <= 0) {
    throw std::runtime_error("Invalid TGA: bad dimensions");
  }

  const uint8_t pixelDepth = data[16];
  const bool topDown = (data[17] & 0x20) != 0;
  const int channels = grayscale ? 1 : pixelDepth / 8;
  if (grayscale && pixelDepth != 8) {
    throw std::runtime_error("Unsupported TGA: grayscale must be 8bpp");
  }
  if (trueColor && pixelDepth != 24 && pixelDepth != 32) {
    throw std::runtime_error("Unsupported TGA: true-color must be 24/32bpp");
  }

  const size_t pixelCount =
      static_cast<size_t>(width) * static_cast<size_t>(height);
  const size_t rawSize = pixelCount * static_cast<size_t>(channels);

  size_t pos = 18u + idLength;
  std::vector<uint8_t> raw(rawSize);

  if (rle) {
    size_t written = 0;
    while (written < rawSize) {
      if (pos >= data.size()) {
        throw std::runtime_error("Truncated TGA RLE data");
      }
      const uint8_t packet = data[pos++];
      const size_t count = static_cast<size_t>(packet & 0x7Fu) + 1u;
      const size_t bytes = count * static_cast<size_t>(channels);

      if ((packet & 0x80u) != 0) {
        if (pos + static_cast<size_t>(channels) > data.size()) {
          throw std::runtime_error("Truncated TGA RLE packet");
        }
        for (size_t i = 0; i < count && written < rawSize; ++i) {
          std::memcpy(raw.data() + written, data.data() + pos,
                      static_cast<size_t>(channels));
          written += static_cast<size_t>(channels);
        }
        pos += static_cast<size_t>(channels);
      } else {
        if (pos + bytes > data.size()) {
          throw std::runtime_error("Truncated TGA raw packet");
        }
        const size_t copySize = std::min(bytes, rawSize - written);
        std::memcpy(raw.data() + written, data.data() + pos, copySize);
        written += copySize;
        pos += bytes;
      }
    }
  } else {
    if (data.size() < pos + rawSize) {
      throw std::runtime_error("Truncated TGA pixel data");
    }
    std::memcpy(raw.data(), data.data() + pos, rawSize);
  }

  TextureImage image;
  image.width = width;
  image.height = height;
  image.pixels.resize(pixelCount * 4u);

  for (int y = 0; y < height; ++y) {
    // TGA 默认为左下原点；descriptor bit 5 置位表示已是上方原点。
    const int sourceRow = topDown ? y : (height - 1 - y);
    const uint8_t *source =
        raw.data() + static_cast<size_t>(sourceRow) *
                         static_cast<size_t>(width) *
                         static_cast<size_t>(channels);
    uint8_t *dst = image.pixels.data() + static_cast<size_t>(y) *
                                             static_cast<size_t>(width) * 4u;
    for (int x = 0; x < width; ++x) {
      const uint8_t *src = source + static_cast<size_t>(x) *
                                        static_cast<size_t>(channels);
      if (grayscale) {
        dst[0] = src[0];
        dst[1] = src[0];
        dst[2] = src[0];
        dst[3] = 255;
      } else {
        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
        dst[3] = channels == 4 ? src[3] : 255;
      }
      dst += 4;
    }
  }

  return image;
}

} // namespace

namespace TextureLoader {

TextureImage load(const std::string &path) {
  const std::vector<uint8_t> data = readFile(path);
  if (looksLikePpm(data)) {
    return parsePpm(data);
  }
  return parseTga(data);
}

TextureImage makeNoise(int size, uint32_t seed) {
  if (size <= 0) {
    throw std::runtime_error("Noise texture size must be positive");
  }

  TextureImage image;
  image.width = size;
  image.height = size;
  image.pixels.resize(static_cast<size_t>(size) * static_cast<size_t>(size) *
                      4u);

  uint32_t state = seed;
  for (size_t i = 0; i < image.pixels.size(); i += 4u) {
    state = state * 1664525u + 1013904223u;
    const uint8_t value = static_cast<uint8_t>((state >> 16) & 0xFFu);
    image.pixels[i + 0] = value;
    image.pixels[i + 1] = value;
    image.pixels[i + 2] = value;
    image.pixels[i + 3] = 255;
  }
  return image;
}

} // namespace TextureLoader
