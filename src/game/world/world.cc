#include <filesystem>
#include <format>
#include <functional>

#include <absl/strings/match.h>

#include <framework/bitmap.h>
#include <framework/framework.h>
#include <framework/logging.h>
#include <framework/input.h>
#include <framework/particle_manager.h>
#include <framework/camera.h>
#include <framework/paths.h>
#include <framework/scenegraph.h>

#include <game/ai/pathing_thread.h>
#include <game/entities/entity_manager.h>
#include <game/entities/ownable_component.h>
#include <game/world/cursor_handler.h>
#include <game/world/world.h>
#include <game/world/world_reader.h>
#include <game/world/terrain.h>
#include <game/screens/hud/pause_window.h>
#include <game/simulation/simulation_thread.h>
#include <game/simulation/local_player.h>

namespace fs = std::filesystem;
using namespace std::placeholders;

namespace game {

//-------------------------------------------------------------------------
World *World::instance_ = nullptr;

World::World(std::shared_ptr<WorldReader> reader) :
    reader_(reader), entities_(nullptr), pathing_(nullptr), initialized_(false) {
  cursor_ = new CursorHandler();
}

World::~World() {
  World::set_instance(nullptr);
  delete cursor_;
  if (pathing_ != nullptr)
    delete pathing_;
}

void World::initialize() {
  terrain_ = reader_->get_terrain();
  minimap_background_ = reader_->get_minimap_background();
  name_ = reader_->get_name();
  description_ = reader_->get_description();
  author_ = reader_->get_author();
  screenshot_ = reader_->get_screenshot();

  for (auto it : reader_->get_player_starts()) {
    player_starts_[it.first] = it.second;
  }

  // tell the Particle manager to wrap particles at the world boundary
  fw::Framework::get_instance()->get_particle_mgr()->set_world_wrap(
      terrain_->get_width(), terrain_->get_length());

  fw::Input *input = fw::Framework::get_instance()->get_input();

  World::set_instance(this);

  initialize_entities();
  if (entities_ != nullptr) {
    cursor_->initialize();
    keybind_tokens_.push_back(
        input->bind_function("pause", std::bind(&World::on_key_pause, this, _1, _2)));
  }
  keybind_tokens_.push_back(
      input->bind_function("screenshot", std::bind(&World::on_key_screenshot, this, _1, _2)));

  initialize_pathing();

  initialized_ = true;
}

void World::destroy() {
  pathing_->stop();

  // unbind all the keys we had bound
  fw::Input *Input = fw::Framework::get_instance()->get_input();
  for (int token : keybind_tokens_) {
    Input->unbind_key(token);
  }
  keybind_tokens_.clear();
  cursor_->destroy();
}

void World::initialize_pathing() {
  pathing_ = new PathingThread();
  pathing_->start();
}

void World::initialize_entities() {
  entities_ = new ent::EntityManager();
  entities_->initialize();
}

// this is called when the "pause" button is pressed (usually "ESC")
void World::on_key_pause(std::string, bool is_down) {
  if (!is_down) {
    if (fw::Framework::get_instance()->is_paused()) {
      fw::Framework::get_instance()->unpause();
    } else {
      hud_pause->show();
      fw::Framework::get_instance()->pause();
    }
  }
}

void World::on_key_screenshot(std::string, bool is_down) {
  if (!is_down) {
    LOG(INFO) << "requesting screenshot";
    fw::Framework::get_instance()->take_screenshot(
        0, 0, std::bind(&World::screenshot_callback, this, _1));
  }
}

void World::screenshot_callback(fw::Bitmap const &screenshot) {
  // screenshots go under the data directory\screens folder
  fs::path base_path = fw::resolve("screens", true);
  if (!fs::exists(base_path)) {
    fs::create_directories(base_path);
  }

  // create a file called "Screen-nnnn.png" where nnnn is a number from 1 to 9999 and make
  // sure the name is unique
  int max_file_number = 0;
  for (fs::directory_iterator it(base_path); it != fs::directory_iterator(); ++it) {
    fs::path p(*it);
    if (fs::is_regular_file(p)) {
      std::string filename = p.filename().string();
      if (absl::StartsWithIgnoreCase(filename, "screen-") &&
          absl::EndsWithIgnoreCase(filename, ".png")) {
        std::string number_part = filename.substr(7, filename.length() - 11);
        int number;
        if (absl::SimpleAtoi(number_part, &number) && number > max_file_number) {
          max_file_number = number;
        }
      }
    }
  }

  fs::path full_path = base_path / std::format("screen-{:04d}.png", (max_file_number + 1));
  auto status = screenshot.SaveBitmap(full_path.string());
  if (!status.ok()) {
    LOG(ERR) << "error saving screenshot: " << status;
  }
}

void World::update() {
  if (!initialized_) {
    return;
  }
  // if update is called, we can't be paused so hide the "pause" menu...
  if (hud_pause != nullptr && hud_pause->is_visible()) {
    hud_pause->hide();
  }

  cursor_->update();
  terrain_->update();

  if (entities_ != nullptr) {
    entities_->update();
  }
}

}
