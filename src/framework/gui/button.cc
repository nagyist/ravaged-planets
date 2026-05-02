
#include <framework/audio.h>
#include <framework/framework.h>
#include <framework/font.h>
#include <framework/gui/drawable.h>
#include <framework/gui/gui.h>
#include <framework/gui/button.h>

namespace fw::gui {

// Property that sets the background of the button.
class ButtonBackgroundProperty : public Property {
private:
  std::string drawable_name_;
  std::shared_ptr<Drawable> drawable_;
public:
  ButtonBackgroundProperty(std::string_view drawable_name) :
      drawable_name_(drawable_name) {
  }
  ButtonBackgroundProperty(std::shared_ptr<Drawable> drawable) :
      drawable_(drawable) {
  }

  void apply(Widget &widget) override {
    Button* btn = dynamic_cast<Button*>(&widget);
    if (!btn) {
      LOG(WARN) << "Button::background set on non-button widget " << widget.get_name();
      return;
    }
    if (drawable_) {
      btn->background_ = drawable_;
    } else {
      btn->background_ = fw::Get<Gui>().get_drawable_manager().get_drawable(drawable_name_);
    }
  }
};

// Property that sets the icon of the button.
class ButtonIconProperty : public Property {
private:
  std::string drawable_name_;
  std::shared_ptr<Drawable> drawable_;
public:
  ButtonIconProperty(std::string_view drawable_name) :
      drawable_name_(drawable_name) {
  }
  ButtonIconProperty(std::shared_ptr<Drawable> drawable) :
      drawable_(drawable) {
  }

  void apply(Widget &widget) override {
    Button* btn = dynamic_cast<Button*>(&widget);
    if (!btn) {
      LOG(WARN) << "Button::icon set on non-button widget " << widget.get_name();
      return;
    }
    if (drawable_) {
      btn->icon_ = drawable_;
    } else {
      btn->icon_ = fw::Get<Gui>().get_drawable_manager().get_drawable(drawable_name_);
    }
  }
};

// Property that sets the text of the button.
class ButtonTextProperty : public Property {
private:
  std::string text_;
public:
  ButtonTextProperty(std::string_view text) :
      text_(text) {
  }

  void apply(Widget &widget) override {
    Button *btn = dynamic_cast<Button *>(&widget);
    if (!btn) {
			LOG(WARN) << "Button::text set on non-button widget " << widget.get_name();
      return;
    }
    btn->text_ = text_;
  }
};

// Property that sets the alignment of text on the button.
class ButtonTextAlignProperty : public Property {
private:
  Button::Alignment text_align_;
public:
  ButtonTextAlignProperty(Button::Alignment text_align) :
      text_align_(text_align) {
  }

