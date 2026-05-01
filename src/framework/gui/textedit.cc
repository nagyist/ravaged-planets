
#include <string>

#include <SDL2/SDL.h>
#include <utf8.h>

#include <framework/framework.h>
#include <framework/gui/drawable.h>
#include <framework/gui/gui.h>
#include <framework/gui/textedit.h>
#include <framework/logging.h>
#include <framework/font.h>

#define STB_TEXTEDIT_CHARTYPE char32_t
#define STB_TEXTEDIT_POSITIONTYPE int
#define STB_TEXTEDIT_UNDOSTATECOUNT 99
#define STB_TEXTEDIT_UNDOCHARCOUNT 999
#include <stb/stb_textedit.h>

//-----------------------------------------------------------------------------

class TextEditBuffer {
public:
  STB_TexteditState state;
  std::shared_ptr<fw::FontFace> font;
  std::u32string codepoints;

  inline TextEditBuffer() {
    font = fw::Get<fw::FontManager>().get_face();
  }
};

//-----------------------------------------------------------------------------

#define STB_TEXTEDIT_IMPLEMENTATION

#define STB_TEXTEDIT_STRING TextEditBuffer
#define STB_TEXTEDIT_NEWLINE '\n'

int STB_TEXTEDIT_STRINGLEN(TextEditBuffer *buffer) {
  return buffer->codepoints.size();
}

char STB_TEXTEDIT_GETCHAR(const TextEditBuffer *buffer, int idx) {
  return buffer->codepoints[idx];
}

float STB_TEXTEDIT_GETWIDTH(TextEditBuffer *buffer, int line_start_idx, int char_idx) {
  auto size = buffer->font->measure_glyph(buffer->codepoints[char_idx]);
  if (!size.ok()) {
    return 0.0f;
  }
  return (*size)[0];
}

int STB_TEXTEDIT_KEYTOTEXT(int key) {
  if (key >= 0x10000) {
    return -1;
  }

  SDL_Keymod mod = SDL_GetModState();
  if ((mod & KMOD_SHIFT) != 0) {
    key = std::toupper(key);
  }
  return key;
}

void STB_TEXTEDIT_LAYOUTROW(StbTexteditRow *r, TextEditBuffer *buffer, int line_start_idx) {
  const char32_t *text_remaining = nullptr;

  // TODO: handle actual more than one line...
  const fw::Point size = buffer->font->measure_string(buffer->codepoints);
  r->x0 = 0.0f;
  r->x1 = size[0];
  r->baseline_y_delta = size[1];
  r->ymin = 0.0f;
  r->ymax = size[1];
  r->num_chars = static_cast<int>(buffer->codepoints.size());
}

/** Not just spaces, but basically anything we can word wrap on. */
bool STB_TEXTEDIT_IS_SPACE(char32_t ch) {
  static std::u32string spaces = utf8::utf8to32(std::string(" \t\n\r,;(){}[]<>|"));
  return spaces.find(ch) != std::string::npos;
}

void STB_TEXTEDIT_DELETECHARS(TextEditBuffer *buffer, int pos, int n) {
  buffer->codepoints.erase(pos, n);
}

bool STB_TEXTEDIT_INSERTCHARS(
    TextEditBuffer *buffer, int pos, const char32_t* new_text, int new_text_len) {
  buffer->codepoints.insert(pos, new_text, new_text_len);
  return true;
}

#define STB_TEXTEDIT_K_LEFT         SDLK_LEFT
#define STB_TEXTEDIT_K_RIGHT        SDLK_RIGHT // keyboard Input to move cursor right
#define STB_TEXTEDIT_K_UP           SDLK_UP // keyboard Input to move cursor up
#define STB_TEXTEDIT_K_DOWN         SDLK_DOWN // keyboard Input to move cursor down
#define STB_TEXTEDIT_K_PGUP         SDLK_PAGEUP // keyboard Input to move up one page
#define STB_TEXTEDIT_K_PGDOWN       SDLK_PAGEDOWN // keyboard Input to move down one page
#define STB_TEXTEDIT_K_LINESTART    SDLK_HOME // keyboard Input to move cursor to start of line
#define STB_TEXTEDIT_K_LINEEND      SDLK_END // keyboard Input to move cursor to end of line
#define STB_TEXTEDIT_K_TEXTSTART    (1<<29 | 1)  // TODO // keyboard Input to move cursor to start of text
#define STB_TEXTEDIT_K_TEXTEND      (1<<29 | 2) // TODO // keyboard Input to move cursor to end of text
#define STB_TEXTEDIT_K_DELETE       SDLK_DELETE // keyboard Input to delete selection or character under cursor
#define STB_TEXTEDIT_K_BACKSPACE    SDLK_BACKSPACE // keyboard Input to delete selection or character left of cursor
#define STB_TEXTEDIT_K_UNDO         (1<<29 | 4) // TODO
#define STB_TEXTEDIT_K_REDO         (1<<29 | 8) // TODO
#define STB_TEXTEDIT_K_WORDLEFT     (1<<29 | 16) // TODO
#define STB_TEXTEDIT_K_WORDRIGHT    (1<<29 | 32) // TODO
#define STB_TEXTEDIT_K_SHIFT        (1<<28) // This is combined with the other key codes, so must be unique bit.

#include <stb/stb_textedit.h>

//-----------------------------------------------------------------------------

