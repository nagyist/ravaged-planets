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
typedef struct FT_FaceRec_ *FT_Face;

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

  enum MeasureFlags {
    kMeasureDefault      = 0x0000,
    // Measure the actual height of the text. Normally, we just return the pixel size as the
    // height. But with this flag we measure the actual height of the glyphs (e.g. a period will
    // be around 1px height).
    kMeasureActualHeight = 0x0001,
  };

  explicit FontFace(int size = 16);
  ~FontFace();

  fw::Status Initialize(std::filesystem::path const& filename);

  /** Called by the font_managed every update frame. */
  void Update(float dt);

  // Only useful for debugging, gets the atlas bitmap we're using to hold rendered glyphs
  std::shared_ptr<fw::Bitmap> get_bitmap() const {
    return bitmap_;
  }

  /**
   * Pre-renders all of the glyphs required to render the given string, useful when starting up to
   * ensure common characters are already available.
   */
  void EnsureGlyphs(std::string_view str);

  /** Measures the given string and returns the width/height of the final rendered string. */
  fw::Point MeasureString(
      std::string_view str, MeasureFlags flags = MeasureFlags::kMeasureDefault);
  /** Measures the given string and returns the width/height of the final rendered string. */
  fw::Point MeasureString(
      std::u32string_view str, MeasureFlags flags = MeasureFlags::kMeasureDefault);
  /** Measures the given string and returns the width/height of the final rendered string. */
  fw::Point MeasureSubstring(
      std::u32string_view str,
      int pos,
      int num_chars,
      MeasureFlags flags = MeasureFlags::kMeasureDefault);

  /** Measures a single glyph. */
  fw::StatusOr<fw::Point> MeasureGlyph(char32_t ch);

  /** Draws the given string on the screen at the given (x,y) coordinates. */
  void DrawString(
    int x, int y, std::string const& str, DrawFlags flags = kDrawDefault,
    fw::Color color = fw::Color::WHITE());

  /** Draws the given string on the screen at the given (x,y) coordinates. */
  void DrawString(
    int x, int y, std::u32string_view str, DrawFlags flags, fw::Color color);

private:
  fw::Status EnsureGlyph(char32_t ch);
  void EnsureGlyphs(std::u32string_view str);
  std::shared_ptr<StringCacheEntry> GetOrCreateCacheEntry(std::u32string_view str);
  std::shared_ptr<StringCacheEntry> CreateCacheEntry(std::u32string_view str);

  FT_Face face_;
  int size_; //<! Size in pixels of this font.
  std::mutex mutex_;

  // Glyphs are rendered into a bitmap and then copied to a texture as required, before we render
  // when draw is called.
  std::shared_ptr<fw::Bitmap> bitmap_;
  std::shared_ptr<fw::Texture> texture_;
  bool texture_dirty_ = false;

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
};

inline FontFace::DrawFlags operator |(FontFace::DrawFlags lhs, FontFace::DrawFlags rhs) {
  using T = std::underlying_type_t<FontFace::DrawFlags>;
  return static_cast<FontFace::DrawFlags>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

inline FontFace::MeasureFlags operator |(FontFace::MeasureFlags lhs, FontFace::MeasureFlags rhs) {
  using T = std::underlying_type_t<FontFace::MeasureFlags>;
  return static_cast<FontFace::MeasureFlags>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

class FontManager {
public:
  static std::string service_name;

public:
  fw::Status Initialize();
  void Update(float dt);

  /** Gets the default \ref font_face. */
  std::shared_ptr<FontFace> GetFace();

  /** Gets the \ref font_face for the font at the given path (assumed to be a .ttf file). */
  std::shared_ptr<FontFace> GetFace(std::filesystem::path const &filename);

private:
  friend class FontFace;

  FT_Library library_;
  std::map<std::string, std::shared_ptr<FontFace>> faces_;
};

}
