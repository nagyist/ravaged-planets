#pragma once

#include <mutex>
#include <tuple>

#include <game/world/terrain.h>

namespace fw {
class bitmap;
class texture;
}

namespace ed {

// this subclass of game::terrain allows us to edit the actual heightfield data, textures, and so on.
class editor_terrain: public game::terrain {
private:
  std::mutex _patches_to_bake_mutex;
  std::vector<std::tuple<int, int>> _patches_to_bake;

  // we keep a separate vector of the splatt bitmaps for easy editing
  std::vector<fw::bitmap> _splatt_bitmaps;

  // We also keep a separate vector of the layer bitmaps
  std::vector<std::shared_ptr<fw::bitmap>> _layer_bitmaps;

public:
  editor_terrain();
  virtual ~editor_terrain();

  // set a new height for the given vertex
  void set_vertex_height(int x, int y, float height);

  // gets and sets the texture of the given layer.
  int get_num_layers() const;
  std::shared_ptr<fw::bitmap> get_layer(int number);
  void set_layer(int number, std::shared_ptr<fw::bitmap> bitmap);

  // creates all of the splatt textures and sets them up initially
  void initialize_splatt();

  // sets the splat texture for the given patch to the given bitmap
  virtual void set_splatt(int patch_x, int patch_z, fw::bitmap const &bmp);
  fw::bitmap &get_splatt(int patch_x, int patch_z);

  float *get_height_data() const {
    return _heights;
  }

  // builds the collision data for the whole map. we assume the vertices buffer
  // is big enough to hold one data point per vertex in the map. each data point
  // holds a single boolean flag - true means "passable", false means "impassable"
  void build_collision_data(std::vector<bool> &vertices);

  virtual void render(fw::sg::scenegraph &scenegraph);
};

}
