#pragma once

#include <memory>
#include <map>
#include <mutex>
#include <set>
#include <string>

#include <framework/bitmap.h>
#include <framework/color.h>
#include <framework/math.h>
#include <framework/texture.h>
#include <framework/status.h>

/* Cut'n'pasted from the freetype.h header so we don't have to include that whole thing. */
typedef struct FT_LibraryRec_ *FT_Library;
typedef struct FT_FaceRec_*  FT_Face;

namespace fw {
class Glyph;
class StringCacheEntry;

class FontFace {
public:
  /** Flags we use to control how the string is drawn. */
  enum DrawFlags {
    kDrawDefault   = 0x0000,
    kAlignBaseline = 0x0000,
    kAlignLeft     = 0x0000,
    kAlignTop      = 0x0001,
    kAlignBottom   = 0x0002,
    kAlignMiddle   = 0x0004,
    kAlignCenter   = 0x0008,
    kAlignRight    = 0x0010,
  };

private:
  FT_Face face_;
  int size_; //<! Size in pixels of this font.
  std::mutex mutex_;

  // Glyphs are rendered into a bitmap and then copied to a texture as required, before we render
  // when draw is called.
  std::shared_ptr<fw::Bitmap> bitmap_;
  std::shared_ptr<fw::Texture> texture_;
  bool texture_dirty_;

  /** Mapping of UTF-32 character to glyph object describing the glyph. */
  std::map<char32_t, Glyph *> glyphs_;

  // If we try to ensure a glyph and get error, they're added here so that we don't try over and
  // over.
  std::set<char32_t> error_glyphs_;

  /**
   * Mapping of UTF-32 strings to a \ref string_cache_entry which caches the data we need to draw
   * the given string.
   */
  std::map<std::u32string, std::shared_ptr<StringCacheEntry>> string_cache_;

  fw::Status ensure_glyph(char32_t ch);
  void ensure_glyphs(std::u32string_view str);
  std::shared_ptr<StringCacheEntry> get_or_create_cache_entry(std::u32string_view str);
  std::shared_ptr<StringCacheEntry> create_cache_entry(std::u32string_view str);
public:
  FontFace();
  ~FontFace();

  fw::Status initialize(std::filesystem::path const &filename);

  /** Called by the font_managed every update frame. */
  void update(float dt);

  // Only useful for debugging, gets the atlas bitmap we're using to hold rendered glyphs
  std::shared_ptr<fw::Bitmap> get_bitmap() const {
    return bitmap_;
  }

  /**
   * Pre-renders all of the glyphs required to render the given string, useful when starting up to
   * ensure common characters are already available.
   */
  void ensure_glyphs(std::string_view str);

  /** Measures the given string and returns the width/height of the final rendered string. */
  fw::Point measure_string(std::string_view str);
  fw::Point measure_string(std::u32string_view str);
  fw::Point measure_substring(std::u32string_view str, int pos, int num_chars);

  /** Measures a single glyph. */
  fw::StatusOr<fw::Point> measure_glyph(char32_t ch);

  /**
   * Draws the given string on the Screen at the given (x,y) coordinates.
   */
  void draw_string(
      int x, int y, std::string const &str, DrawFlags flags = kDrawDefault,
      fw::Color color = fw::Color::WHITE());
  void draw_string(
      int x, int y, std::u32string_view str, DrawFlags flags, fw::Color color);
};

class FontManager {
public:
  static std::string service_name;

public:
  fw::Status initialize();
  void update(float dt);

  /** Gets the default \ref font_face. */
  std::shared_ptr<FontFace> get_face();

  /** Gets the \ref font_face for the font at the given path (assumed to be a .ttf file). */
  std::shared_ptr<FontFace> get_face(std::filesystem::path const &filename);

private:
  friend class FontFace;

  FT_Library library_;
  std::map<std::string, std::shared_ptr<FontFace>> faces_;
};

}
