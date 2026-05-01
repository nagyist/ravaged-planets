
#include <framework/gui/drawable.h>
#include <framework/gui/gui.h>
#include <framework/gui/checkbox.h>

#include <framework/framework.h>
#include <framework/font.h>


namespace fw::gui {

/** Property that sets the text of the checkbox. */
class CheckboxTextProperty : public Property {
private:
  std::string text_;
public:
  CheckboxTextProperty(std::string_view text) :
      text_(text) {
  }

  void apply(Widget &widget) override {
    Checkbox &chbx = dynamic_cast<Checkbox &>(widget);
    chbx.text_ = text_;
  }
};

//-----------------------------------------------------------------------------

Checkbox::Checkbox() : Widget(), is_checked_(false), is_mouse_over_(false) {
}

Checkbox::~Checkbox() {
}

std::unique_ptr<Property> Checkbox::text(std::string_view text) {
  return std::make_unique<CheckboxTextProperty>(text);
}

void Checkbox::OnAttachedToParent(Widget &parent) {
  Widget::OnAttachedToParent(parent);

  StateDrawable *bkgnd = new StateDrawable();
  bkgnd->add_drawable(
      StateDrawable::kNormal, fw::Get<Gui>().get_drawable_manager().get_drawable("button_normal"));
  bkgnd->add_drawable(
      StateDrawable::kHover, fw::Get<Gui>().get_drawable_manager().get_drawable("button_hover"));
  background_ = std::shared_ptr<Drawable>(bkgnd);

  check_icon_ = fw::Get<Gui>().get_drawable_manager().get_drawable("checkbox");
}

fw::Point Checkbox::OnMeasureSelf() {
  fw::Point text_size = 
    fw::Get<FontManager>().get_face()->measure_string(text_);
  return fw::Point(
    check_icon_->get_intrinsic_width() + 12.f + text_size[0],
    std::max(check_icon_->get_intrinsic_height() + 8.f, text_size[1]));
}

void Checkbox::on_mouse_out() {
  is_mouse_over_ = false;
  update_drawable_state();
}

void Checkbox::on_mouse_over() {
  is_mouse_over_ = true;
  update_drawable_state();
}

bool Checkbox::on_mouse_down(float x, float y) {
  set_checked(!is_checked_);
  return Widget::on_mouse_down(x, y);
}

void Checkbox::set_checked(bool is_checked) {
  is_checked_ = is_checked;
  update_drawable_state();
}

void Checkbox::update_drawable_state() {
  StateDrawable *drawable = dynamic_cast<StateDrawable *>(background_.get());
  if (is_mouse_over_) {
    drawable->set_current_state(StateDrawable::kHover);
  } else {
    drawable->set_current_state(StateDrawable::kNormal);
  }
}

void Checkbox::render() {
	auto rect = GetScreenRect();

  float icon_width = check_icon_->get_intrinsic_width();
  float icon_height = check_icon_->get_intrinsic_height();

  fw::Rectangle<float> icon_rect(
    rect.left + 4.f,
    rect.top + (rect.height / 2.0f) - (icon_height / 2.0f),
    icon_width,
    icon_height);
  background_->Render(icon_rect.Grow(4.0f));

  if (is_checked_) {
    float x = rect.left + (rect.height / 2.0f) - (icon_width / 2.0f);
    float y = rect.top + (rect.height / 2.0f) - (icon_height / 2.0f);
    check_icon_->render(icon_rect.left, icon_rect.top, icon_rect.width, icon_rect.height);
  }

  if (text_.length() > 0) {
    fw::Get<FontManager>().get_face()->draw_string(
        rect.left + icon_rect.width + 12.f,
        rect.top + rect.height / 2,
        text_,
        static_cast<fw::FontFace::DrawFlags>(fw::FontFace::kAlignLeft | fw::FontFace::kAlignMiddle));
  }
}

}
