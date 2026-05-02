
#include <framework/framework.h>
#include <framework/font.h>
#include <framework/gui/drawable.h>
#include <framework/gui/gui.h>
#include <framework/gui/label.h>
#include <framework/texture.h>

namespace fw::gui {

// Property that sets the background of the widget.
class LabelBackgroundProperty : public Property {
private:
  std::string drawable_name_;
  bool centred_;
public:
  LabelBackgroundProperty(std::string_view drawable_name, bool centred)
      : drawable_name_(drawable_name), centred_(centred) {
  }

  void apply(Widget &widget) override {
    Label &label = dynamic_cast<Label &>(widget);
    label.background_ = fw::Get<Gui>().get_drawable_manager().get_drawable(drawable_name_);
    label.background_centred_ = centred_;
  }
};

// Property that sets the text of the widget.
class LabelTextProperty : public Property {
private:
  std::string text_;
public:
    LabelTextProperty(std::string_view text)
        : text_(text) {
  }

  void apply(Widget &widget) override {
    Label &label = dynamic_cast<Label &>(widget);
    label.text_ = text_;
  }
};

// Property that sets the text of the widget.
class LabelTextAlignProperty : public Property {
private:
  Label::Alignment text_alignment_;
public:
    LabelTextAlignProperty(Label::Alignment text_alignment)
        : text_alignment_(text_alignment) {
  }

  void apply(Widget &widget) override {
    Label &label = dynamic_cast<Label &>(widget);
    label.text_alignment_ = text_alignment_;
  }
};

Label::Label()
  : Widget(),
    background_centred_(false),
    text_alignment_(Alignment::kLeft) {
}

Label::~Label() {
}

std::unique_ptr<Property> Label::background(
    std::string_view drawable_name, bool centred /*= false */) {
  return std::make_unique<LabelBackgroundProperty>(drawable_name, centred);
}

std::unique_ptr<Property> Label::text(std::string_view text) {
  return std::make_unique<LabelTextProperty>(text);
}

std::unique_ptr<Property> Label::text_align(Label::Alignment text_alignment) {
  return std::make_unique<LabelTextAlignProperty>(text_alignment);
}

Point Label::OnMeasureSelf() {
	float max_width = 0.0f;
	float max_height = 0.0f;

  if (background_) {
    max_width = background_->get_intrinsic_width();
    max_height = background_->get_intrinsic_height();
	}

  auto font_face = fw::Get<FontManager>().GetFace();
  auto text_size = font_face->MeasureString(text_);

	max_width = std::max(max_width, text_size.v[0]);
	max_height = std::max(max_height, text_size.v[1]);

	return Point(
      max_width + padding_left_ + padding_right_,
      max_height + padding_top_ + padding_bottom_);
}

void Label::render() {
	auto rect = GetScreenRect();
  if (background_) {
    if (background_centred_) {
      float background_width = background_->get_intrinsic_width();
      float background_height = background_->get_intrinsic_height();
      if (background_width == 0.0f) {
        background_width = rect.width;
      }
      if (background_height == 0.0f) {
        background_height = rect.height;
      }
      float x = rect.left + (rect.width / 2.0f) - (background_width / 2.0f);
      float y = rect.top + (rect.height / 2.0f) - (background_height / 2.0f);
      background_->render(x, y, background_width, background_height);
    } else {
      background_->render(rect.left, rect.top, rect.width, rect.height);
    }
  }
  if (text_ != "") {
    switch (text_alignment_) {
    case Alignment::kLeft:
      fw::Get<FontManager>().GetFace()->DrawString(
        rect.left + padding_left_,
        rect.top + rect.height / 2.f + padding_top_,
        text_,
        static_cast<FontFace::DrawFlags>(FontFace::kAlignLeft | FontFace::kAlignMiddle));
      break;
    case Alignment::kCenter:
      fw::Get<FontManager>().GetFace()->DrawString(
        rect.left + rect.width / 2 + padding_left_,
        rect.top + rect.height / 2 + padding_top_,
        text_,
        static_cast<FontFace::DrawFlags>(FontFace::kAlignCenter | FontFace::kAlignMiddle));
      break;
    case Alignment::kRight:
      fw::Get<FontManager>().GetFace()->DrawString(
          rect.left + rect.width - padding_right_,
          rect.top + rect.height / 2 + padding_top_,
          text_,
          static_cast<FontFace::DrawFlags>(FontFace::kAlignRight | FontFace::kAlignMiddle));
      break;
    }
  }
}

void Label::set_text(std::string_view text) {
  text_ = std::string(text);
}

std::string Label::get_text() const {
  return text_;
}

void Label::set_background(std::shared_ptr<Drawable> background, bool centred /*= false */) {
  background_ = background;
  background_centred_ = centred;
}

void Label::set_background(Bitmap const &bmp, bool centred /*= false*/) {
  std::shared_ptr<fw::Texture> texture(new fw::Texture());
  texture->create(bmp);
  background_ =
    fw::Get<Gui>().get_drawable_manager().build_drawable(
          texture, 0, 0, bmp.get_width(), bmp.get_height());
  background_centred_ = centred;
}

}
