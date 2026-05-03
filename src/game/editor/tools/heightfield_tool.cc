
#include <framework/framework.h>
#include <framework/graphics.h>
#include <framework/bitmap.h>
#include <framework/color.h>
#include <framework/input.h>
#include <framework/timer.h>
#include <framework/logging.h>
#include <framework/scenegraph.h>
#include <framework/gui/window.h>
#include <framework/gui/gui.h>
#include <framework/gui/builder.h>
#include <framework/gui/button.h>
#include <framework/gui/slider.h>
#include <framework/gui/label.h>
#include <framework/misc.h>

#include <game/editor/editor_terrain.h>
#include <game/editor/editor_world.h>
#include <game/editor/windows/open_file.h>
#include <game/editor/windows/message_box.h>
#include <game/editor/tools/heightfield_tool.h>
#include <framework/gui/linear_layout.h>

using namespace fw::gui;
using namespace std::placeholders;

//-----------------------------------------------------------------------------
/** This is the base "brush" class which handles the actual modification of the terrain. */
class HeightfieldBrush {
protected:
  std::shared_ptr<ed::EditorTerrain> terrain_;
  ed::HeightfieldTool *tool_;

public:
  HeightfieldBrush();
  virtual ~HeightfieldBrush();

  void Initialize(ed::HeightfieldTool *tool, std::shared_ptr<ed::EditorTerrain> terrain);

  virtual void update(fw::Vector const &) {
  }
};

HeightfieldBrush::HeightfieldBrush() :
    tool_(nullptr), terrain_(nullptr) {
}

HeightfieldBrush::~HeightfieldBrush() {
}

void HeightfieldBrush::Initialize(
    ed::HeightfieldTool *tool,
    std::shared_ptr<ed::EditorTerrain> terrain) {
  tool_ = tool;
  terrain_ = terrain;
}

//-----------------------------------------------------------------------------
class RaiseLowerBrush: public HeightfieldBrush {
public:
  RaiseLowerBrush();
  virtual ~RaiseLowerBrush();

  void OnRaiseKey(std::string keyname, bool is_down);
  void OnLowerKey(std::string keyname, bool is_down);
  virtual void update(fw::Vector const &cursor_loc) override;

private:
  bool is_raising_ = false;
  bool is_lowering_ = false;

  int raise_key_bind_;
  int lower_key_bind_;
};

RaiseLowerBrush::RaiseLowerBrush() {
  fw::Input *inp = fw::Framework::get_instance()->get_input();
  raise_key_bind_ = inp->bind_key(
      "Left-Mouse",
      fw::InputBinding(std::bind(&RaiseLowerBrush::OnRaiseKey, this, _1, _2)));
  lower_key_bind_ = inp->bind_key(
      "Right-Mouse",
      fw::InputBinding(std::bind(&RaiseLowerBrush::OnLowerKey, this, _1, _2)));
}

RaiseLowerBrush::~RaiseLowerBrush() {
  fw::Input *inp = fw::Framework::get_instance()->get_input();
  inp->unbind_key(raise_key_bind_);
  inp->unbind_key(lower_key_bind_);
}

void RaiseLowerBrush::OnRaiseKey(std::string keyname, bool is_down) {
  is_raising_ = is_down;
}

void RaiseLowerBrush::OnLowerKey(std::string keyname, bool is_down) {
  is_lowering_ = is_down;
}


