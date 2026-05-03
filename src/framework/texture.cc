#include <filesystem>
#include <memory>

#include <stb/stb_image.h>

#include <framework/texture.h>
#include <framework/framework.h>
#include <framework/bitmap.h>
#include <framework/graphics.h>
#include <framework/misc.h>
#include <framework/paths.h>
#include <framework/logging.h>

namespace fs = std::filesystem;

namespace fw {

//-------------------------------------------------------------------------
struct TextureData {
  GLuint texture_id = 0;
  int width = -1, height = -1;
  fs::path filename; // might be empty if we weren't created from a file

  GLenum min_filter = GL_LINEAR;
  GLenum mag_filter = GL_LINEAR;

  TextureData() = default;
  TextureData(const TextureData&) = delete;
  TextureData& operator=(const TextureData&) = delete;

  ~TextureData() {
    GLuint id = texture_id;
    if (id != 0) {
      fw::Get<Graphics>().run_on_render_thread(
        [id]() {
          glDeleteTextures(1, &id);
        });
    }
  }
};

//-------------------------------------------------------------------------
// This is a cache of textures, so we don't have to load them over and over.
class TextureCache {
private:
  typedef std::map<fs::path, std::shared_ptr<TextureData>> TexturesMap;
  TexturesMap textures_;

public:
  std::shared_ptr<TextureData> get_texture(fs::path const &filename);
  void add_texture(fs::path const &filename, std::shared_ptr<TextureData> data);

  void clear_cache();
};

std::shared_ptr<TextureData> TextureCache::get_texture(fs::path const &filename) {
  TexturesMap::iterator it = textures_.find(filename);
  if (it == textures_.end())
    return std::shared_ptr<TextureData>();

  return it->second;
}

void TextureCache::add_texture(fs::path const &filename, std::shared_ptr<TextureData> data) {
  textures_[filename] = data;
}

void TextureCache::clear_cache() {
  FW_ENSURE_RENDER_THREAD();

  for (auto texture : textures_) {
    glDeleteTextures(1, &texture.second->texture_id);
  }
  textures_.clear();
}

static TextureCache g_cache;

//-------------------------------------------------------------------------

Texture::Texture() {
}

Texture::~Texture() {
}

void Texture::create(fs::path const &fn) {
  // Local the image on this thread to avoid loading it on the render thread.
  fs::path filename = fn;
  LOG(INFO) << "loading texture: " << filename.string();
  int width, height, channels;
  unsigned char* pixels = stbi_load(filename.string().c_str(), &width, &height, &channels, 4);

  if (!data_) {
    data_ = std::make_shared<TextureData>();
  }
  data_->filename = filename;
  data_->width = width;
  data_->height = height;

  data_creator_ = [filename, pixels, width, height, channels](TextureData& data) {
    if (data.texture_id == 0) {
      glGenTextures(1, &data.texture_id);
    }
    glBindTexture(GL_TEXTURE_2D, data.texture_id);
    // TODO: pre-multiply alpha
    // TODO: DXT compress
    // TODO: mipmaps
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, data.width, data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    stbi_image_free(pixels);
  };
}

void Texture::create(std::shared_ptr<fw::Bitmap> bmp) {
  create(*bmp);
}

void Texture::create(fw::Bitmap const &bmp, GLenum internal_format /*= GL_RGBA8*/, GLenum format /*= GL_RGBA*/,
                     GLenum component_type /*= GL_UNSIGNED_BYTE*/) {
  fw::Bitmap bitmap(bmp);

  if (!data_) {
    data_ = std::make_shared<TextureData>();
  }
  data_->width = bitmap.get_width();
  data_->height = bitmap.get_height();

  data_creator_ = [bitmap, internal_format, format, component_type](TextureData& data) {
    if (data.texture_id == 0) {
      glGenTextures(1, &data.texture_id);
    }
    glBindTexture(GL_TEXTURE_2D, data.texture_id);
    glTexImage2D(
        GL_TEXTURE_2D, 0, internal_format, data.width, data.height, 0,
        format, component_type, bitmap.GetPixels().data());
  };
}

void Texture::create(int width, int height, GLenum internal_format /*=GL_RGBA8*/, GLenum format /*= GL_RGBA*/,
                     GLenum component_type /*= GL_UNSIGNED_BYTE*/) {
  if (!data_) {
    data_ = std::make_shared<TextureData>();
  }
  data_->width = width;
  data_->height = height;

  data_creator_ = [width, height, internal_format, format, component_type](TextureData& data) {
    if (data.texture_id == 0) {
      glGenTextures(1, &data.texture_id);
    }
    glBindTexture(GL_TEXTURE_2D, data.texture_id);
    glTexImage2D(
        GL_TEXTURE_2D, 0, internal_format, data.width, data.height, 0, format, component_type,
        nullptr);
  };
}

void Texture::set_filter(GLenum min_filter, GLenum mag_filter) {
  if (!data_) return;

  data_->min_filter = min_filter;
  data_->mag_filter = mag_filter;
}

void Texture::ensure_created() {
  FW_ENSURE_RENDER_THREAD();
  if (data_creator_) {
    data_creator_(*data_);
    data_creator_ = nullptr;
  }
}

void Texture::bind() const {
  if (!data_) {
    glBindTexture(GL_TEXTURE_2D, 0);
    return;
  }

  glBindTexture(GL_TEXTURE_2D, data_->texture_id);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, data_->min_filter);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, data_->mag_filter);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

