
#include <functional>
#include <memory>
#include <boost/foreach.hpp>

#include <framework/bitmap.h>
#include <framework/camera.h>
#include <framework/colour.h>
#include <framework/framework.h>
#include <framework/graphics.h>
#include <framework/gui/builder.h>
#include <framework/gui/drawable.h>
#include <framework/gui/gui.h>
#include <framework/gui/label.h>
#include <framework/gui/window.h>
#include <framework/logging.h>
#include <framework/shader.h>
#include <framework/texture.h>
#include <framework/timer.h>
#include <framework/vector.h>

#include <game/entities/entity.h>
#include <game/entities/entity_manager.h>
#include <game/entities/minimap_visible_component.h>
#include <game/entities/ownable_component.h>
#include <game/entities/position_component.h>
#include <game/screens/hud/minimap_window.h>
#include <game/world/world.h>
#include <game/world/terrain.h>
#include <game/simulation/player.h>

using namespace fw::gui;
using namespace std::placeholders;

namespace game {

/** This is the special drawble implementation we use to draw the rotated bitmap for the minimap. */
class minimap_drawable : public fw::gui::bitmap_drawable {
protected:
  fw::matrix _transform;
  fw::matrix get_transform(fw::graphics *g, float x, float y, float width, float height);

public:
  minimap_drawable(std::shared_ptr<fw::texture> texture);
  virtual ~minimap_drawable();

