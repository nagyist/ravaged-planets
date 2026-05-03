#include <game/screens/hud/minimap_window.h>

#include <functional>
#include <memory>

#include <framework/bitmap.h>
#include <framework/camera.h>
#include <framework/color.h>
#include <framework/framework.h>
#include <framework/graphics.h>
#include <framework/gui/builder.h>
#include <framework/gui/drawable.h>
#include <framework/gui/gui.h>
#include <framework/gui/label.h>
#include <framework/gui/window.h>
#include <framework/logging.h>
#include <framework/math.h>
#include <framework/shader.h>
#include <framework/texture.h>
#include <framework/timer.h>

#include <game/entities/entity.h>
#include <game/entities/entity_manager.h>
#include <game/entities/minimap_visible_component.h>
#include <game/entities/ownable_component.h>
#include <game/entities/position_component.h>
#include <game/world/world.h>
#include <game/world/terrain.h>
#include <game/simulation/player.h>

using namespace fw::gui;
using namespace std::placeholders;

namespace game {

/** This is the special drawble implementation we use to draw the rotated bitmap for the minimap. */
class MinimapDrawable : public fw::gui::BitmapDrawable {
protected:
  fw::Matrix transform_;
  fw::Matrix get_pos_transform(float x, float y, float width, float height);
  fw::Matrix get_uv_transform();

public:
  MinimapDrawable(std::shared_ptr<fw::Texture> texture);
  virtual ~MinimapDrawable();

