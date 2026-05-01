#include <framework/bitmap.h>
#include <framework/framework.h>
#include <framework/graphics.h>
#include <framework/gui/gui.h>
#include <framework/gui/builder.h>
#include <framework/gui/grid_layout.h>
#include <framework/gui/label.h>
#include <framework/gui/linear_layout.h>
#include <framework/gui/textedit.h>
#include <framework/gui/window.h>
#include <framework/gui/button.h>
#include <framework/misc.h>
#include <framework/texture.h>

#include <game/world/world.h>
#include <game/editor/editor_screen.h>
#include <game/editor/editor_world.h>
#include <game/editor/world_writer.h>
#include <game/editor/windows/save_map.h>

using namespace fw::gui;
using namespace std::placeholders;

namespace ed {

std::unique_ptr<SaveMapWindow> save_map;

enum ids {
  NAME_ID = 1343,
  AUTHOR_ID,
  DESCRIPTION_ID,
  SCREENSHOT_ID
};

SaveMapWindow::SaveMapWindow() : wnd_(nullptr) {
}

SaveMapWindow::~SaveMapWindow() {
}

void SaveMapWindow::initialize() {
  wnd_ = Builder<Window>()
      << Widget::width(Widget::Fixed(500.f))
      << Widget::height(Widget::WrapContent())
      << Window::initial_position(WindowInitialPosition::Center())
      << Widget::background("frame")
      << Widget::visible(false)
      << (Builder<LinearLayout>()
          << Widget::width(Widget::MatchParent())
          << Widget::height(Widget::WrapContent())
          << Widget::margin(0.f, 0.f, 50.f, 0.f)
          << LinearLayout::orientation(LinearLayout::Orientation::kHorizontal)
          << (Builder<GridLayout>()
              << LinearLayout::weight(1.0f)
              << Widget::height(Widget::WrapContent())
              << (Builder<Label>()
                  << Widget::width(Widget::WrapContent())
                  << Widget::height(Widget::WrapContent())
                  << Widget::margin(10.f, 5.0f, 5.0f, 10.f)
                  << Label::text("Name:"))
              << (Builder<TextEdit>()
                  << Widget::width(Widget::MatchParent())
                  << Widget::height(Widget::WrapContent())
                  << Widget::margin(10.f, 10.0f, 5.0f, 5.f)
                  << Widget::padding(4.f, 4.f, 4.f, 4.f)
                  << Widget::id(NAME_ID))
              << (Builder<Label>()
                  << Widget::width(Widget::WrapContent())
                  << Widget::height(Widget::WrapContent())
                  << Widget::margin(5.f, 5.0f, 5.0f, 10.f)
                  << Label::text("Author:"))
              << (Builder<TextEdit>()
                  << Widget::width(Widget::MatchParent())
                  << Widget::height(Widget::WrapContent())
                  << Widget::margin(5.f, 10.0f, 5.0f, 5.f)
                  << Widget::padding(4.f, 4.f, 4.f, 4.f)
                  << Widget::id(AUTHOR_ID))
              << (Builder<Label>()
                  << Widget::width(Widget::WrapContent())
                  << Widget::height(Widget::WrapContent())
                  << Widget::margin(5.f, 5.0f, 10.0f, 10.f)
                  << Label::text("Description:"))
              << (Builder<TextEdit>()
                  << Widget::width(Widget::MatchParent())
                  << Widget::height(Widget::WrapContent())
                  << Widget::margin(5.f, 10.0f, 10.0f, 5.f)
                  << Widget::padding(4.f, 4.f, 4.f, 4.f)
                  << Widget::id(DESCRIPTION_ID))
            )
          << (Builder<LinearLayout>()
              << Widget::width(Widget::WrapContent())
              << Widget::height(Widget::MatchParent())
              << LinearLayout::orientation(LinearLayout::Orientation::kVertical)
                  << (Builder<Label>()
                      << Widget::width(Widget::Fixed(160.0f))
                      << Widget::height(Widget::Fixed(120.0f))
                      << Widget::margin(10.f, 10.f, 10.f, 0.f)
                      << Widget::background("frame")
                      << Widget::id(SCREENSHOT_ID))
                  << (Builder<Button>()
                      << Widget::width(Widget::WrapContent())
                      << Widget::height(Widget::Fixed(30.0f))
                      << Widget::padding(0.f, 8.f, 0.f, 8.f)
                      << Widget::gravity(LayoutParams::Gravity::kCenterHorizontal)
                      << Button::text("Update screenshot")
                      << Widget::click(std::bind(&SaveMapWindow::screenshot_clicked, this, _1)))
              )
          )
      << (Builder<LinearLayout>()
          << Widget::width(Widget::WrapContent())
          << Widget::height(Widget::WrapContent())
          << Widget::gravity(LayoutParams::Gravity::kBottom | LayoutParams::Gravity::kRight)
          << Widget::margin(0.f, 10.f, 10.f, 0.f)
          << LinearLayout::orientation(LinearLayout::Orientation::kHorizontal)
          << (Builder<Button>()
              << Widget::width(Widget::Fixed(100.f))
              << Widget::height(Widget::Fixed(30.f))
              << Button::text("Cancel")
              << Widget::click(std::bind(&SaveMapWindow::cancel_clicked, this, _1)))
          << (Builder<Button>()
              << Widget::width(Widget::Fixed(100.f))
              << Widget::height(Widget::Fixed(30.f))
              << Button::text("Save")
              << Widget::click(std::bind(&SaveMapWindow::save_clicked, this, _1)))
          );
  fw::Get<Gui>().AttachWindow(wnd_);
}

// when we go to show, we have to update our controls with what we currently know about the map
// we're editing.
void SaveMapWindow::show() {
  wnd_->set_visible(true);

  auto world = dynamic_cast<EditorWorld *>(game::World::get_instance());

  wnd_->Find<TextEdit>(NAME_ID)->set_text(world->get_name());
  wnd_->Find<TextEdit>(DESCRIPTION_ID)->set_text(world->get_description());
  if (world->get_author() == "") {
    wnd_->Find<TextEdit>(AUTHOR_ID)->set_text(fw::get_user_name());
  } else {
    wnd_->Find<TextEdit>(AUTHOR_ID)->set_text(world->get_author());
  }
  update_screenshot();
}

void SaveMapWindow::hide() {
  // TODO?
}

// updates the screenshot that we're displaying whenever it changes.
void SaveMapWindow::update_screenshot() {
  auto world = dynamic_cast<EditorWorld *>(game::World::get_instance());
  if (world->get_screenshot().get_width() == 0)
    return;

  fw::Bitmap const &bmp = world->get_screenshot();
  wnd_->Find<Label>(SCREENSHOT_ID)->set_background(bmp);
}

bool SaveMapWindow::screenshot_clicked(Widget &w) {
  auto world = dynamic_cast<EditorWorld *>(game::World::get_instance());
  fw::Framework::get_instance()->take_screenshot(
      1024, 768, std::bind(&SaveMapWindow::screenshot_complete, this, _1), false);
  return true;
}

void SaveMapWindow::screenshot_complete(fw::Bitmap const &bitmap) {
  fw::Bitmap resized = bitmap;
  resized.resize(640, 480);

  auto world = dynamic_cast<EditorWorld *>(game::World::get_instance());
  world->set_screenshot(resized);
  fw::Get<fw::Graphics>().run_on_render_thread([this]() {
    update_screenshot();
  });
}

bool SaveMapWindow::save_clicked(Widget &w) {
  auto world = dynamic_cast<EditorWorld *>(game::World::get_instance());
  world->set_name(wnd_->Find<TextEdit>(NAME_ID)->get_text());
  world->set_author(wnd_->Find<TextEdit>(AUTHOR_ID)->get_text());
  world->set_description(wnd_->Find<TextEdit>(DESCRIPTION_ID)->get_text());

  WorldWriter writer(world);
  auto status = writer.write(world->get_name());
  if (!status.ok()) {
    // TODO: show error to user?
    LOG(ERR) << "error saving map: " << status << std::endl;
    return true;
  }

  wnd_->set_visible(false);
  return true;
}

bool SaveMapWindow::cancel_clicked(Widget &w) {
  wnd_->set_visible(false);
  return true;
}

}