fw::Status Texture::save_png(fs::path const &filename) {
  Bitmap bmp = LoadBitmap(*this);
  return bmp.SaveBitmap(filename);
}

void Texture::calculate_size() const {
  if (data_->width > 0 && data_->height > 0)
    return;
}

int Texture::get_width() const {
  calculate_size();
  return data_->width;
}

int Texture::get_height() const {
  calculate_size();
  return data_->height;
}

fs::path Texture::get_filename() const {
  return data_->filename;
}

//-----------------------------------------------------------------------------

TextureArray::TextureArray(int width, int height)
  : width_(width), height_(height) {
}

TextureArray::~TextureArray() {
}

Status TextureArray::add(fs::path const& filename) {
  // TODO: check if data has been created.
  ASSIGN_OR_RETURN(auto bmp, LoadBitmap(filename));
  bmp.Resize(width_, height_);
  bitmaps_.push_back(bmp);
  return OkStatus();
}

void TextureArray::add(fw::Bitmap const &bmp) {
  auto resized_bmp = bmp;
  resized_bmp.Resize(width_, height_);
  bitmaps_.push_back(resized_bmp);
}

void TextureArray::ensure_created() {
  FW_ENSURE_RENDER_THREAD();
  if (data_) return;

  data_ = std::make_shared<TextureData>();
  glGenTextures(1, &data_->texture_id);
  glBindTexture(GL_TEXTURE_2D_ARRAY, data_->texture_id);
  glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, width_, height_, bitmaps_.size());
  for (int i = 0; i < bitmaps_.size(); i++) {
    auto& bitmap = bitmaps_[i];
    glTexSubImage3D(
        GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, width_, height_, 1, GL_RGBA, GL_UNSIGNED_BYTE,
        bitmap.GetPixels().data());
  }
}

void TextureArray::bind() const {
  FW_ENSURE_RENDER_THREAD();
  if (!data_) return;

  glBindTexture(GL_TEXTURE_2D_ARRAY, data_->texture_id);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

//-----------------------------------------------------------------------------

struct FramebufferData {
  GLuint fbo_id;
  std::shared_ptr<Texture> color_texture;
  std::shared_ptr<Texture> depth_texture;
  bool initialized;

  FramebufferData() : initialized(false) {
    glGenFramebuffers(1, &fbo_id);
  }

  FramebufferData(const FramebufferData&) = delete;
  FramebufferData& operator=(const FramebufferData&) = delete;

  ~FramebufferData() {
    glDeleteFramebuffers(1, &fbo_id);
  }

  void ensure_initialized() {
    if (initialized) {
      return;
    }
    initialized = true;

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
    if (depth_texture) {
      GLuint texture_id = depth_texture->get_data()->texture_id;
      glBindTexture(GL_TEXTURE_2D, texture_id);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture_id, 0);
    }

    if (color_texture) {
      GLuint texture_id = color_texture->get_data()->texture_id;
      glBindTexture(GL_TEXTURE_2D, texture_id);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_id, 0);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      LOG(ERR) << "Framebuffer is not complete, expect errors: " << status;
    }
  }
};

//-----------------------------------------------------------------------------
Framebuffer::Framebuffer() : data_(new FramebufferData()) {
}

Framebuffer::~Framebuffer() {
}

void Framebuffer::set_color_buffer(std::shared_ptr<Texture> color_texture) {
  data_->color_texture = color_texture;
}

void Framebuffer::set_depth_buffer(std::shared_ptr<Texture> depth_texture) {
  data_->depth_texture = depth_texture;
}

std::shared_ptr<Texture> Framebuffer::get_color_buffer() const {
  return data_->color_texture;
}

std::shared_ptr<Texture> Framebuffer::get_depth_buffer() const {
  return data_->depth_texture;
}

void Framebuffer::bind() {
  data_->ensure_initialized();
  glBindFramebuffer(GL_FRAMEBUFFER, data_->fbo_id);
}

void Framebuffer::clear() {
  int bits = 0;
  if (data_->color_texture) {
    bits |= GL_COLOR_BUFFER_BIT;
  }
  if (data_->depth_texture) {
    bits |= GL_DEPTH_BUFFER_BIT;
  }
  glClear(bits);
}

void Framebuffer::unbind() {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

int Framebuffer::get_width() const {
  if (data_->color_texture) {
    return data_->color_texture->get_width();
  } else if (data_->depth_texture) {
    return data_->depth_texture->get_width();
  } else {
    return 0;
  }
}

int Framebuffer::get_height() const {
  if (data_->color_texture) {
    return data_->color_texture->get_height();
  } else if (data_->depth_texture) {
    return data_->depth_texture->get_height();
  } else {
    return 0;
  }
}

}
