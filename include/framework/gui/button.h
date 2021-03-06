#pragma once

#include <memory>
#include <string>

#include <framework/gui/widget.h>

namespace fw { namespace gui {
class gui;
class drawable;

/** Buttons have a background image and text (or image, or both). */
class button : public widget {
public:
  enum alignment {
    left,
    center,
    right
  };

protected:
  friend class button_background_property;
  friend class button_text_property;
  friend class button_text_align_property;
  friend class button_icon_property;

  std::shared_ptr<drawable> _background;
  std::shared_ptr<drawable> _icon;
  std::string _text;
  alignment _text_align;
  bool _is_pressed;
  bool _is_mouse_over;

  void on_mouse_out();
  void on_mouse_over();

  void update_drawable_state();

public:
  button(gui *gui);
  virtual ~button();

  static property *background(std::string const &drawable_name);
  static property *background(std::shared_ptr<drawable> drawable);
  static property *icon(std::string const &drawable_name);
  static property *icon(std::shared_ptr<drawable> drawable);
  static property *text(std::string const &text);
  static property *text_align(alignment align);

  void on_attached_to_parent(widget *parent);
  void render();

  inline void set_text(std::string const &new_text) {
    _text = new_text;
  }
  inline std::string get_text() const {
    return _text;
  }
  inline void set_icon(std::shared_ptr<drawable> drawable) {
    _icon = drawable;
  }

  void set_pressed(bool is_pressed);
  bool is_pressed() {
    return _is_pressed;
  }
};

} }
