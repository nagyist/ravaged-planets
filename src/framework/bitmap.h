#pragma once

#include <filesystem>
#include <memory>

#include <framework/color.h>
#include <framework/status.h>

namespace fw {
class Texture;
struct BitmapData;

// Represents a bitmap, always RGBA.
class Bitmap {
public:
  // Constructs a blank bitmap.
  Bitmap();

  Bitmap(std::shared_ptr<BitmapData> const& data);

  // Constructs a new bitmap that is the given width/height.
  Bitmap(int width, int height, uint32_t *argb = nullptr);
  
  // Constructs a new bitmap that is a copy of the given bitmap. We won't actually copy the data
  // until you write to it.
  Bitmap(Bitmap const &copy);

  // Destroys the bitmap and frees all associated data.
  ~Bitmap();

  // Assigns this bitmap to what's in the the other one
  Bitmap&operator =(fw::Bitmap const &copy);

  // Gets the filename of this bitmap (if it was loaded from a file).
  std::filesystem::path get_filename() const;

  // Saves the bitmap to the given file.
  Status SaveBitmap(std::filesystem::path const &filename) const;

  // Gets the width/height (in pixels) of this image
  int get_width() const;
  int get_height() const;

  // Gets or sets the actual pixel data. We assume the format of the pixels is RGBA and that the given data is big
  // enough to fix this bitmap
  std::vector<uint32_t> const &GetPixels() const;
  void GetPixels(std::vector<uint32_t> &rgba) const;
  void SetPixels(std::vector<uint32_t> const &rgba);

  // Helper methods to get/set the color of a single pixel.
  fw::Color GetPixel(int x, int y);
  void SetPixel(int x, int y, fw::Color color);
  void SetPixel(int x, int y, uint32_t rgba);

  // Copies the bitmap data from the given source image to our buffer.
  void Copy(fw::Bitmap const &src);

  // Resizes the bitmap to the new width/height
  void Resize(int new_width, int new_height);

  // Calculate the "dominant" color of this bitmap.
  fw::Color GetDominantColor() const;

private:
  std::shared_ptr<BitmapData> data_;
};

// Loads a Bitmap from the given file.
StatusOr<Bitmap> LoadBitmap(std::filesystem::path const &filename);

// Loads a Bitmap from the given in-memory data.
StatusOr<Bitmap> LoadBitmap(uint8_t const *data, size_t data_size);

// Loads a Bitmap from the given texture.
Bitmap LoadBitmap(Texture &tex);

}
