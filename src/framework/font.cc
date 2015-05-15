
#include <boost/foreach.hpp>
#include <boost/locale.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <framework/bitmap.h>
#include <framework/exception.h>
#include <framework/font.h>
#include <framework/graphics.h>
#include <framework/index_buffer.h>
#include <framework/logging.h>
#include <framework/paths.h>
#include <framework/shader.h>
#include <framework/texture.h>
#include <framework/vertex_buffer.h>
#include <framework/vertex_formats.h>

typedef std::basic_string<uint32_t> utf32string;

namespace fs = boost::filesystem;
namespace conv = boost::locale::conv;

#define FT_CHECK(fn) { \
  FT_Error err = fn; \
  if (err) { \
    BOOST_THROW_EXCEPTION(fw::exception() << fw::message_error_info(#fn)/*TODO << freetype_error_info(err)*/); \
  } \
}

namespace fw {

class glyph {
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

  glyph(uint32_t ch, int glyph_index, int offset_x, int offset_y, float advance_x,
      float advance_y, int bitmap_left, int bitmap_top, int bitmap_width, int bitmap_height);
  ~glyph();
};

glyph::glyph(uint32_t ch, int glyph_index, int offset_x, int offset_y, float advance_x,
    float advance_y, int bitmap_left, int bitmap_top, int bitmap_width, int bitmap_height) :
    ch(ch), glyph_index(glyph_index), offset_x(offset_x), offset_y(offset_y), advance_x(advance_x),
    advance_y(advance_y), bitmap_left(bitmap_left), bitmap_top(bitmap_top), bitmap_width(bitmap_width),
    bitmap_height(bitmap_height) {
}

glyph::~glyph() {
}

//-----------------------------------------------------------------------------

class string_cache_entry {
public:
  std::shared_ptr<vertex_buffer> vb;
  std::shared_ptr<index_buffer> ib;
  std::shared_ptr<fw::shader> shader;
  std::shared_ptr<shader_parameters> shader_params;

  string_cache_entry(std::shared_ptr<vertex_buffer> vb, std::shared_ptr<index_buffer> ib,
      std::shared_ptr<fw::shader> shader, std::shared_ptr<shader_parameters> shader_params);
  ~string_cache_entry();
};

string_cache_entry::string_cache_entry(std::shared_ptr<vertex_buffer> vb,
    std::shared_ptr<index_buffer> ib, std::shared_ptr<fw::shader> shader,
    std::shared_ptr<shader_parameters> shader_params) :
      vb(vb), ib(ib), shader(shader), shader_params(shader_params) {
}

string_cache_entry::~string_cache_entry() {
}

//-----------------------------------------------------------------------------

font_face::font_face(font_manager *manager, fs::path const &filename) :
    _manager(manager), _size(16) {
  FT_CHECK(FT_New_Face(_manager->_library, filename.string().c_str(), 0, &_face));
  fw::debug << "Loaded " << filename << std::endl;
  fw::debug << "  " << _face->num_faces << " face(s) " << _face->num_glyphs << " glyph(s)" << std::endl;

  FT_CHECK(FT_Set_Pixel_Sizes(_face, 0, _size));

  // TODO: allow us to resize the bitmap?
  _bitmap = std::shared_ptr<fw::bitmap>(new fw::bitmap(256, 256));
  _texture = std::shared_ptr<fw::texture>(new fw::texture());
  _texture_dirty = true;
}

font_face::~font_face() {
  BOOST_FOREACH(auto entry, _glyphs) {
    delete entry.second;
  }
  BOOST_FOREACH(auto entry, _string_cache) {
    delete entry.second;
  }
}

void font_face::ensure_glyphs(std::string const &str) {
  ensure_glyphs(conv::utf_to_utf<uint32_t>(str));
}

void font_face::ensure_glyphs(std::basic_string<uint32_t> const &str) {
  BOOST_FOREACH(uint32_t ch, str) {
    if (_glyphs.find(ch) != _glyphs.end()) {
      // Already cached.
      continue;
    }

    int glyph_index = FT_Get_Char_Index(_face, ch);

    // Load the glyph into the glyph slot and render it if it's not a bitmap
    FT_CHECK(FT_Load_Glyph(_face, glyph_index, FT_LOAD_DEFAULT));
    if (_face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
      // TODO: FT_RENDER_MODE_LCD?
      FT_CHECK(FT_Render_Glyph(_face->glyph, FT_RENDER_MODE_NORMAL));
    }

    // TODO: better bitmap packing than just a straight grid
    int glyphs_per_row = _bitmap->get_width() / _size;
    int offset_y = _glyphs.size() / glyphs_per_row;
    int offset_x = (_glyphs.size() - (offset_y * glyphs_per_row));
    offset_y *= _size;
    offset_x *= _size;

    for (int y = 0; y < _face->glyph->bitmap.rows; y++) {
      for (int x = 0; x < _face->glyph->bitmap.width; x++) {
        uint32_t rgba = 0x00ffffff | (
            _face->glyph->bitmap.buffer[y * _face->glyph->bitmap.width + x] << 24);
        _bitmap->set_pixel(offset_x + x, offset_y + y, rgba);
      }
    }

    _glyphs[ch] = new glyph(ch, glyph_index, offset_x, offset_y,
        _face->glyph->advance.x / 64.0f, _face->glyph->advance.y / 64.0f, _face->glyph->bitmap_left,
        _face->glyph->bitmap_top, _face->glyph->bitmap.width, _face->glyph->bitmap.rows);
    _texture_dirty = true;
  }
}

void font_face::draw_string(int x, int y, std::string const &str) {
  draw_string(x, y, conv::utf_to_utf<uint32_t>(str));
}

void font_face::draw_string(int x, int y, std::basic_string<uint32_t> const &str) {
  string_cache_entry *data = _string_cache[str];
  if (data == nullptr) {
    data = create_cache_entry(str);
    _string_cache[str] = data;
  }

  if (_texture_dirty) {
    _texture->create(_bitmap);
  }

  fw::matrix pos_transform;
  cml::matrix_orthographic_RH(pos_transform, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f, -1.0f, cml::z_clip_neg_one);
  pos_transform = fw::translation(x, y, 0.0f) * pos_transform;

  data->shader_params->set_matrix("pos_transform", pos_transform);
  data->shader_params->set_matrix("uv_transform", fw::identity());

  _texture->bind();
  data->vb->begin();
  data->ib->begin();
  data->shader->begin(data->shader_params);
  FW_CHECKED(glDrawElements(GL_TRIANGLES, data->ib->get_num_indices(), GL_UNSIGNED_SHORT, nullptr));
  data->shader->end();
  data->ib->end();
  data->vb->end();
}

string_cache_entry *font_face::create_cache_entry(std::basic_string<uint32_t> const &str) {
  ensure_glyphs(str);

  std::vector<fw::vertex::xyz_uv> verts;
  verts.reserve(str.size() * 4);
  std::vector<uint16_t> indices;
  indices.reserve(str.size() * 6);
  float x = 0;
  float y = 0;
  BOOST_FOREACH(uint32_t ch, str) {
    glyph *g = _glyphs[ch];
    uint16_t index_offset = verts.size();
    verts.push_back(fw::vertex::xyz_uv(x + g->bitmap_left, y - g->bitmap_top, 0.0f,
        static_cast<float>(g->offset_x) / _bitmap->get_width(), static_cast<float>(g->offset_y) / _bitmap->get_height()));
    verts.push_back(fw::vertex::xyz_uv(x + g->bitmap_left, y - g->bitmap_top + g->bitmap_height, 0.0f,
        static_cast<float>(g->offset_x) / _bitmap->get_width(), static_cast<float>(g->offset_y + g->bitmap_height) / _bitmap->get_height()));
    verts.push_back(fw::vertex::xyz_uv(x + g->bitmap_left + g->bitmap_width, y - g->bitmap_top + g->bitmap_height, 0.0f,
        static_cast<float>(g->offset_x + g->bitmap_width) / _bitmap->get_width(), static_cast<float>(g->offset_y + g->bitmap_height) / _bitmap->get_height()));
    verts.push_back(fw::vertex::xyz_uv(x + g->bitmap_left + g->bitmap_width, y - g->bitmap_top, 0.0f,
        static_cast<float>(g->offset_x + g->bitmap_width) / _bitmap->get_width(), static_cast<float>(g->offset_y) / _bitmap->get_height()));

    indices.push_back(index_offset);
    indices.push_back(index_offset + 1);
    indices.push_back(index_offset + 2);
    indices.push_back(index_offset);
    indices.push_back(index_offset + 2);
    indices.push_back(index_offset + 3);

    x += g->advance_x;
    y += g->advance_y;
  }

  std::shared_ptr<fw::vertex_buffer> vb = fw::vertex_buffer::create<fw::vertex::xyz_uv>();
  vb->set_data(verts.size(), verts.data());

  std::shared_ptr<fw::index_buffer> ib = std::shared_ptr<fw::index_buffer>(new fw::index_buffer());
  ib->set_data(indices.size(), indices.data());

  std::shared_ptr<fw::shader> shader = fw::shader::create("gui");
  std::shared_ptr<fw::shader_parameters> shader_params = shader->create_parameters();

  return new string_cache_entry(vb, ib, shader, shader_params);
}

//-----------------------------------------------------------------------------

void font_manager::initialize() {
  FT_CHECK(FT_Init_FreeType(&_library));
}

std::shared_ptr<font_face> font_manager::get_face() {
  return get_face(fw::resolve("gui/SaccoVanzetti.ttf"));
}

/** Gets the \ref font_face for the font at the given path (assumed to be a .ttf file). */
std::shared_ptr<font_face> font_manager::get_face(fs::path const &filename) {
  // TODO: thread-safety?
  std::shared_ptr<font_face> face = _faces[filename.string()];
  if (!face) {
    face = std::shared_ptr<font_face>(new font_face(this, filename));
    _faces[filename.string()] = face;
  }
  return face;
}

}
