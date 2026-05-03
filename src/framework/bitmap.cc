#include <framework/bitmap.h>

#include <filesystem>
#include <memory>

#include <framework/color.h>
#include <framework/framework.h>
#include <framework/graphics.h>
#include <framework/logging.h>
#include <framework/misc.h>
#include <framework/texture.h>

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#include <stb/stb_image_resize.h>

namespace fs = std::filesystem;

namespace fw {

//-------------------------------------------------------------------------
// This class contains the actual bitmap data, which is an array of 32-bit ARGB pixels
struct BitmapData {
  int width;
  int height;
  std::vector<uint32_t> rgba;
  fs::path filename;

  // constructs a new BitmapData (just set everything to 0)
  inline BitmapData() {
    width = height = 0;
  }

  inline BitmapData(int width, int height)
      : width(width), height(height) {
    rgba.resize(width * height);
  }

  BitmapData(const BitmapData&) = delete;
  BitmapData& operator=(const BitmapData&) = delete;
};

//-------------------------------------------------------------------------
Bitmap::Bitmap() :
    data_(nullptr) {
}

Bitmap::Bitmap(std::shared_ptr<BitmapData> const& data) :
    data_(data) {
}

Bitmap::Bitmap(int width, int height, uint32_t *argb /*= nullptr*/) :
    data_(std::make_shared<BitmapData>(width, height)) {
  if (argb != nullptr) {
    memcpy(&data_->rgba[0], argb, width * height * sizeof(uint32_t));
  }
}

Bitmap::Bitmap(Bitmap const &copy) {
  data_ = copy.data_;
}

Bitmap::~Bitmap() {
  data_.reset();
}

Bitmap& Bitmap::operator =(fw::Bitmap const &copy) {
  // ignore self assignment
  if (this == &copy)
    return (*this);

  data_ = copy.data_;

  return (*this);
}

StatusOr<Bitmap> LoadBitmap(fs::path const &filename) {
  LOG(INFO) << "loading image: " << filename.string();
  auto data = std::make_shared<BitmapData>(0, 0);

  int channels;
  unsigned char *pixels =
      stbi_load(filename.string().c_str(), &data->width, &data->height, &channels, 4);
  if (pixels == nullptr) {
    return ErrorStatus("Error reading image: ") << filename.string();
  }

  // copy pixels from what stb returned into our own buffer
  data->rgba.resize(data->width * data->height);
  memcpy(data->rgba.data(), reinterpret_cast<uint32_t const *>(pixels),
      data->width * data->height * sizeof(uint32_t));

  // don't need this anymore
  stbi_image_free(pixels);
  return Bitmap(data);
}

StatusOr<Bitmap> LoadBitmap(uint8_t const *data, size_t data_size) {
  int channels;
  int width;
  int height;
  unsigned char *pixels = stbi_load_from_memory(
      reinterpret_cast<unsigned char const *>(data), data_size, &width, &height, &channels, 4);
  if (pixels == nullptr) {
    return ErrorStatus("Error reading image.");
  }

  // copy pixels from what stb returned into our own buffer
  auto bitmap_data = std::make_shared<BitmapData>(width, height);
  memcpy(bitmap_data->rgba.data(), reinterpret_cast<uint32_t const *>(pixels), width * height);

  // don't need this anymore
  stbi_image_free(pixels);
  return Bitmap(bitmap_data);
}

Bitmap LoadBitmap(Texture &tex) {
  FW_ENSURE_RENDER_THREAD();

  tex.ensure_created();
  auto data = std::make_shared<BitmapData>(tex.get_width(), tex.get_height());
  tex.bind();
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data->rgba.data());

