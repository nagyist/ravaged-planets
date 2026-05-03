#include <game/world/terrain.h>

#include <framework/graphics.h>
#include <framework/misc.h>
#include <framework/paths.h>
#include <framework/logging.h>
#include <framework/camera.h>
#include <framework/bitmap.h>
#include <framework/texture.h>
#include <framework/input.h>
#include <framework/math.h>
#include <framework/scenegraph.h>
#include <framework/shader.h>
#include <framework/status.h>

#include <game/world/terrain_helper.h>

namespace game {

Terrain::Terrain(int width, int height, float* height_data /*= nullptr*/) :
    width_(width), length_(width), heights_(nullptr), root_node_(std::make_shared<fw::sg::Node>()) {
  if (height_data != nullptr) {
    heights_ = height_data;
  } else {
    heights_ = new float[width_ * length_];
    for (int i = 0; i < width_ * length_; i++)
      heights_[i] = 0.0f;
  }
}

Terrain::~Terrain() {
  delete[] heights_;
}

fw::Status Terrain::initialize() {
  // generate indices
  std::vector<uint16_t> index_data;
  generate_terrain_indices(index_data, PATCH_SIZE);

  // TODO: this should come from the world_reader
  textures_ = std::make_shared<fw::TextureArray>(512, 512);
  const auto bitmap_names = {
    fw::resolve("terrain/grass-01.jpg"),
    fw::resolve("terrain/rock-01.jpg"),
    fw::resolve("terrain/rock-01.jpg"),
    fw::resolve("terrain/snow-01.jpg"),
    fw::resolve("terrain/rock-snow-01.jpg"),
    fw::resolve("terrain/sand-01.png"),
    fw::resolve("terrain/grass-02.jpg"),
    fw::resolve("terrain/road-01.png"),
  };
  int index = 0;
  for (const auto& bitmap_name : bitmap_names) {
     ASSIGN_OR_RETURN(auto bitmap, fw::LoadBitmap(bitmap_name));
     set_layer(index++, bitmap);
  }

  std::shared_ptr<fw::sg::Node> root_node = root_node_;
  fw::Framework::get_instance()->get_scenegraph_manager()->enqueue(
    [root_node, this, index_data](fw::sg::Scenegraph& scenegraph) {
      // bake the patches into the vertex buffers that'll be used for rendering
      for (int patch_z = 0; patch_z < get_patches_length(); patch_z++) {
        for (int patch_x = 0; patch_x < get_patches_width(); patch_x++) {
          bake_patch(patch_x, patch_z);
        }
      }

      ib_ = std::make_shared<fw::IndexBuffer>();
      ib_->set_data(index_data.size(), &index_data[0], 0);

      // TODO: we should return an error if this is a real error.
      shader_ = fw::Shader::CreateOrEmpty("terrain.shader");

      scenegraph.add_node(root_node);
    });
  return fw::OkStatus();
}

std::shared_ptr<fw::Texture> Terrain::get_patch_splatt(int patch_x, int patch_z) {
  ensure_patches();

  unsigned int index = get_patch_index(patch_x, patch_z);
  return patches_[index]->texture;
}

void Terrain::set_layer(int number, fw::Bitmap const &bitmap) {
  if (number < 0)
    return;

  // TODO: support setting layers and changing them and stuff.
  textures_->add(bitmap);
}

void Terrain::set_patch_splatt(int patch_x, int patch_z, std::shared_ptr<fw::Texture> texture) {
  ensure_patches();

  unsigned int index = get_patch_index(patch_x, patch_z);
  patches_[index]->texture = texture;
}

void Terrain::set_splatt(int patch_x, int patch_z, fw::Bitmap const &bmp) {
  auto splatt = create_splatt(bmp);

  set_patch_splatt(patch_x, patch_z, splatt);
}

std::shared_ptr<fw::Texture> Terrain::create_splatt(fw::Bitmap const& bmp) {
  auto splatt = std::make_shared<fw::Texture>();
  splatt->create(bmp, /*internal_format=*/GL_R8UI, /*format=*/GL_RED_INTEGER, /*component_type=*/GL_INT);
  splatt->set_filter(GL_NEAREST, GL_NEAREST);
  return splatt;
}

void Terrain::bake_patch(int patch_x, int patch_z) {
  unsigned int index = get_patch_index(patch_x, patch_z, &patch_x, &patch_z);
  ensure_patches();

  // if we haven't created the vertex buffer for this patch yet, do it now
  if (patches_[index]->vb == std::shared_ptr<fw::VertexBuffer>()) {
    patches_[index]->vb = fw::VertexBuffer::create<fw::vertex::xyz_n>();
  }

  fw::vertex::xyz_n *vert_data;
  int num_verts = generate_terrain_vertices(&vert_data, heights_, width_, length_, PATCH_SIZE, patch_x, patch_z);

  std::shared_ptr<TerrainPatch> patch(patches_[index]);
  patch->vb->set_data(num_verts, vert_data, 0);
  delete[] vert_data;

  patch->shader_params = shader_->CreateParameters();
  patch->shader_params->set_texture("textures", textures_);
}

void Terrain::ensure_patches() {
  unsigned int index = get_patch_index(get_patches_width() - 1,
      get_patches_length() - 1);
  while (patches_.size() <= index) {
    std::shared_ptr<TerrainPatch> patch(new TerrainPatch());
    patches_.push_back(patch);
  }
}

void Terrain::update() {
  fw::Camera *camera = fw::Framework::get_instance()->get_camera();

  // if the camera has moved off the edge of the map, wrap it back around
  fw::Vector old_loc = camera->get_location();
  fw::Vector new_loc(
      fw::constrain(old_loc[0], (float) width_),
      old_loc[1],
      fw::constrain(old_loc[2], (float) length_));
  if ((old_loc - new_loc).length() > 0.001f) {
    camera->set_location(new_loc);
  }

  // also, set the ground height so the camera follows the terrain
  float new_height = get_height(new_loc[0], new_loc[2]);
  float height_diff = camera->get_ground_height() - new_height;
  if (height_diff > 0.001f || height_diff < -0.001f) {
    camera->set_ground_height(new_height);
  }

  fw::Vector location = get_cursor_location(camera->get_position(), camera->get_direction());
  std::shared_ptr<fw::sg::Node> root_node = root_node_;
  fw::Framework::get_instance()->get_scenegraph_manager()->enqueue(
    [location, root_node, &terrain = std::as_const(*this)](fw::sg::Scenegraph& scenegraph) {
      int centre_patch_x = (int)(location[0] / PATCH_SIZE);
      int centre_patch_z = (int)(location[2] / PATCH_SIZE);

      root_node->clear_children();

      for (int patch_z = centre_patch_z - 1; patch_z <= centre_patch_z + 1; patch_z++) {
        for (int patch_x = centre_patch_x - 1; patch_x <= centre_patch_x + 1; patch_x++) {
          int patch_index = terrain.get_patch_index(patch_x, patch_z);

          std::shared_ptr<TerrainPatch> patch(terrain.patches_[patch_index]);
          //if (patch->dirty) {
         //   bake_patch(patch_x, patch_z);
         // }

          patch->shader_params->set_texture("splatt", patch->texture);

          std::shared_ptr<fw::sg::Node> node = patch->node_;
          if (!node) {
            node = patch->node_ = std::make_shared<fw::sg::Node>();

            // we have to set up the Scenegraph Node with these manually
            node->set_vertex_buffer(patch->vb);
            node->set_index_buffer(terrain.ib_);
            node->set_shader(terrain.shader_);
            node->set_shader_parameters(patch->shader_params);
            node->set_primitive_type(fw::sg::PrimitiveType::kTriangleStrip);
          }
          fw::Matrix world = fw::translation(
            static_cast<float>(patch_x * PATCH_SIZE), 0,
            static_cast<float>(patch_z * PATCH_SIZE));
          node->set_world_matrix(world);

          // set up the world matrix for this patch so that it's being rendered at the right offset
          root_node->add_child(patch->node_);
        }
      }
    });
}

int Terrain::get_patch_index(int patch_x, int patch_z, int *new_patch_x, int *new_patch_z) const {
  patch_x = fw::constrain(patch_x, get_patches_width());
  patch_z = fw::constrain(patch_z, get_patches_length());

  if (new_patch_x != 0)
    *new_patch_x = patch_x;
  if (new_patch_z != 0)
    *new_patch_z = patch_z;

  return patch_z * get_patches_width() + patch_x;
}

float Terrain::get_vertex_height(int x, int z) {
  return heights_[fw::constrain(z, length_) * width_ + fw::constrain(x, width_)];
}

// this method is pretty simple. we basically get the height at (x, z) then interpolate that value between (x+1, z+1).
float Terrain::get_height(float x, float z) {
  int x0 = (int) floor(x);
  int x1 = x0 + 1;

  int z0 = (int) floor(z);
  int z1 = z0 + 1;

  float h00 = get_vertex_height(x0, z0);
  float h10 = get_vertex_height(x1, z0);

  float h01 = get_vertex_height(x0, z1);
  float h11 = get_vertex_height(x1, z1);

  float dx = x - x0;
  float dz = z - z0;
  float dxdz = dx * dz;

  return h00 * (1.0f - dz - dx + dxdz) + h10 * (dx - dxdz) + h11 * dxdz + h01 * (dz - dxdz);
}

// gets the point on the terrain that the camera is currently looking at
fw::Vector Terrain::get_camera_lookat() {
  fw::Framework *frmwrk = fw::Framework::get_instance();
  fw::Camera *camera = frmwrk->get_camera();

  fw::Vector center = camera->unproject(0.0f, 0.0f);
  fw::Vector start = camera->get_position();
  fw::Vector direction = (center - start).normalized();

  return get_cursor_location(start, direction);
}

// This method is fairly simple, we just trace a line from the camera through the cursor point to the end of the
// terrain, projected in the (x,y) plane (so it's a nice, easy 2D line). Then, for each 2D point on that line, we check
// how close the ray is to that point in 3D-space. If it's close enough, we return that one.
fw::Vector Terrain::get_cursor_location() {
  fw::Framework *frmwrk = fw::Framework::get_instance();
  fw::Input *Input = frmwrk->get_input();
  fw::Camera *camera = frmwrk->get_camera();

  float mx = (float) Input->mouse_x();
  float my = (float) Input->mouse_y();

  mx = 1.0f - (2.0f * mx / fw::Get<fw::Graphics>().get_width());
  my = 1.0f - (2.0f * my / fw::Get<fw::Graphics>().get_height());

  fw::Vector mvec = camera->unproject(-mx, my);

  fw::Vector start = camera->get_position();
  fw::Vector direction = (mvec - start).normalized();

  return get_cursor_location(start, direction);
}

fw::Vector Terrain::get_cursor_location(fw::Vector const &start, fw::Vector const &direction) {
  fw::Vector evec = start + (direction * 150.0f);
  fw::Vector svec = start + (direction * 5.0f);

  // TODO: we use the same algorithm here and in PathFfind::is_passable(fw::Vector const &start, fw::Vector const &end)
  // can we factor it out?
  int sx = static_cast<int>(floor(svec[0] + 0.5f));
  int sz = static_cast<int>(floor(svec[2] + 0.5f));
  int ex = static_cast<int>(floor(evec[0] + 0.5f));
  int ez = static_cast<int>(floor(evec[2] + 0.5f));

  int dx = ex - sx;
  int dz = ez - sz;
  int abs_dx = abs(dx);
  int abs_dz = abs(dz);

  int steps = (abs_dx > abs_dz) ? abs_dx : abs_dz;

  float xinc = dx / static_cast<float>(steps);
  float zinc = dz / static_cast<float>(steps);

  float x = static_cast<float>(sx);
  float z = static_cast<float>(sz);
  for (int i = 0; i <= steps; i++) {
    // We actually check in a 9x9 matrix around this point, to make sure we check all possible candidates. Because we
    // return as soon as match is found, this shouldn't be too bad...
    int ix = static_cast<int>(floor(x + 0.5f));
    int iz = static_cast<int>(floor(z + 0.5f));
    for (int oz = iz - 1; oz <= iz + 1; oz++) {
      for (int ox = ix - 1; ox <= ix + 1; ox++) {
        float x1 = static_cast<float>(ox);
        float x2 = static_cast<float>(ox + 1);
        float z1 = static_cast<float>(oz);
        float z2 = static_cast<float>(oz + 1);

        fw::Vector p11(x1, get_vertex_height(static_cast<int>(x1), static_cast<int>(z1)), z1);
        fw::Vector p21(x2, get_vertex_height(static_cast<int>(x2), static_cast<int>(z1)), z1);
        fw::Vector p12(x1, get_vertex_height(static_cast<int>(x1), static_cast<int>(z2)), z2);
        fw::Vector p22(x2, get_vertex_height(static_cast<int>(x2), static_cast<int>(z2)), z2);

        fw::Vector n1 = fw::cross(p12 - p11, p21 - p11);
        fw::Vector n2 = fw::cross(p21 - p22, p12 - p22);

        // The line intersects the plane defined by the first triangle at i1. If that's within this triangle's "x,z"
        // coordinates, this is the intersection point!
        fw::Vector i1 = fw::point_plane_intersect(p11, n1, start, direction);
        if (i1[0] > x1 && i1[0] <= x2 && i1[2] > z1 && i1[2] <= z2) {
          return i1;
        }

        // Same calculation for the second triangle
        fw::Vector i2 = fw::point_plane_intersect(p22, n2, start, direction);
        if (i2[0] > x1 && i2[0] <= x2 && i2[2] > z1 && i2[2] <= z2) {
          return i2;
        }
      }
    }

    x += xinc;
    z += zinc;
  }

  return fw::Vector(0, 0, 0);
}
}