  void update(fw::Matrix transform);
};

MinimapDrawable::MinimapDrawable(std::shared_ptr<fw::Texture> texture)
    : fw::gui::BitmapDrawable(texture) {
  top_ = left_ = 0;
  width_ = texture->get_width() * 3;
  height_ = texture->get_height() * 3;
}

MinimapDrawable::~MinimapDrawable() {
}

void MinimapDrawable::update(fw::Matrix transform) {
  transform_ = transform;
}

fw::Matrix MinimapDrawable::get_uv_transform() {
  float x = static_cast<float>(left_) / static_cast<float>(texture_->get_width());
  float y = static_cast<float>(top_) / static_cast<float>(texture_->get_height());
  float width = static_cast<float>(width_) / static_cast<float>(texture_->get_width());
  float height = static_cast<float>(height_) / static_cast<float>(texture_->get_height());
  return fw::scale(fw::Vector(width, height, 0.0f)) * fw::translation(fw::Vector(x, y, 0));
}

fw::Matrix MinimapDrawable::get_pos_transform(float x, float y, float width, float height) {
  fw::Graphics &g = fw::Get<fw::Graphics>();
  fw::Matrix transform =
    fw::projection_orthographic(
      0.0f, static_cast<float>(g.get_width()), static_cast<float>(g.get_height()), 0.0f, 1.0f, -1.0f);
  transform = fw::translation(fw::Vector(x - width, y - height, 0)) * transform;
  transform = fw::scale(fw::Vector(width * 3, height * 3, 0.0f)) * transform;
  return transform_ * transform;
}

//-----------------------------------------------------------------------------

MinimapWindow *hud_minimap = nullptr;

enum ids {
  MINIMAP_IMAGE_ID = 9733,
};

MinimapWindow::MinimapWindow() :
    last_entity_display_update_(0.0f), texture_(new fw::Texture()), wnd_(nullptr) {
}

MinimapWindow::~MinimapWindow() {
 // fw::Get<Gui>().detach_widget(wnd_);
}

void MinimapWindow::initialize() {/*
  wnd_ = Builder<Window>(sum(pct(100), px(-210)), px(10), px(200), px(200))
      << Widget::background("frame")
          << Widget::visible(false)
      << (Builder<Label>(px(8), px(8), sum(pct(100), px(-16)), sum(pct(100), px(-16)))
          << Widget::id(MINIMAP_IMAGE_ID))
      << (Builder<Label>(sum(pct(50), px(-10)), sum(pct(50), px(-22)), px(19), px(31))
          << Label::background("hud_minimap_crosshair"));
  fw::Get<Gui>().attach_widget(wnd_);*/
}

void MinimapWindow::show() {
  auto terrain = game::World::get_instance()->get_terrain();
  texture_->create(terrain->get_width(), terrain->get_length());
  drawable_ = std::shared_ptr<MinimapDrawable>(new MinimapDrawable(texture_));
  wnd_->Find<Label>(MINIMAP_IMAGE_ID)->set_background(drawable_);

  // bind to the camera's sig_updated signal, to be notified when you move the camera around
  camera_updated_connection_ = fw::Framework::get_instance()->get_camera()->sig_updated
      .Connect(std::bind(&MinimapWindow::on_camera_updated, this));

  wnd_->set_visible(true);
  update_drawable();
}

void MinimapWindow::hide() {
  wnd_->set_visible(false);

  // disconnect our signals - we don't want to keep showing the minimap!
  fw::Framework::get_instance()->get_camera()->sig_updated.Disconnect(camera_updated_connection_);
}

void MinimapWindow::update() {
  float gt = fw::Framework::get_instance()->get_timer()->get_total_time();
  if ((gt - 1.0f) > last_entity_display_update_) {
    last_entity_display_update_ = gt;
    update_entity_display();
  }
}

void MinimapWindow::on_camera_updated() {
  update_drawable();
}

void MinimapWindow::update_drawable() {
  fw::Camera *camera = fw::Framework::get_instance()->get_camera();
  auto terrain = game::World::get_instance()->get_terrain();

  // Offset so that it shows up in the correct position relative to where the camera is
  fw::Vector cam_pos(
      1.0f - (camera->get_location()[0] / terrain->get_width()),
      1.0f - (camera->get_location()[2] / terrain->get_length()),
      0);
  // Make it go from -0.5 -> 0.5
  cam_pos = fw::Vector(cam_pos[0] - 0.5f, cam_pos[1] - .5f, 0);
  // Make it 1/3 the size, because we expand the drawable by 3 (size you're always in the center of a 3x3 grid)
  cam_pos = fw::Vector(cam_pos[0] / 3.0f, cam_pos[1] / 3.0f, 0);
  fw::Matrix transform = fw::translation(cam_pos);

  // rotate so that we're facing in the same direction as the camera
  fw::Vector cam_dir(camera->get_direction());
  cam_dir = fw::Vector(cam_dir[0], -cam_dir[2], 0).normalized();
  transform *= fw::translation(-0.5f, -0.5f, 0);
  transform *= fw::rotate(fw::Vector(0, 1, 0), cam_dir);
  transform *= fw::translation(0.5f, 0.5f, 0);

  drawable_->update(transform);
}

void MinimapWindow::update_entity_display() {
  int width = game::World::get_instance()->get_terrain()->get_width();
  int height = game::World::get_instance()->get_terrain()->get_length();

  fw::Graphics& graphics = fw::Get<fw::Graphics>();
  float screen_width = graphics.get_width();
  float screen_height = graphics.get_height();

  int wnd_width = wnd_->get_width();
  int wnd_height = wnd_->get_height();

  // we want to draw pixels that are big enough that you can actually see them on the map...
  int pixel_width = 1 + static_cast<int>(0.5f + width / wnd_width);
  int pixel_height = 1 + static_cast<int>(0.5f + height / wnd_height);

  // make a copy of the minimap background's pixels
  std::vector<uint32_t> pixels(game::World::get_instance()->get_minimap_background().GetPixels());

  // go through each minimap_visible Entity and draw it on our bitmap
  ent::EntityManager *ent_mgr = game::World::get_instance()->get_entity_manager();
  for (std::weak_ptr<ent::Entity> wp : ent_mgr->get_entities_by_component<ent::MinimapVisibleComponent>()) {
    std::shared_ptr<ent::Entity> ent = wp.lock();
    if (!ent) {
      continue;
    }

    ent::PositionComponent *position_comp = ent->get_component<ent::PositionComponent>();
    ent::OwnableComponent *ownable_comp = ent->get_component<ent::OwnableComponent>();

    // obviously we can't display the position if it doesn't HAVE a position!
    if (position_comp == nullptr)
      continue;

    fw::Color col(1, 1, 1); // default is white
    if (ownable_comp != nullptr) {
      // if it's ownable, draw it with the owner's color
      col = ownable_comp->get_owner()->get_color();
    }

    fw::Vector pos = position_comp->get_position();
    for (int y_offset = -pixel_height; y_offset <= pixel_height; y_offset++) {
      for (int x_offset = -pixel_width; x_offset <= pixel_width; x_offset++) {
        int x = static_cast<int>(pos[0] + x_offset);
        int y = height - (static_cast<int>(pos[2] + y_offset));

        // if it outside the valid range, skip this pixel
        if (x < 0 || x >= width || y < 0 || y >= height)
          continue;

        pixels[x + (y * width)] = col.to_argb();
      }
    }
  }

  fw::Bitmap bm(width, height, pixels.data());
  fw::Get<fw::Graphics>().run_on_render_thread([this, bm]() {
    texture_->create(bm);
  });
}

}
