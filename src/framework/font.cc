#include <framework/font.h>

#include <filesystem>
#include <mutex>
#include <string>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <utf8.h>

#include <freetype/fterrors.h>

#include <framework/bitmap.h>
#include <framework/graphics.h>
#include <framework/lang.h>
#include <framework/logging.h>
#include <framework/paths.h>
#include <framework/service_locator.h>
#include <framework/shader.h>
#include <framework/texture.h>

typedef std::basic_string<uint32_t> utf32string;

namespace fs = std::filesystem;

namespace fw {
namespace {

const std::string get_error_message(FT_Error err) {
  #undef FTERRORS_H_
  #define FT_ERRORDEF( e, v, s )  case e: return s;
  #define FT_ERROR_START_LIST     switch (err) {
  #define FT_ERROR_END_LIST       }
  #include FT_ERRORS_H
  return "(Unknown error)";
}

fw::Status check_error(FT_Error error) {
  if (!error) {
    return fw::OkStatus();
  }

  return fw::ErrorStatus("font error: ") << get_error_message(error);
}

}  // namespace

std::string FontManager::service_name = "FontManager";
REGISTER_SERVICE(FontManager);

class Glyph {
public:
  uint32_t ch;
  int glyph_index;
  int offset_x;
  int offset_y;
  float advance_x;
  float advance_y;
  int bitmap_left;
  int bitmap_top;
  int bitmap_width;
  int bitmap_height;
  float distance_from_baseline_to_top;
  float distance_from_baseline_to_bottom;

  Glyph(uint32_t ch, int glyph_index, int offset_x, int offset_y, float advance_x,
      float advance_y, int bitmap_left, int bitmap_top, int bitmap_width, int bitmap_height,
      float distance_from_baseline_to_top, float distance_from_baseline_to_bottom);
  ~Glyph();
};

Glyph::Glyph(uint32_t ch, int glyph_index, int offset_x, int offset_y, float advance_x,
    float advance_y, int bitmap_left, int bitmap_top, int bitmap_width, int bitmap_height,
    float distance_from_baseline_to_top, float distance_from_baseline_to_bottom) :
    ch(ch), glyph_index(glyph_index), offset_x(offset_x), offset_y(offset_y), advance_x(advance_x),
    advance_y(advance_y), bitmap_left(bitmap_left), bitmap_top(bitmap_top), bitmap_width(bitmap_width),
    bitmap_height(bitmap_height), distance_from_baseline_to_top(distance_from_baseline_to_top),
    distance_from_baseline_to_bottom(distance_from_baseline_to_bottom) {
}

Glyph::~Glyph() {
}

//-----------------------------------------------------------------------------

class StringCacheEntry {
public:
  float time_since_use;
  std::vector<fw::vertex::xyz_uv> verts;
  std::vector<uint16_t> indices;
  fw::Point size;
  float distance_to_top;
  float distance_to_bottom;

  // These will be created by EnsureReady.
  std::shared_ptr<VertexBuffer> vb;
  std::shared_ptr<IndexBuffer> ib;
  std::shared_ptr<fw::Shader> shader;
  std::shared_ptr<ShaderParameters> shader_params;

  StringCacheEntry(std::vector<fw::vertex::xyz_uv> verts, std::vector<uint16_t> indices,
      fw::Point size, float distance_to_top, float distance_to_bottom);
  ~StringCacheEntry();

