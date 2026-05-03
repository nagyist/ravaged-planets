#include <game/world/world_reader.h>

#include <memory>

#include <absl/strings/numbers.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_split.h>

#include <framework/bitmap.h>
#include <framework/framework.h>
#include <framework/graphics.h>
#include <framework/logging.h>
#include <framework/misc.h>
#include <framework/status.h>
#include <framework/xml.h>

#include <game/world/world.h>
#include <game/world/world_vfs.h>
#include <game/world/terrain.h>

namespace game {

WorldReader::WorldReader() :
    terrain_(nullptr) {
}

WorldReader::~WorldReader() {
}

fw::Status WorldReader::Read(std::string name) {
  ASSIGN_OR_RETURN(WorldFile wf, OpenWorldFile(name, false));

  int version;
  int trn_width;
  int trn_length;

  WorldFileEntry wfe = wf.get_entry("heightfield", false /* for_write */);
  wfe.read(&version, sizeof(int));
  if (version != 1) {
    return fw::ErrorStatus("unknown terrain version: ") << version;
  }

  wfe.read(&trn_width, sizeof(int));
  wfe.read(&trn_length, sizeof(int));

  float *height_data = new float[trn_width * trn_length];
  wfe.read(height_data, trn_width * trn_length * sizeof(float));

  name_ = name;
  ASSIGN_OR_RETURN(terrain_, create_terrain(trn_width, trn_length, height_data));

  for (int patch_z = 0; patch_z < terrain_->get_patches_length(); patch_z++) {
    for (int patch_x = 0; patch_x < terrain_->get_patches_width(); patch_x++) {
      std::string name = absl::StrCat("splatt-", patch_x, "-", patch_z, ".png");
      wfe = wf.get_entry(name, false /* for_write */);

      ASSIGN_OR_RETURN(auto splatt, fw::LoadBitmap(wfe.get_full_path().c_str()));
      terrain_->set_splatt(patch_x, patch_z, splatt);
    }
  }

  wfe = wf.get_entry("minimap.png", false /* for_write */);
  if (wfe.exists()) {
    wfe.close();

    ASSIGN_OR_RETURN(minimap_background_, fw::LoadBitmap(wfe.get_full_path()));
  }

  wfe = wf.get_entry("screenshot.png", false /* for_write */);
  if (wfe.exists()) {
    wfe.close();

    ASSIGN_OR_RETURN(screenshot_, fw::LoadBitmap(wfe.get_full_path()));
  }

  wfe = wf.get_entry(name + ".mapdesc", false /* for_write */);
  if (wfe.exists()) {
    wfe.close();

    ASSIGN_OR_RETURN(fw::XmlElement root, fw::LoadXml(wfe.get_full_path(), "mapdesc", 1));
    RETURN_IF_ERROR(ReadMapdesc(root));
  }

  wfe = wf.get_entry("collision_data", false /* for_write */);
  if (wfe.exists()) {
    RETURN_IF_ERROR(ReadCollisionData(wfe));
  }

  return fw::OkStatus();
}

fw::Status WorldReader::ReadCollisionData(WorldFileEntry &wfe) {
  int version;
  wfe.read(&version, sizeof(int));
  if (version != 1) {
    return fw::ErrorStatus("unknown collision_data version: ") << version;
  }

  int width, length;
  wfe.read(&width, sizeof(int));
  wfe.read(&length, sizeof(int));

  for (int i = 0; i < (width * length); i++) {
    uint8_t n;
    wfe.read(&n, sizeof(uint8_t));
    terrain_->collision_data_.push_back(n != 0);
  }

  return fw::OkStatus();
}

fw::Status WorldReader::ReadMapdesc(fw::XmlElement root) {
  for (fw::XmlElement child : root.children()) {
    if (child.get_value() == "description") {
      description_ = child.get_text();
    } else if (child.get_value() == "author") {
      author_ = child.get_text();
    } else if (child.get_value() == "size") {
    } else if (child.get_value() == "players") {
      RETURN_IF_ERROR(ReadMapdescPlayers(child));
    } else {
      return fw::ErrorStatus("Unknown child element of <mapdesc> node: ") << child.get_value();
    }
  }

  return fw::OkStatus();
}

fw::Status WorldReader::ReadMapdescPlayers(fw::XmlElement players_node) {
  for (fw::XmlElement child : players_node.children()) {
    if (child.get_value() != "player") {
      return fw::ErrorStatus("unknown child element of <players> node: ") << child.get_value();
    }

    ASSIGN_OR_RETURN(int player_no, child.GetAttributei<int>("no"));
    ASSIGN_OR_RETURN(std::string start_str, child.GetAttribute("start"));
    std::vector<std::string> start_components = absl::StrSplit(start_str, " ");
    if (start_components.size() != 2) {
      return fw::ErrorStatus("<player> node has invalid 'start' attribute: ") << start_str;
    }

    float x, z;
    if (!absl::SimpleAtof(start_components[0], &x) || !absl::SimpleAtof(start_components[1], &z)) {
      return fw::ErrorStatus("<player> node has invalid 'start' attribute: ") << start_str;
    }

    player_starts_[player_no] = fw::Vector(x, 0.0f, z);
  }

  return fw::OkStatus();
}

fw::StatusOr<std::shared_ptr<Terrain>> WorldReader::create_terrain(
    int width, int length, float* height_data) {
  auto terrain = std::make_shared<Terrain>(width, length, height_data);
  RETURN_IF_ERROR(terrain->initialize());
  return terrain;
}

}
