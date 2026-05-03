#include <game/editor/windows/open_file.h>

#include <filesystem>
#include <functional>

#include <absl/strings/match.h>

#include <framework/bitmap.h>
#include <framework/gui/builder.h>
#include <framework/gui/button.h>
#include <framework/gui/checkbox.h>
#include <framework/gui/drawable.h>
#include <framework/gui/gui.h>
#include <framework/gui/label.h>
#include <framework/gui/listbox.h>
#include <framework/gui/textedit.h>
#include <framework/gui/widget.h>
#include <framework/gui/window.h>
#include <framework/logging.h>
#include <framework/paths.h>
#include <framework/texture.h>

namespace fs = std::filesystem;
using namespace fw::gui;
using namespace std::placeholders;

namespace ed {
namespace {

std::string FormatFileSize(uint32_t size) {
  if (size < 1024)
    return std::to_string(size);
  if (size < (1024 * 1024))
    return std::to_string(size / 1024) + " KB";

  return std::to_string(size / (1024 * 1024)) + " MB";
}

}

enum IDS {
  FILE_LIST_ID = 21897,
  FILENAME_ID,
  OK_ID,
  IMAGE_PREVIEW_ID
};

std::unique_ptr<OpenFileWindow> open_file;

OpenFileWindow::OpenFileWindow() : wnd_(nullptr), show_hidden_(false) {
}

OpenFileWindow::~OpenFileWindow() {
}

void OpenFileWindow::Initialize() {
  wnd_ = Builder<Window>()
      << Widget::width(Widget::Fixed(500.f))
      << Widget::height(Widget::Fixed(300.f))
      << Widget::background("frame")
      << Widget::visible(false)
      << Window::initial_position(WindowInitialPosition::Center())
      << (Builder<TextEdit>()
          << Widget::width(Widget::MatchParent())
          << Widget::height(Widget::Fixed(30.f))
          << Widget::margin(10.f, 10.f, 0.f, 10.f)
          << Widget::id(FILENAME_ID))
      << (Builder<LinearLayout>()
          << Widget::width(Widget::MatchParent())
          << Widget::height(Widget::MatchParent())
          << Widget::margin(50.f, 10.f, 50.f, 10.f)
          << LinearLayout::orientation(LinearLayout::Orientation::kHorizontal)
          << (Builder<Listbox>()
              << LinearLayout::weight(1.f)
              << Widget::height(Widget::MatchParent())
              << Widget::margin(0.f, 5.f, 0.f, 5.f)
              << Widget::id(FILE_LIST_ID)
              << Listbox::item_selected(std::bind(&OpenFileWindow::OnItemSelected, this, _1))
              << Listbox::item_activated(std::bind(&OpenFileWindow::OnItemActivated, this, _1)))
          << (Builder<Label>()
              << LinearLayout::weight(1.f)
              << Widget::height(Widget::MatchParent())
              << Widget::margin(0.f, 5.f, 0.f, 5.f)
              << Widget::id(IMAGE_PREVIEW_ID))
      )
      << (Builder<Button>()
          << Widget::width(Widget::Fixed(100.f))
          << Widget::height(Widget::Fixed(30.f))
          << Widget::gravity(LayoutParams::Gravity::kRight | LayoutParams::Gravity::kBottom)
          << Widget::margin(0.f, 10.f, 10.f, 0.f)
          << Button::text("OK")
          << Widget::id(OK_ID)
          << Button::click(std::bind(&OpenFileWindow::OnOkClicked, this, _1)))
      << (Builder<Button>()
          << Widget::width(Widget::Fixed(100.f))
          << Widget::height(Widget::Fixed(30.f))
          << Widget::gravity(LayoutParams::Gravity::kRight | LayoutParams::Gravity::kBottom)
          << Widget::margin(0.f, 120.f, 10.f, 0.f)
          << Button::text("Cancel")
          << Button::click(std::bind(&OpenFileWindow::OnCancelClicked, this, _1)))
      << (Builder<Checkbox>()
          << Widget::width(Widget::WrapContent())
          << Widget::height(Widget::Fixed(30.f))
          << Widget::gravity(LayoutParams::Gravity::kLeft | LayoutParams::Gravity::kBottom)
          << Widget::margin(0.f, 0.f, 10.f, 10.f)
          << Checkbox::text("Show hidden files")
          << Widget::click(std::bind(&OpenFileWindow::OnShowHiddenClicked, this, _1)));
  fw::Get<Gui>().AttachWindow(wnd_);
  curr_directory_ = fw::user_base_path();
}

void OpenFileWindow::Show(FileSelectedHandler fn) {
  file_selected_handler_ = fn;
  wnd_->set_visible(true);
  Refresh();
}

void OpenFileWindow::Hide() {
  wnd_->set_visible(false);
}

void OpenFileWindow::AddRow(Listbox &lbx, std::string_view name) {
  fs::path full_path = curr_directory_ / name;

  std::string file_size = "";
  if (fs::is_regular_file(full_path)) {
    file_size = FormatFileSize(fs::file_size(full_path));
  }

  std::string icon_name =
      fs::is_directory(full_path) ? "editor_icon_directory" : "editor_icon_file";

  lbx.AddItem(Builder<LinearLayout>()
          << Widget::width(Widget::MatchParent())
          << Widget::height(Widget::WrapContent())
          << LinearLayout::orientation(LinearLayout::Orientation::kHorizontal)
          << (Builder<Label>()
              << Widget::width(Widget::WrapContent())
              << Widget::height(Widget::WrapContent())
              << Widget::padding(2.f, 2.f, 2.f, 2.f)
              << Label::background(icon_name, true))
          << (Builder<Label>()
              << LinearLayout::weight(1.f)
              << Widget::height(Widget::WrapContent())
              << Widget::padding(2.f, 4.f, 2.f, 4.f)
              << Label::text(name))
          << (Builder<Label>()
              << Widget::width(Widget::WrapContent())
              << Widget::height(Widget::WrapContent())
              << Widget::padding(2.f, 8.f, 2.f, 4.f)
              << Label::text(file_size))
      );
  items_.push_back(std::string(name));
}

void OpenFileWindow::Refresh() {
  if (!fw::Graphics::is_render_thread()) {
    // Make sure refresh() runs on the render thread.
    fw::Get<fw::Graphics>().run_on_render_thread([this](){ Refresh(); });
    return;
  }
  auto lbx = wnd_->Find<Listbox>(FILE_LIST_ID);
  lbx->Clear();
  items_.clear();

  curr_directory_ = fs::canonical(curr_directory_);
  if (curr_directory_ != curr_directory_.root_name()) {
    // if it's not the root folder, add a ".." entry
    AddRow(*lbx, "..");
  }

  // Add them all to a directory so that we can sort them.
  std::vector<fs::path> paths;
  for (fs::directory_iterator it(curr_directory_); it != fs::directory_iterator(); ++it) {
    if (!show_hidden_ && fw::is_hidden(it->path())) {
      continue;
    }
    paths.push_back(it->path());
  }

  // Sort them by name, but with directories first
  std::sort(paths.begin(), paths.end(), [](fs::path const &lhs, fs::path const &rhs) {
    bool lhs_is_dir = fs::is_directory(lhs);
    bool rhs_is_dir = fs::is_directory(rhs);
    if (lhs_is_dir && !rhs_is_dir) {
      return true;
    } else if (rhs_is_dir && !lhs_is_dir) {
      return false;
    } else {
      std::string lhs_file = absl::AsciiStrToLower(lhs.filename().string());
      std::string rhs_file = absl::AsciiStrToLower(rhs.filename().string());
      return lhs_file < rhs_file;
    }
  });

  // Now add them to the listbox
  for(fs::path path : paths) {
    AddRow(*lbx, path.filename().string());
  }

  // we also want to set the "Path" to be the full path that we're currently displaying
  wnd_->Find<TextEdit>(FILENAME_ID)->set_text(curr_directory_.string());
}

fs::path OpenFileWindow::get_selected_file() const {
  return fs::path(wnd_->Find<TextEdit>(FILENAME_ID)->get_text());
}

void OpenFileWindow::NavigateToDirectory(fs::path const &new_directory) {
  curr_directory_ = new_directory;
  Refresh();
}

void OpenFileWindow::OnItemSelected(int index) {
  fs::path item_path = curr_directory_ / items_[index];
  wnd_->Find<TextEdit>(FILENAME_ID)->set_text(item_path.string());

  if (fs::is_regular_file(item_path)) {
    std::string ext = item_path.extension().string();
    bool is_image = false;
    if (ext == ".jpg" || ext == ".png") {
      // we only support JPG and PNG image formats
      auto bmp_or_status = fw::LoadBitmap(item_path);
      if (!bmp_or_status.ok()) {
        LOG(ERR) << "error loading bitmap '" << item_path << "': " << bmp_or_status.status();
      } else {
        auto bmp = *bmp_or_status;
        is_image = true; // if we loaded it, then we know it's an image

        auto preview = wnd_->Find<Label>(IMAGE_PREVIEW_ID);
        float width = preview->get_width();
        float height = preview->get_height();
        float bmp_width = bmp.get_width();
        float bmp_height = bmp.get_height();
        if (bmp_width > width || bmp_height > height) {
          float bmp_ratio = bmp_width / bmp_height;
          if (bmp_width > width) {
            bmp_width = width;
            bmp_height = width / bmp_ratio;
          }
          if (bmp_height > height) {
            bmp_height = height;
            bmp_width = height * bmp_ratio;
          }

          bmp.Resize(bmp_width, bmp_height);
        }

        std::shared_ptr<fw::Texture> texture = std::shared_ptr<fw::Texture>(new fw::Texture());
        texture->create(bmp);
        std::shared_ptr<Drawable> drawable =
            fw::Get<Gui>().get_drawable_manager().build_drawable(
                texture, 0, 0, bmp_width, bmp_height);
        preview->set_background(drawable, true);
      }
    }

    if (!is_image) {
      auto preview = wnd_->Find<Label>(IMAGE_PREVIEW_ID);
      preview->set_background(std::shared_ptr<Drawable>());
    }
  }
}

void OpenFileWindow::OnItemActivated(int index) {
  OnOkClicked(*wnd_->Find<Button>(OK_ID));
}

bool OpenFileWindow::OnShowHiddenClicked(Widget &w) {
  show_hidden_ = dynamic_cast<Checkbox &>(w).is_checked();
  Refresh();
  return true;
}

bool OpenFileWindow::OnOkClicked(Widget &w) {
  fs::path item_path = fs::path(wnd_->Find<TextEdit>(FILENAME_ID)->get_text());
  if (fs::is_directory(item_path)) {
    curr_directory_ = item_path;
    Refresh();
    return true;
  }

  if (fs::is_regular_file(item_path)) {
    if (file_selected_handler_) {
      file_selected_handler_(*this);
    }
    Hide();
  }
  return true;
}

bool OpenFileWindow::OnCancelClicked(Widget &w) {
  Hide();
  return true;
}

}