  void apply(Widget &widget) override {
    Button &btn = dynamic_cast<Button &>(widget);
    btn.text_align_ = text_align_;
  }
};

//-----------------------------------------------------------------------------

static std::shared_ptr<fw::AudioBuffer> g_hover_sound;

Button::Button()
    : Widget(), text_align_(kCenter), is_pressed_(false), is_mouse_over_(false) {
  sig_mouse_out.Connect(std::bind(&Button::on_mouse_out, this));
  sig_mouse_over.Connect(std::bind(&Button::on_mouse_over, this));

  if (!g_hover_sound) {
    g_hover_sound =
        fw::Framework::get_instance()->get_audio_manager()->get_audio_buffer(
            "gui/sounds/click.ogg");
  }
}

Button::~Button() {
}

std::unique_ptr<Property> Button::background(std::string_view drawable_name) {
  return std::make_unique<ButtonBackgroundProperty>(drawable_name);
}

std::unique_ptr<Property> Button::background(std::shared_ptr<Drawable> drawable) {
  return std::make_unique<ButtonBackgroundProperty>(drawable);
}

std::unique_ptr<Property> Button::icon(std::string_view drawable_name) {
  return std::make_unique<ButtonIconProperty>(drawable_name);
}

std::unique_ptr<Property> Button::icon(std::shared_ptr<Drawable> drawable) {
  return std::make_unique<ButtonIconProperty>(drawable);
}

std::unique_ptr<Property> Button::text(std::string_view text) {
  return std::make_unique<ButtonTextProperty>(text);
}

std::unique_ptr<Property> Button::text_align(Button::Alignment align) {
  return std::make_unique<ButtonTextAlignProperty>(align);
}

void Button::OnAttachedToParent(Widget &parent) {
	Widget::OnAttachedToParent(parent);

  // Assign default values for things that haven't been overwritten.
  if (!background_) {
    StateDrawable *bkgnd = new StateDrawable();
    bkgnd->add_drawable(
        StateDrawable::kNormal, fw::Get<Gui>().get_drawable_manager().get_drawable("button_normal"));
    bkgnd->add_drawable(
        StateDrawable::kHover, fw::Get<Gui>().get_drawable_manager().get_drawable("button_hover"));
    bkgnd->add_drawable(
        StateDrawable::kPressed, fw::Get<Gui>().get_drawable_manager().get_drawable("button_hover"));
    background_ = std::shared_ptr<Drawable>(bkgnd);
  }
}

void Button::on_mouse_out() {
  is_mouse_over_ = false;
  update_drawable_state();
}

void Button::on_mouse_over() {
  is_mouse_over_ = true;
  update_drawable_state();

  fw::Get<Gui>().get_audio_source()->play(g_hover_sound);
}

void Button::set_pressed(bool is_pressed) {
  is_pressed_ = is_pressed;
  update_drawable_state();
}

void Button::update_drawable_state() {
  StateDrawable *drawable = dynamic_cast<StateDrawable *>(background_.get());
  if (drawable == nullptr) {
    return;
  }

  if (!is_enabled()) {
    drawable->set_current_state(StateDrawable::kDisabled);
  } else if (is_pressed_) {
    drawable->set_current_state(StateDrawable::kPressed);
  } else if (is_mouse_over_) {
    drawable->set_current_state(StateDrawable::kHover);
  } else {
    drawable->set_current_state(StateDrawable::kNormal);
  }
}

Point Button::OnMeasureSelf() {
  float max_width = 0.0f;
  float max_height = 0.0f;

  if (background_) {
    max_width = background_->get_intrinsic_width();
    max_height = background_->get_intrinsic_height();
  }

  auto font_face = fw::Get<FontManager>().GetFace();
  auto text_size = font_face->MeasureString(text_);
	float text_width = text_size.v[0];
	float text_height = text_size.v[1];

	if (icon_) {
    float icon_width = icon_->get_intrinsic_width();
    float icon_height = icon_->get_intrinsic_height();
    text_width += icon_width;
    text_height = std::max(text_height, icon_height);
  }

  max_width = std::max(max_width, text_width);
  max_height = std::max(max_height, text_height);

	max_width += padding_left_ + padding_right_;
	max_height += padding_top_ + padding_bottom_;

  return Point(max_width, max_height);
}

void Button::render() {
  auto rect = GetScreenRect();

  if (background_) {
    background_->render(rect.left, rect.top, rect.width, rect.height);
  }

  if (icon_) {
    float icon_width = icon_->get_intrinsic_width();
    float icon_height = icon_->get_intrinsic_height();
    if (icon_width == 0.0f) {
      icon_width = rect.width * 0.75f;
    }
    if (icon_height == 0.0f) {
      icon_height = rect.height * 0.75f;
    }
    float x = rect.left + (rect.width / 2.0f) - (icon_width / 2.0f);
    float y = rect.top + (rect.height / 2.0f) - (icon_height / 2.0f);
    icon_->render(x, y, icon_width, icon_height);
  }

  if (text_.length() > 0) {
    if (text_align_ == kLeft) {
      fw::Get<FontManager>().GetFace()->DrawString(
          rect.left + padding_left_,
          rect.top + rect.height / 2,
          text_,
          fw::FontFace::kAlignLeft | fw::FontFace::kAlignMiddle);
    } else if (text_align_ == kCenter) {
      fw::Get<FontManager>().GetFace()->DrawString(
          rect.left + rect.width / 2,
          rect.top + rect.height / 2,
          text_,
          fw::FontFace::kAlignCenter | fw::FontFace::kAlignMiddle);
    }
  }
}

}