  void update(fw::matrix transform);
};

minimap_drawable::minimap_drawable(std::shared_ptr<fw::texture> texture) :
    fw::gui::bitmap_drawable(texture) {
}

minimap_drawable::~minimap_drawable() {
}

void minimap_drawable::update(fw::matrix transform) {
  _transform = transform;
}

fw::matrix minimap_drawable::get_transform(fw::graphics *g, float x, float y, float width, float height) {
  fw::matrix transform;
  cml::matrix_orthographic_RH(transform, 0.0f,
      static_cast<float>(g->get_width()), static_cast<float>(g->get_height()), 0.0f, 1.0f, -1.0f, cml::z_clip_neg_one);
  transform = fw::scale(fw::vector(width, height, 0.0f)) * fw::translation(fw::vector(x, y, 0)) * transform;
  return _transform * transform;
}

//-----------------------------------------------------------------------------

minimap_window *hud_minimap = nullptr;

enum ids {
  MINIMAP_IMAGE_ID = 9733,
};

minimap_window::minimap_window() :
    _last_entity_display_update(0.0f), _texture(new fw::texture()), _wnd(nullptr) {
}

minimap_window::~minimap_window() {
  fw::framework::get_instance()->get_gui()->detach_widget(_wnd);
}

void minimap_window::initialize() {
  _wnd = builder<window>(sum(pct(100), px(-210)), px(10), px(200), px(200))
      << window::background("frame") << widget::visible(false)
      << (builder<label>(px(8), px(8), sum(pct(100), px(-16)), sum(pct(100), px(-16))) << widget::id(MINIMAP_IMAGE_ID));
  fw::framework::get_instance()->get_gui()->attach_widget(_wnd);
}

void minimap_window::show() {
  game::terrain *trn = game::world::get_instance()->get_terrain();
  _texture->create(trn->get_width(), trn->get_length());
  _drawable = std::shared_ptr<minimap_drawable>(new minimap_drawable(_texture));
  _wnd->find<label>(MINIMAP_IMAGE_ID)->set_background(_drawable);

  // bind to the camera's sig_updated signal, to be notified when you move the camera around
  _camera_updated_connection = fw::framework::get_instance()->get_camera()->sig_updated
      .connect(std::bind(&minimap_window::on_camera_updated, this));

  _wnd->set_visible(true);
  update_drawable();
}

void minimap_window::hide() {
  _wnd->set_visible(false);

  // disconnect our signals - we don't want to keep showing the minimap!
  _camera_updated_connection.disconnect();
}

void minimap_window::on_camera_updated() {
  update_drawable();

  float gt = fw::framework::get_instance()->get_timer()->get_total_time();
  if ((gt - 1.0f) > _last_entity_display_update) {
    _last_entity_display_update = gt;
    fw::framework::get_instance()->get_graphics()->run_on_render_thread([this]() {
      update_entity_display();
    });
  }
}

void minimap_window::update_drawable() {
  fw::camera *camera = fw::framework::get_instance()->get_camera();
  game::terrain *terrain = game::world::get_instance()->get_terrain();
  fw::graphics *graphics = fw::framework::get_instance()->get_graphics();

  // get the width/height of the screen
  float screen_width = graphics->get_width();
  float screen_height = graphics->get_height();

  // get the width/height of the HUD window as a percentage of the screen
  float scale_x = _wnd->get_width() / screen_width;
  float scale_y = _wnd->get_height() / screen_height;

  // offset so that it shows up in the correct position relative to where the camera is
  fw::vector cam_pos(camera->get_position()[0] / terrain->get_width(),
      camera->get_position()[2] / terrain->get_length(), 0);
  cam_pos = fw::vector((cam_pos[0] * 2.0f) - 1.0f, (cam_pos[1] * 2.0f) - 1.0f, 0);
  cam_pos = fw::vector(cam_pos[0] / 3.0f, -cam_pos[1] / 3.0f, 0);
  fw::matrix transform = fw::translation(cam_pos);

  transform *= fw::scale(fw::vector(3.0f, 3.0f, 0.0f));

  // rotate so that we're facing in the same direction as the camera
  fw::vector cam_dir(camera->get_direction());
  cam_dir = fw::vector(cam_dir[0], cam_dir[2], 0).normalize();
  transform *= fw::rotate(fw::vector(0, -1, 0), cam_dir);

  // scale it so that it's the correct aspect ratio for the window
  transform *= fw::scale(fw::vector(scale_x, -scale_y, 0.0f));
  _drawable->update(transform);
}

void minimap_window::update_entity_display() {
  int width = game::world::get_instance()->get_terrain()->get_width();
  int height = game::world::get_instance()->get_terrain()->get_length();

  fw::graphics *graphics = fw::framework::get_instance()->get_graphics();
  float screen_width = graphics->get_width();
  float screen_height = graphics->get_height();

  int wnd_width = _wnd->get_width();
  int wnd_height = _wnd->get_height();

  // we want to draw pixels that are big enough that you can actually see them on the map...
  int pixel_width = 1 + static_cast<int>(0.5f + width / wnd_width);
  int pixel_height = 1 + static_cast<int>(0.5f + height / wnd_height);

  // make a copy of the minimap background's pixels
  std::vector<uint32_t> pixels(game::world::get_instance()->get_minimap_background()->get_pixels());

  // go through each minimap_visible entity and draw it on our bitmap
  ent::entity_manager *ent_mgr = game::world::get_instance()->get_entity_manager();
  BOOST_FOREACH(std::weak_ptr<ent::entity> wp, ent_mgr->get_entities_by_component<ent::minimap_visible_component>()) {
    std::shared_ptr<ent::entity> ent = wp.lock();
    if (!ent) {
      continue;
    }

    ent::position_component *position_comp = ent->get_component<ent::position_component>();
    ent::ownable_component *ownable_comp = ent->get_component<ent::ownable_component>();

    // obviously we can't display the position if it doesn't HAVE a position!
    if (position_comp == nullptr)
      continue;

    fw::colour col(1, 1, 1); // default is white
    if (ownable_comp != nullptr) {
      // if it's ownable, draw it with the owner's colour
      col = ownable_comp->get_owner()->get_colour();
    }

    fw::vector pos = position_comp->get_position();
    for (int y_offset = -pixel_height; y_offset <= pixel_height; y_offset++) {
      for (int x_offset = -pixel_width; x_offset <= pixel_width; x_offset++) {
        int x = static_cast<int>(pos[0] + x_offset);
        int y = height - (static_cast<int>(pos[2] + y_offset));

        // if it outside the valid range, skip this pixel
        if (x < 0 || x >= width || y < 0 || y >= height)
          continue;

        pixels[x + (y * width)] = col.to_rgba();
      }
    }
  }

  fw::bitmap bm(width, height, pixels.data());
  _texture->create(bm);
}
/*
void minimap_window::on_before_present() {
  if (!_bg_fx_params)
    return;

  float gt = fw::framework::get_instance()->get_timer()->get_total_time();
  if ((gt - 1.0f) > _last_entity_display_update) {
    _last_entity_display_update = gt;
    update_entity_display();
  }

  // calculate the rectangle that we want to draw the map into
  fw::main_window *main_window = fw::framework::get_instance()->get_window();
  float screen_width = main_window->get_width();
  float screen_height = main_window->get_height();
  CEGUI::Vector2 pos = get_window()->getPosition().asAbsolute(CEGUI::Size(screen_width, screen_height));
  CEGUI::Vector2 size = get_window()->getSize().asAbsolute(CEGUI::Size(screen_width, screen_height));
  RECT rect = { pos.d_x + 4, pos.d_y + 4, pos.d_x + size.d_x - 4, pos.d_y + size.d_y - 4 };

  // use a scissor rectangle to limit the area in which we actually draw the map
  fw::graphics *g = fw::framework::get_instance()->get_graphics();
  g->get_device()->SetScissorRect(&rect);
  g->get_device()->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);

  // render the minimap background
  int num_passes = _fx->begin(_bg_fx_params.get());
  _fx->set_matrix("map_transform", _m);
  _fx->set_vector("map_centre", _map_centre);
  _fx->set_vector("map_size", _map_size);
  for (int i = 0; i < num_passes; i++) {
    _fx->begin_pass(i);
    _vb->render(2, D3DPT_TRIANGLELIST, _ib.get());
    _fx->end_pass();
  }
  _fx->end();

  // render the minimap again with the foreground texture this time
  num_passes = _fx->begin(_fg_fx_params.get());
  _fx->set_matrix("map_transform", _m);
  _fx->set_vector("map_centre", _map_centre);
  _fx->set_vector("map_size", _map_size);
  for (int i = 0; i < num_passes; i++) {
    _fx->begin_pass(i);
    _vb->render(2, D3DPT_TRIANGLELIST, _ib.get());
    _fx->end_pass();
  }
  _fx->end();

  // turn the scissor test off again
  g->get_device()->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
}
*/
}