namespace fw {
namespace gui {

/** Property that sets the text of the widget. */
class TextEditTextProperty : public Property {
private:
  std::string text_;
public:
  TextEditTextProperty(std::string_view text) :
    text_(text) {
  }

  void apply(Widget &widget) override {
    TextEdit &te = dynamic_cast<TextEdit &>(widget);
    te.buffer_->codepoints = utf8::utf8to32(text_);
  }
};

class TextEditFilterProperty : public Property {
private:
  std::function<bool(std::string ch)> filter_;
public:
  TextEditFilterProperty(std::function<bool(std::string ch)> filter) :
    filter_(filter) {
  }

  void apply(Widget &widget) override {
    TextEdit &te = dynamic_cast<TextEdit &>(widget);
    te.filter_ = filter_;
  }
};

//-----------------------------------------------------------------------------

TextEdit::TextEdit()
    : Widget(), buffer_(new TextEditBuffer()), cursor_flip_time_(0), draw_cursor_(true) {
  background_ = fw::Get<Gui>().get_drawable_manager().get_drawable("textedit");
  selection_background_ = fw::Get<Gui>().get_drawable_manager().get_drawable("textedit_selection");
  cursor_ = fw::Get<Gui>().get_drawable_manager().get_drawable("textedit_cursor");
  stb_textedit_initialize_state(&buffer_->state, true /* is_single_line */);
}

TextEdit::~TextEdit() {
  delete buffer_;
}

std::unique_ptr<Property> TextEdit::text(std::string_view text) {
  return std::make_unique<TextEditTextProperty>(text);
}

std::unique_ptr<Property> TextEdit::filter(std::function<bool(std::string ch)> filter) {
  return std::make_unique<TextEditFilterProperty>(filter);
}

fw::Point TextEdit::OnMeasureSelf() {
  // TODO: support multi-line?
  // TODO: measure actual text size?
  fw::Point size = fw::Get<FontManager>().get_face()->measure_string(
    "QWERTYUIOPASDFGHJKLZXCVBNMqwertyuiopasdfghjklzxcvbnm");
  return fw::Point(
    0.f,
    size[1] + padding_top_ + padding_bottom_);
}

void TextEdit::on_focus_gained() {
  Widget::on_focus_gained();
  draw_cursor_ = true;
  cursor_flip_time_ = 0.0f;
}

void TextEdit::on_focus_lost() {
  Widget::on_focus_lost();
}

bool TextEdit::on_key(int key, bool is_down) {
  // stb_textedit only supports one shift key, so if it's right-shift just translate to left
  if (key == SDLK_RSHIFT || key == SDLK_LSHIFT) {
    key = STB_TEXTEDIT_K_SHIFT;
  }

  if (is_down) {
    if (filter_) {
      std::string str(1, (char) key); // TODO: this is horrible!
      if (!filter_(str)) {
        return true;
      }
    }

    stb_textedit_key(buffer_, &buffer_->state, key);
  }
  cursor_flip_time_ = 0.0f;
  draw_cursor_ = true;
  return true;
}

bool TextEdit::on_mouse_down(float x, float y) {
  Widget::on_mouse_down(x, y);
  stb_textedit_click(buffer_, &buffer_->state, x, y);
  cursor_flip_time_ = 0.0f;
  draw_cursor_ = true;
  return true;
}

bool TextEdit::on_mouse_up(float x, float y) {
  return Widget::on_mouse_up(x, y);
}

void TextEdit::set_filter(std::function<bool(std::string ch)> filter) {
  filter_ = filter;
}

void TextEdit::select_all() {
  buffer_->state.select_start = 0;
  buffer_->state.cursor = buffer_->state.select_end = buffer_->codepoints.size();
}

std::string TextEdit::get_text() const {
  return utf8::utf32to8(buffer_->codepoints);
}

void TextEdit::set_text(std::string_view text) {
  buffer_->codepoints = utf8::utf8to32(text);
  if (buffer_->state.cursor > buffer_->codepoints.size()) {
		buffer_->state.cursor = buffer_->codepoints.size();
  }
}

std::string TextEdit::get_cursor_name() const {
  return "i-beam";
}

void TextEdit::update(float dt) {
  if (focused_) {
    cursor_flip_time_ += dt;
    if (cursor_flip_time_ > 0.75f) {
      draw_cursor_ = !draw_cursor_;
      cursor_flip_time_ = 0.0f;
    }
  }
}

void TextEdit::render() {
	auto rect = GetScreenRect();
  background_->render(rect.left, rect.top, rect.width, rect.height);

  if (buffer_->state.select_start != buffer_->state.select_end) {
    float left = rect.left /* + something */;
    float width = rect.top + 100 /*something else */;
    selection_background_->render(left, rect.top + 1, width, rect.height - 2);
  }

  if (!buffer_->codepoints.empty()) {
    fw::Get<FontManager>().get_face()->draw_string(
        rect.left + padding_left_, rect.top + rect.height / 2, buffer_->codepoints,
        static_cast<fw::FontFace::DrawFlags>(fw::FontFace::kAlignLeft | fw::FontFace::kAlignMiddle),
        fw::Color::BLACK());
  }

  if (focused_ && draw_cursor_) {
    int cursor_pos = buffer_->state.cursor;
    fw::Point size_to_cursor = buffer_->font->measure_substring(buffer_->codepoints, 0, cursor_pos);
    float left = rect.left + size_to_cursor[0] + padding_left_;
    cursor_->render(left, rect.top + 2, 1.0f, rect.height - 4.0f);
  }
}

}
}
