#include <framework/cursor.h>

#include <SDL2/SDL.h>

#include <framework/bitmap.h>
#include <framework/framework.h>
#include <framework/graphics.h>
#include <framework/status.h>
#include <framework/paths.h>

namespace fw {

Cursor::Cursor() : cursor_visible_(true) {
}

Cursor::~Cursor() {
}

void Cursor::initialize() {
  set_cursor(0, "arrow");
}

void Cursor::destroy() {
  for(auto it : loaded_cursors_) {
    SDL_FreeCursor(it.second);
  }
  loaded_cursors_.clear();
}

StatusOr<SDL_Cursor *> Cursor::load_cursor(std::string const &name) {
  ASSIGN_OR_RETURN(fw::Bitmap bmp, LoadBitmap(fw::resolve("cursors/" + name + ".png")));

  int hot_x = bmp.get_width() / 2;
  int hot_y = bmp.get_height() / 2;
  if (name == "arrow") {
    // TODO: hard coding this seems a bit... flaky.
    hot_x = hot_y = 0;
  }

  // Well this is a bit annoying.
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    int rmask = 0xff000000;
    int gmask = 0x00ff0000;
    int bmask = 0x0000ff00;
    int amask = 0x000000ff;
#else
    int rmask = 0x000000ff;
    int gmask = 0x0000ff00;
    int bmask = 0x00ff0000;
    int amask = 0xff000000;
#endif

  SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(
      const_cast<void *>(reinterpret_cast<void const *>(bmp.GetPixels().data())),
      bmp.get_width(), bmp.get_height(), 32, bmp.get_width() * 4, rmask, gmask, bmask, amask);
  SDL_Cursor *cursor = SDL_CreateColorCursor(surface, hot_x, hot_y);
  SDL_FreeSurface(surface);
  return cursor;
}

void Cursor::set_cursor_for_real(std::string const &name) {
  SDL_Cursor *cursor;
  auto it = loaded_cursors_.find(name);
  if (it == loaded_cursors_.end()) {
    auto new_cursor = load_cursor(name);
    if (!new_cursor.ok()) {
      LOG(ERR) << "error loading cursor: " << new_cursor.status();
    } else {
      cursor = *new_cursor;
      loaded_cursors_[name] = cursor;
    }
  } else {
    cursor = it->second;
  }

  SDL_SetCursor(cursor);
}

void Cursor::update_cursor() {
  std::map<int, std::string>::reverse_iterator cit = cursor_stack_.rbegin();
  if (cit != cursor_stack_.rend()) {
     std::string cursor_name = cit->second;
     set_cursor_for_real(cursor_name);
  }
}

void Cursor::set_cursor(int priority, std::string const &name) {
  if (name == "") {
    auto it = cursor_stack_.find(priority);
    if (it != cursor_stack_.end()) {
      cursor_stack_.erase(it);
    }
  } else {
    cursor_stack_[priority] = name;
  }

  update_cursor();
}

void Cursor::set_visible(bool is_visible) {
  SDL_ShowCursor(is_visible ? 1 : 0);
  cursor_visible_ = is_visible;
}

bool Cursor::is_visible() {
  return cursor_visible_;
}

}