void RaiseLowerBrush::update(fw::Vector const &cursor_loc) {
  float dy = 0.0f;
  if (is_raising_) {
    dy += 5.0f * fw::Framework::get_instance()->get_timer()->get_update_time();
  }
  if (is_lowering_) {
    dy -= 5.0f * fw::Framework::get_instance()->get_timer()->get_update_time();
  }

  if (dy != 0.0f) {
    int cx = (int) cursor_loc[0];
    int cz = (int) cursor_loc[2];
    int sx = cx - tool_->get_radius();
    int sz = cz - tool_->get_radius();
    int ex = cx + tool_->get_radius();
    int ez = cz + tool_->get_radius();

    float amount = 4.0f * tool_->get_speed();

    for (int z = sz; z < ez; z++) {
      for (int x = sx; x < ex; x++) {
        float height = terrain_->get_vertex_height(x, z);
        float distance =
            sqrt((float) ((x - cx) * (x - cx) + (z - cz) * (z - cz))) / tool_->get_radius();
        if (distance < 1.0f)
          terrain_->set_vertex_height(x, z, height + (dy * (1.0f - distance)) * amount);
      }
    }
  }
}

//-----------------------------------------------------------------------------
class LevelBrush: public HeightfieldBrush {
private:
  bool start_;
  bool levelling_;
  float level_height_;
  int key_bind_;

public:
  LevelBrush();
  virtual ~LevelBrush();

  void OnLevelKey(std::string keyname, bool is_down);
  virtual void update(fw::Vector const &cursor_loc) override;
};

LevelBrush::LevelBrush() :
    start_(false), levelling_(false), level_height_(0.0f) {
  fw::Input *inp = fw::Framework::get_instance()->get_input();
  key_bind_ = inp->bind_key(
      "Left-Mouse",
      fw::InputBinding(std::bind(&LevelBrush::OnLevelKey, this, _1, _2)));
}

LevelBrush::~LevelBrush() {
  fw::Input *inp = fw::Framework::get_instance()->get_input();
  inp->unbind_key(key_bind_);
}

void LevelBrush::OnLevelKey(std::string keyname, bool is_down) {
  if (!levelling_ && is_down) {
    levelling_ = true;
    start_ = true;
  } else {
    levelling_ = is_down;
  }
}