  // Ensures the vertex buffer, index buffer, shader, etc are ready. Must be called on the render
  // thread.
  void EnsureReady();
};

StringCacheEntry::StringCacheEntry(
    std::vector<fw::vertex::xyz_uv> verts, std::vector<uint16_t> indices, fw::Point size,
    float distance_to_top, float distance_to_bottom) :
      verts(std::move(verts)), indices(std::move(indices)), time_since_use(0), size(size),
      distance_to_top(distance_to_top), distance_to_bottom(distance_to_bottom) {
}

StringCacheEntry::~StringCacheEntry() {
}

void StringCacheEntry::EnsureReady() {
  vb = fw::VertexBuffer::create<fw::vertex::xyz_uv>();
  vb->set_data(verts.size(), verts.data());

  ib = std::shared_ptr<fw::IndexBuffer>(new fw::IndexBuffer());
  ib->set_data(indices.size(), indices.data());

  shader = fw::Shader::CreateOrEmpty("gui.shader");
  shader_params = shader->CreateParameters();
  shader_params->set_program_name("font");
}

//-----------------------------------------------------------------------------

FontFace::FontFace()
 : size_(16) {
}

FontFace::~FontFace() {
  for(auto entry : glyphs_) {
    delete entry.second;
  }
}

fw::Status FontFace::initialize(std::filesystem::path const &filename) {
  auto err = FT_New_Face(fw::Get<FontManager>().library_, filename.string().c_str(), 0, &face_);
  RETURN_IF_ERROR(check_error(err));

  LOG(INFO) << "loaded " << filename.string() << ": " << face_->num_faces << " face(s) "
      << face_->num_glyphs << " glyph(s)";

  err = FT_Set_Pixel_Sizes(face_, 0, size_);
  RETURN_IF_ERROR(check_error(err));

  // TODO: allow us to resize the bitmap?
  bitmap_ = std::make_shared<fw::Bitmap>(256, 256);
  texture_ = std::shared_ptr<fw::Texture>(new fw::Texture());
  texture_dirty_ = true;

  return fw::OkStatus();
}

void FontFace::update(float dt) {
  // We actually want this to run on the render thread.
  fw::Get<fw::Graphics>().run_on_render_thread([=]() {
    std::unique_lock<std::mutex> lock(mutex_);
    auto it = string_cache_.begin();
    while (it != string_cache_.end()) {
      // Note we update the time_since_use after adding dt. This ensures that if the thread time is
      // really long (e.g. if there's been some delay) we'll go through at least one update loop
      // before destroying the string.
      if (it->second->time_since_use > 1.0f) {
        string_cache_.erase(it++);
      } else {
        it->second->time_since_use += dt;
        ++it;
      }
    }
  });
}

void FontFace::ensure_glyphs(std::string_view str) {
  ensure_glyphs(utf8::utf8to32(str));
}

fw::Status FontFace::ensure_glyph(char32_t ch) {
  if (glyphs_.find(ch) != glyphs_.end()) {
    // Already cached.
    return OkStatus();
  }
  if (error_glyphs_.contains(ch)) {
    // We got an error last we tried, don't try again.
    return OkStatus();
  }

  int glyph_index = FT_Get_Char_Index(face_, ch);

  // Load the glyph into the glyph slot and render it if it's not a bitmap
  RETURN_IF_ERROR(check_error(FT_Load_Glyph(face_, glyph_index, FT_LOAD_DEFAULT)));
  if (face_->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
    // TODO: FT_RENDER_MODE_LCD?
    RETURN_IF_ERROR(check_error(FT_Render_Glyph(face_->glyph, FT_RENDER_MODE_NORMAL)));
  }

  // TODO: better bitmap packing than just a straight grid
  int glyphs_per_row = bitmap_->get_width() / size_;
  int offset_y = glyphs_.size() / glyphs_per_row;
  int offset_x = (glyphs_.size() - (offset_y * glyphs_per_row));
  offset_y *= size_;
  offset_x *= size_;

  for (int y = 0; y < face_->glyph->bitmap.rows; y++) {
    for (int x = 0; x < face_->glyph->bitmap.width; x++) {
      uint32_t rgba = 0x00ffffff | (
          face_->glyph->bitmap.buffer[y * face_->glyph->bitmap.width + x] << 24);
      bitmap_->set_pixel(offset_x + x, offset_y + y, rgba);
    }
  }

  std::string chstr = utf8::utf32to8(std::u32string({ch, 0}));
  glyphs_[ch] = new Glyph(ch, glyph_index, offset_x, offset_y,
      face_->glyph->advance.x / 64.0f, face_->glyph->advance.y / 64.0f, face_->glyph->bitmap_left,
      face_->glyph->bitmap_top, face_->glyph->bitmap.width, face_->glyph->bitmap.rows,
      face_->glyph->metrics.horiBearingY / 64.0f,
      (face_->glyph->metrics.height - face_->glyph->metrics.horiBearingY) / 64.0f);
  texture_dirty_ = true;

  return fw::OkStatus();
}

void FontFace::ensure_glyphs(std::u32string_view str) {
  for (uint32_t ch : str) {
    auto status = ensure_glyph(ch);
    if (!status.ok()) {
      LOG(ERR) << "error ensuring glyph '"
               << utf8::utf32to8(std::u32string(1, ch)) << "' " << status;

      error_glyphs_.emplace(ch);
      // But keep going.
    }
  }
}

fw::Point FontFace::measure_string(std::string_view str) {
  return measure_string(utf8::utf8to32(str));
}

fw::Point FontFace::measure_string(std::u32string_view str) {
  std::shared_ptr<StringCacheEntry> data = get_or_create_cache_entry(str);
  return data->size;
}

fw::Point FontFace::measure_substring(std::u32string_view str, int pos, int num_chars) {
  ensure_glyphs(str);

  fw::Point size(0, 0);
  for (int i = pos; i < pos + num_chars; i++) {
    auto glyph_size_or_status = measure_glyph(str[i]);
    if (!glyph_size_or_status.ok()) {
      continue;
    }
    auto glyph_size = *glyph_size_or_status;
    size[0] += glyph_size[0];
    if (size[1] < glyph_size[1]) {
      size[1] = glyph_size[1];
    }
  }

  return size;
}

fw::StatusOr<fw::Point> FontFace::measure_glyph(char32_t ch) {
  RETURN_IF_ERROR(ensure_glyph(ch));
  Glyph *g = glyphs_[ch];
  float y = g->distance_from_baseline_to_top + g->distance_from_baseline_to_bottom;
  return fw::Point(g->advance_x, y);
}

void FontFace::draw_string(int x, int y, std::string const &str, DrawFlags flags /*= 0*/,
    fw::Color color /*= fw::color::WHITE*/) {
  draw_string(x, y, utf8::utf8to32(str), flags, color);
}

void FontFace::draw_string(
    int x, int y, std::u32string_view str, DrawFlags flags, fw::Color color) {
  std::shared_ptr<StringCacheEntry> data = get_or_create_cache_entry(str);
  data->EnsureReady();

  if (texture_dirty_) {
    texture_->create(bitmap_);
    texture_dirty_ = false;
  }

  if ((flags & kAlignCenter) != 0) {
    x -= data->size[0] / 2;
  } else if (( flags & kAlignRight) != 0) {
    x -= data->size[0];
  }
  if ((flags & kAlignTop) != 0) {
    y += data->distance_to_top;
  } else if ((flags & kAlignMiddle) != 0) {
    y += (data->distance_to_top - data->distance_to_bottom) / 2;
  } else if ((flags & kAlignBottom) != 0) {
    y -= data->distance_to_bottom;
  }

  // TODO: recalculating this every time seems wasteful
  auto& g = fw::Get<Graphics>();
  fw::Matrix pos_transform =
    fw::projection_orthographic(
      0.0f,
      static_cast<float>(g.get_width()),
      static_cast<float>(g.get_height()),
      0.0f, 1.0f, -1.0f);
  pos_transform = fw::translation(x, y, 0.0f) * pos_transform;

  data->shader_params->set_matrix("pos_transform", pos_transform);
  data->shader_params->set_matrix("uv_transform", fw::identity());
  data->shader_params->set_color("color", color);
  data->shader_params->set_texture("texsampler", texture_);

  data->vb->begin();
  data->ib->begin();
  data->shader->Begin(data->shader_params);
  glDrawElements(GL_TRIANGLES, data->ib->get_num_indices(), GL_UNSIGNED_SHORT, nullptr);
  data->shader->End();
  data->ib->end();
  data->vb->end();

  // Reset the Timer so we keep this string cached.
  data->time_since_use = 0.0f;
}

std::shared_ptr<StringCacheEntry> FontFace::get_or_create_cache_entry(std::u32string_view str) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = string_cache_.find(std::u32string(str));
  if (it != string_cache_.end()) {
    return it->second;
  }

