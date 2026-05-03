#include <memory>

#include <absl/strings/str_cat.h>

#include <framework/framework.h>
#include <framework/logging.h>
#include <framework/bitmap.h>
#include <framework/color.h>
#include <framework/graphics.h>
#include <framework/misc.h>
#include <framework/status.h>
#include <framework/texture.h>

#include <game/world/world_vfs.h>
#include <game/world/terrain.h>
#include <game/editor/world_writer.h>
#include <game/editor/editor_world.h>
#include <game/editor/editor_terrain.h>

namespace ed {

WorldWriter::WorldWriter(EditorWorld *wrld) :
    world_(wrld) {
}

WorldWriter::~WorldWriter() {
}

fw::Status WorldWriter::write(std::string name) {
  name_ = name;

  ASSIGN_OR_RETURN(game::WorldFile wf, game::OpenWorldFile(name, /* for_writing= */ true));

  RETURN_IF_ERROR(write_terrain(wf));
  write_mapdesc(wf);
  RETURN_IF_ERROR(WriteMinimapBackground(wf));
  RETURN_IF_ERROR(WriteCollisionData(wf));

  // write the screenshot as well, which is pretty simple...
  if (world_->get_screenshot().get_width() > 0) {
    game::WorldFileEntry wfe = wf.get_entry("screenshot.png", true /* for_write */);
    auto status = world_->get_screenshot().SaveBitmap(wfe.get_full_path());
    if (!status.ok()) {
      return status;
    }
  }

  return fw::OkStatus();
}

fw::Status WorldWriter::write_terrain(game::WorldFile &wf) {
  auto trn = dynamic_pointer_cast<EditorTerrain>(world_->get_terrain());
  int version = 1;
  int trn_width = trn->get_width();
  int trn_length = trn->get_length();

  game::WorldFileEntry wfe = wf.get_entry("heightfield", true /* for_write */);
  wfe.write(&version, sizeof(int));
  wfe.write(&trn_width, sizeof(int));
  wfe.write(&trn_length, sizeof(int));
  wfe.write(trn->heights_, trn_width * trn_length * sizeof(float));

  for (int patch_z = 0; patch_z < trn->get_patches_length(); patch_z++) {
    for (int patch_x = 0; patch_x < trn->get_patches_width(); patch_x++) {
      fw::Bitmap &splatt = trn->get_splatt(patch_x, patch_z);

      std::string name = absl::StrCat("splatt-", patch_x, "-", patch_z, ".png");
      wfe = wf.get_entry(name, true /* for_write */);
      RETURN_IF_ERROR(splatt.SaveBitmap(wfe.get_full_path()));
    }
  }
  return fw::OkStatus();
}

void WorldWriter::write_mapdesc(game::WorldFile &wf) {
  game::WorldFileEntry wfe = wf.get_entry(name_ + ".mapdesc", true /* for_write */);
  wfe.write("<mapdesc version=\"1\">");
  wfe.write(absl::StrCat("  <description>", world_->get_description(), "</description>"));
  wfe.write(absl::StrCat("  <author>", world_->get_author(), "</author>"));
  wfe.write("  <size width=\"3\" height=\"3\" />");
  wfe.write("  <players>");
  for (std::map<int, fw::Vector>::iterator it = world_->get_player_starts().begin();
      it != world_->get_player_starts().end(); ++it) {
    wfe.write(
      absl::StrCat(
        "    <player no=\"", it->first, "\" start=\"", it->second[0], " ", it->second[2], "\" />"));
  }
  wfe.write("  </players>");
  wfe.write("</mapdesc>");
}

// The minimap background consist of basically one pixel per vertex. We calculate the color
// of the pixel as a combination of the height of the terrain at that point and the texture that
// is displayed on the terrain at that point (so "high" and "grass" would be a Light green, etc)
fw::Status WorldWriter::WriteMinimapBackground(game::WorldFile &wf) {
  auto trn = world_->get_terrain();
  int width = trn->get_width();
  int height = trn->get_length();

  // calculate the base colors we use for the minimap (basically the "average" color
  // of each splatt texture)
  calculate_base_minimap_colors();

  std::vector<uint32_t> pixels(width * height);
  for (int z = 0; z < height; z++) {
    for (int x = 0; x < width; x++) {
      // get the base color of the terrain
      fw::Color col = get_terrain_color(x, z);

      // we'll normalize the height so it's between 0.25 and 1.75
      float height = trn->get_vertex_height(x, z);
      height = (20.0f + height) / 60.0f; // -20 becomes 0 and +40 becomes 1
      height *= 2.0f;

      if (height < 0.25f)
        height = 0.25f;
      if (height > 1.75f)
        height = 1.75f;

      // adjust the base color so that it's lighter when it's higher, darker when it's lower, etc
      col *= height;

      if (col.r < 0.0f)
        col.r = 0.0f;
      if (col.r > 1.0f)
        col.r = 1.0f;
      if (col.g < 0.0f)
        col.g = 0.0f;
      if (col.g > 1.0f)
        col.g = 1.0f;
      if (col.b < 0.0f)
        col.b = 0.0f;
      if (col.b > 1.0f)
        col.b = 1.0f;

      // make sure the alpha is 1.0
      col.a = 1.0f;

      int index = x + (z * width);
      pixels[index] = col.to_abgr();
    }
  }

  fw::Bitmap img(width, height);
  img.SetPixels(pixels);

  game::WorldFileEntry wfe = wf.get_entry("minimap.png", true /* for_write */);
  return img.SaveBitmap(wfe.get_full_path());
}

// gets the basic color of the terrain at the given (x,z) location
fw::Color WorldWriter::get_terrain_color(int x, int z) {
  int patch_x = static_cast<int>(static_cast<float>(x) / game::Terrain::PATCH_SIZE);
  int patch_z = static_cast<int>(static_cast<float>(z) / game::Terrain::PATCH_SIZE);

  auto trn = std::dynamic_pointer_cast<EditorTerrain>(world_->get_terrain());
  fw::Bitmap &bmp = trn->get_splatt(patch_x, patch_z);

  // centre_u and centre_v are the texture coordinates (in the range [0..1])
  // of what the cursor is currently pointing at
  float centre_u = (x - (patch_x * game::Terrain::PATCH_SIZE)) / static_cast<float>(game::Terrain::PATCH_SIZE);
  float centre_v = (z - (patch_z * game::Terrain::PATCH_SIZE)) / static_cast<float>(game::Terrain::PATCH_SIZE);

  // centre_x and centre_y are the (x,y) coordinates (in texture space) of the splatt texture where the cursor
  // is currently pointing.
  int centre_x = static_cast<int>(centre_u * bmp.get_width());
  int centre_y = static_cast<int>(centre_v * bmp.get_height());

  fw::Color splatt_color = bmp.GetPixel(centre_x, centre_y);

  fw::Color final_color(0, 0, 0);
  final_color += base_minimap_colors_[0] * splatt_color.a;
  final_color += base_minimap_colors_[1] * splatt_color.r;
  final_color += base_minimap_colors_[2] * splatt_color.g;
  final_color += base_minimap_colors_[3] * splatt_color.b;
  final_color.a = 1.0f;

  return final_color;
}

void WorldWriter::calculate_base_minimap_colors() {
  auto trn = std::dynamic_pointer_cast<EditorTerrain>(world_->get_terrain());
  for (int i = 0; i < 4; i++) {
     // Use the average color of this layer
    fw::Bitmap const &layer_bmp = trn->get_layer(i);
    base_minimap_colors_[i] = layer_bmp.GetDominantColor();
    LOG(DBG) << "base minimap color [" << i << "] = " << base_minimap_colors_[i];
  }
}

fw::Status WorldWriter::WriteCollisionData(game::WorldFile &wf) {
  auto trn = std::dynamic_pointer_cast<EditorTerrain>(world_->get_terrain());
  int width = trn->get_width();
  int length = trn->get_length();

  std::vector<bool> collision_data(width * length);
  RETURN_IF_ERROR(trn->BuildCollisionData(collision_data));

  int version = 1;

  game::WorldFileEntry wfe = wf.get_entry("collision_data", true /* for_write */);
  wfe.write(&version, sizeof(int));
  wfe.write(&width, sizeof(int));
  wfe.write(&length, sizeof(int));

  for(bool b : collision_data) {
    uint8_t n = static_cast<uint8_t>(b);
    wfe.write(&n, 1);
  }

  return fw::OkStatus();
}

}