void LevelBrush::update(fw::Vector const &cursor_loc) {
  if (start_) {
    // this means you've just clicked the left-mouse button, we have to start
    // levelling, which means we need to save the current terrain height
    level_height_ = terrain_->get_height(cursor_loc[0], cursor_loc[2]);
    levelling_ = true;
    start_ = false;
  }

  if (levelling_) {
    int cx = (int) cursor_loc[0];
    int cz = (int) cursor_loc[2];
    int sx = cx - tool_->get_radius();
    int sz = cz - tool_->get_radius();
    int ex = cx + tool_->get_radius();
    int ez = cz + tool_->get_radius();

    float amount = 4.0f * tool_->get_speed();

    for (int z = sz; z < ez; z++) {
      for (int x = sx; x < ex; x++) {
        float height = terrain_->get_vertex_height(x, z);
        float diff = (level_height_ - height) * fw::Framework::get_instance()->get_timer()->get_update_time();
        float distance = sqrt((float) ((x - cx) * (x - cx) + (z - cz) * (z - cz))) / tool_->get_radius();
        if (distance < 1.0f) {
          terrain_->set_vertex_height(x, z, height + (diff * (1.0f - distance)) * amount);
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------
enum IDS {
  RAISE_LOWER_BRUSH_ID = 329723,
  LEVEL_BRUSH_ID
};

class HeightfieldToolWindow {
private:
  std::shared_ptr<Window> wnd_;
  ed::HeightfieldTool *tool_;

  void OnRadiusUpdated(int value);
  void OnSpeedUpdated(int value);
  bool on_tool_clicked(fw::gui::Widget &w);
  bool on_import_clicked(fw::gui::Widget &w);
  void on_import_file_selected(ed::OpenFileWindow &ofw);

public:
  HeightfieldToolWindow(ed::HeightfieldTool *Tool);
  ~HeightfieldToolWindow();

  void show();
  void hide();
};

HeightfieldToolWindow::HeightfieldToolWindow(ed::HeightfieldTool *tool) :
    tool_(tool) {
  wnd_ = Builder<Window>()
      << Widget::width(Widget::Fixed(100.f))
      << Widget::height(Widget::WrapContent())
      << Window::initial_position(WindowInitialPosition::Absolute(20.f, 40.f))
      << Widget::background("frame")
      << (Builder<LinearLayout>()
          << Widget::width(Widget::MatchParent())
          << Widget::height(Widget::WrapContent())
          << LinearLayout::orientation(LinearLayout::Orientation::kVertical)
          << (Builder<LinearLayout>()
              << Widget::width(Widget::MatchParent())
              << Widget::height(Widget::WrapContent())
              << LinearLayout::orientation(LinearLayout::Orientation::kHorizontal)
              << (Builder<Button>()
                  << Widget::height(Widget::Fixed(30.f))
                  << LinearLayout::weight(1.0f)
                  << Widget::margin(10.f, 5.f, 10.0f, 10.0f)
                  << Widget::id(RAISE_LOWER_BRUSH_ID)
                  << Button::icon("editor_hightfield_raiselower")
                  << Button::click(std::bind(&HeightfieldToolWindow::on_tool_clicked, this, _1)))
              << (Builder<Button>()
                  << Widget::height(Widget::Fixed(30.f))
                  << LinearLayout::weight(1.0f)
                  << Widget::margin(10.f, 10.f, 10.0f, 5.0f)
                  << Widget::id(LEVEL_BRUSH_ID)
                  << Button::icon("editor_hightfield_level")
                  << Button::click(std::bind(&HeightfieldToolWindow::on_tool_clicked, this, _1)))
              )
        << (Builder<Label>()
            << Widget::width(Widget::MatchParent())
            << Widget::height(Widget::WrapContent())
            << Widget::margin(5.f, 10.f, 5.0f, 10.0f)
            << Label::text("Size:"))
        << (Builder<Slider>()
            << Widget::width(Widget::MatchParent())
            << Widget::height(Widget::WrapContent())
            << Widget::margin(5.f, 10.f, 5.0f, 10.0f)
            << Slider::limits(20, 100)
            << Slider::value(40)
            << Slider::on_update(std::bind(&HeightfieldToolWindow::OnRadiusUpdated, this, _1)))
        << (Builder<Label>()
            << Widget::width(Widget::MatchParent())
            << Widget::height(Widget::WrapContent())
            << Widget::margin(5.f, 10.f, 5.0f, 10.0f)
            << Label::text("Speed:"))
        << (Builder<Slider>()
            << Widget::width(Widget::MatchParent())
            << Widget::height(Widget::WrapContent())
            << Widget::margin(5.f, 10.f, 5.0f, 10.0f)
            << Slider::limits(20, 100)
            << Slider::value(25)
            << Slider::on_update(std::bind(&HeightfieldToolWindow::OnSpeedUpdated, this, _1)))
        << (Builder<Button>()
            << Widget::width(Widget::MatchParent())
            << Widget::height(Widget::Fixed(30.f))
            << Widget::margin(5.f, 10.f, 10.0f, 10.0f)
            << Button::text("Import")
            << Button::click(std::bind(&HeightfieldToolWindow::on_import_clicked, this, _1)))
        );
  fw::Get<Gui>().AttachWindow(wnd_);
}

HeightfieldToolWindow::~HeightfieldToolWindow() {
  fw::Get<Gui>().DetachWindow(wnd_);
}

void HeightfieldToolWindow::show() {
  wnd_->set_visible(true);
}

void HeightfieldToolWindow::hide() {
  wnd_->set_visible(false);
}

void HeightfieldToolWindow::OnRadiusUpdated(int value) {
  int radius = value / 10;
  tool_->set_radius(radius);
}

void HeightfieldToolWindow::OnSpeedUpdated(int value) {
  float speed = value / 100.0f;
  tool_->set_speed(speed);
}

bool HeightfieldToolWindow::on_import_clicked(fw::gui::Widget &w) {
  ed::open_file->Show(std::bind(&HeightfieldToolWindow::on_import_file_selected, this, _1));
  return true;
}

void HeightfieldToolWindow::on_import_file_selected(ed::OpenFileWindow &ofw) {
  auto bmp = fw::LoadBitmap(ofw.get_selected_file());
  if (!bmp.ok()) {
    // error, probably not an image?
    return;
  }
  tool_->import_heightfield(*bmp);
}

bool HeightfieldToolWindow::on_tool_clicked(fw::gui::Widget &w) {
  auto raise_lower = wnd_->Find<Button>(RAISE_LOWER_BRUSH_ID);
  auto level = wnd_->Find<Button>(LEVEL_BRUSH_ID);

  raise_lower->set_pressed(false);
  level->set_pressed(false);
  dynamic_cast<Button &>(w).set_pressed(true);

  if (w.get_id() == RAISE_LOWER_BRUSH_ID) {
    tool_->set_brush(std::make_unique<RaiseLowerBrush>());
  } else if (w.get_id() == LEVEL_BRUSH_ID) {
    tool_->set_brush(std::make_unique<LevelBrush>());
  }

  return true;
}

//-----------------------------------------------------------------------------
namespace ed {
REGISTER_TOOL("heightfield", HeightfieldTool);

float HeightfieldTool::max_radius = 6;

HeightfieldTool::HeightfieldTool(EditorWorld *wrld) :
    Tool(wrld), brush_(nullptr) {
  wnd_ = std::make_unique<HeightfieldToolWindow>(this);
}

HeightfieldTool::~HeightfieldTool() {}

void HeightfieldTool::activate() {
  Tool::activate();

  set_brush(std::make_unique<RaiseLowerBrush>());

  indicator_ = std::make_shared<IndicatorNode>(terrain_);
  indicator_->set_radius(radius_);
  fw::Framework::get_instance()->get_scenegraph_manager()->enqueue(
    [indicator = indicator_](fw::sg::Scenegraph& sg) {
      sg.add_node(indicator);
    });

  wnd_->show();
}

void HeightfieldTool::deactivate() {
  Tool::deactivate();

  fw::Framework::get_instance()->get_scenegraph_manager()->enqueue(
    [indicator = indicator_](fw::sg::Scenegraph& sg) {
      sg.remove_node(indicator);
    });
  indicator_.reset();

  wnd_->hide();
}

void HeightfieldTool::set_brush(std::unique_ptr<HeightfieldBrush> brush) {
  brush_ = std::move(brush);
  brush_->Initialize(this, terrain_);
}

void HeightfieldTool::set_radius(int radius) {
  if (radius_ == radius) return;

  radius_ = radius;
  indicator_->set_radius(radius);
}

void HeightfieldTool::update() {
  Tool::update();

  fw::Vector cursor_loc = terrain_->get_cursor_location();
  if (brush_ != nullptr) {
    brush_->update(cursor_loc);
  }
  indicator_->set_center(cursor_loc);
}

void HeightfieldTool::import_heightfield(fw::Bitmap &bm) {
  int terrain_width = terrain_->get_width();
  int terrain_length = terrain_->get_length();

  // resize the bitmap so it's the same size as our terrain
  bm.Resize(terrain_width, terrain_length);

  // get the pixel data from the bitmap
  std::vector<uint32_t> const &pixels = bm.GetPixels();

  // now, for each pixel, we set the terrain height!
  for (int x = 0; x < terrain_width; x++) {
    for (int z = 0; z < terrain_length; z++) {
      fw::Color col = fw::Color::from_argb(pixels[x + (z * terrain_width)]);
      float val = col.grayscale();

      terrain_->set_vertex_height(x, z, val * 30.0f);
    }
  }
}
/*
void HeightfieldTool::render(fw::sg::Scenegraph &scenegraph) {
  draw_circle(scenegraph, terrain_, terrain_->get_cursor_location(), (float) radius_);
}*/

}