  auto data = create_cache_entry(str);
  string_cache_[std::u32string(str)] = data;
  return data;
}

std::shared_ptr<StringCacheEntry> FontFace::create_cache_entry(std::u32string_view str) {
  ensure_glyphs(str);

  std::vector<fw::vertex::xyz_uv> verts;
  verts.reserve(str.size() * 4);
  std::vector<uint16_t> indices;
  indices.reserve(str.size() * 6);
  float x = 0;
  float y = 0;
  float max_distance_to_top = 0.0f;
  float max_distance_to_bottom = 0.0f;
  for(uint32_t ch : str) {
    Glyph *g = glyphs_[ch];
    uint16_t index_offset = verts.size();
    verts.push_back(fw::vertex::xyz_uv(
        x + g->bitmap_left,
        y - g->bitmap_top, 0.0f,
        static_cast<float>(g->offset_x) / bitmap_->get_width(),
        static_cast<float>(g->offset_y) / bitmap_->get_height()));
    verts.push_back(fw::vertex::xyz_uv(
        x + g->bitmap_left,
        y - g->bitmap_top + g->bitmap_height, 0.0f,
        static_cast<float>(g->offset_x) / bitmap_->get_width(),
        static_cast<float>(g->offset_y + g->bitmap_height) / bitmap_->get_height()));
    verts.push_back(fw::vertex::xyz_uv(
        x + g->bitmap_left + g->bitmap_width,
        y - g->bitmap_top + g->bitmap_height, 0.0f,
        static_cast<float>(g->offset_x + g->bitmap_width) / bitmap_->get_width(),
        static_cast<float>(g->offset_y + g->bitmap_height) / bitmap_->get_height()));
    verts.push_back(fw::vertex::xyz_uv(
        x + g->bitmap_left + g->bitmap_width,
        y - g->bitmap_top, 0.0f,
        static_cast<float>(g->offset_x + g->bitmap_width) / bitmap_->get_width(),
        static_cast<float>(g->offset_y) / bitmap_->get_height()));

    indices.push_back(index_offset);
    indices.push_back(index_offset + 1);
    indices.push_back(index_offset + 2);
    indices.push_back(index_offset);
    indices.push_back(index_offset + 2);
    indices.push_back(index_offset + 3);

    x += g->advance_x;
    y += g->advance_y;
    if (max_distance_to_top < g->distance_from_baseline_to_top) {
      max_distance_to_top = g->distance_from_baseline_to_top;
    }
    if (max_distance_to_bottom < g->distance_from_baseline_to_bottom) {
      max_distance_to_bottom = g->distance_from_baseline_to_bottom;
    }
  }


  return std::shared_ptr<StringCacheEntry>(new StringCacheEntry(
      std::move(verts), std::move(indices),
      fw::Point(x, max_distance_to_bottom + max_distance_to_top), max_distance_to_top,
      max_distance_to_bottom));
}

//-----------------------------------------------------------------------------

fw::Status FontManager::initialize() {
  RETURN_IF_ERROR(check_error(FT_Init_FreeType(&library_)));

  return fw::OkStatus();
}

void FontManager::update(float dt) {
  for (auto it : faces_) {
    it.second->update(dt);
  }
}

std::shared_ptr<FontFace> FontManager::get_face() {
  return get_face(fw::resolve("gui/" + fw::text("lang.font")));
}

/** Gets the \ref font_face for the font at the given path (assumed to be a .ttf file). */
std::shared_ptr<FontFace> FontManager::get_face(fs::path const &filename) {
  // TODO: thread-safety?
  std::shared_ptr<FontFace> face = faces_[filename.string()];
  if (!face) {
    face = std::make_shared<FontFace>();
    auto status = face->initialize(filename);
    if (!status.ok()) {
      LOG(ERR) << "error loading font from " << filename.string() << ": " << status;
      // Keep going, we'll add the uninitialized font so we don't keep trying.
    }
    faces_[filename.string()] = face;
  }
  return face;
}

}