  // OpenGL returns images with 0 at the bottom, but we want 0 at the top so we have to flip it
  uint32_t *row_buffer = new uint32_t[data->width];
  for (int y = 0; y < data->height / 2; y++) {
    memcpy(row_buffer, &data->rgba[y * data->width], sizeof(uint32_t) * data->width);
    memcpy(
        &data->rgba[y * data->width],
        &data->rgba[(data->height - y - 1) * data->width],
        sizeof(uint32_t) * data->width);
    memcpy(
        &data->rgba[(data->height - y - 1) * data->width],
        row_buffer,
        sizeof(uint32_t) * data->width);
  }
  delete[] row_buffer;

  return Bitmap(data);
}

Status Bitmap::SaveBitmap(fs::path const &filename) const {
  if (data_ == nullptr)
    return OkStatus();

  LOG(INFO) << "saving image: " << filename;

  fs::path path(filename);
  if (fs::exists(path)) {
    fs::remove(path);
  }

  int res;
  if (filename.extension() == ".png") {
    res = stbi_write_png(filename.string().c_str(), data_->width, data_->height, 4,
        reinterpret_cast<void const *>(data_->rgba.data()), 0);
  } else if (filename.extension() == ".bmp") {
    res = stbi_write_bmp(filename.string().c_str(), data_->width, data_->height, 4,
        reinterpret_cast<void const *>(data_->rgba.data()));
  } else {
    res = 0;
  }
  if (res == 0) {
    return ErrorStatus("Error writing file.");
  }

  return OkStatus();
}

int Bitmap::get_width() const {
  if (!data_)
    return 0;

  return data_->width;
}

int Bitmap::get_height() const {
  if (!data_)
    return 0;

  return data_->height;
}

fs::path Bitmap::get_filename() const {
  if (!data_) {
    return fs::path();
  }
  return data_->filename;
}

std::vector<uint32_t> const &Bitmap::GetPixels() const {
  return data_->rgba;
}

void Bitmap::GetPixels(std::vector<uint32_t> &rgba) const {
  if (data_ == 0)
    return;

  int width = get_width();
  int height = get_height();

  // make sure it's big enough to hold the data
  rgba.resize(width * height);
  memcpy(rgba.data(), data_->rgba.data(), width * height * sizeof(uint32_t));
}

void Bitmap::SetPixels(std::vector<uint32_t> const &rgba) {
  int width = get_width();
  int height = get_height();

  data_ = std::make_shared<BitmapData>(width, height);
  memcpy(data_->rgba.data(), rgba.data(), width * height * sizeof(uint32_t));
}

fw::Color Bitmap::GetPixel(int x, int y) {
  return fw::Color::from_rgba(data_->rgba[(get_width() * y) + x]);
}

void Bitmap::SetPixel(int x, int y, fw::Color color) {
  data_->rgba[(get_width() * y) + x] = color.to_rgba();
}

void Bitmap::SetPixel(int x, int y, uint32_t rgba) {
  data_->rgba[(get_width() * y) + x] = rgba;
}

void Bitmap::Resize(int new_width, int new_height) {
  int curr_width = get_width();
  int curr_height = get_height();

  if (curr_width == new_width && curr_height == new_height) {
    return;
  }

  std::vector<uint32_t> resized(new_width * new_height);
  stbir_resize_uint8(reinterpret_cast<unsigned char const *>(data_->rgba.data()), curr_width, curr_height, 0,
      reinterpret_cast<unsigned char *>(resized.data()), new_width, new_height, 0, 4);

  data_ = std::make_shared<BitmapData>(new_width, new_height);
  memcpy(data_->rgba.data(), resized.data(), new_width * new_height * sizeof(uint32_t));
}

/**
 * Gets the "dominant" color of this bitmap. For now, we simple return the average color, but
 * something like finding the most common color or something might be better.
 */
fw::Color Bitmap::GetDominantColor() const {
  fw::Color average(0, 0, 0, 0);
  for(uint32_t rgba : data_->rgba) {
    average += fw::Color::from_abgr(rgba);
  }

  average /= static_cast<double>(data_->rgba.size());
  return average.clamp();
}

}
