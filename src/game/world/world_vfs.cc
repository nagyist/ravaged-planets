#include <filesystem>
#include <vector>

#include <absl/strings/str_cat.h>

#include <framework/bitmap.h>
#include <framework/logging.h>
#include <framework/misc.h>
#include <framework/paths.h>
#include <framework/status.h>
#include <framework/xml.h>

#include <game/world/world_vfs.h>

namespace fs = std::filesystem;

namespace game {
namespace {

void PopulateMaps(std::vector<game::WorldSummary> &list, fs::path path) {
  LOG(INFO) << "populating maps from: " << path.string();

  if (!fs::exists(path) || !fs::is_directory(path))
    return;

  for (fs::directory_iterator it(path); it != fs::directory_iterator(); ++it) {
    fs::path p(*it);
    LOG(DBG) << "  - " << p.string();
    if (fs::is_directory(p)) {
      game::WorldSummary ws;
      ws.initialize(p.filename().string());

      // todo: see if it already exists before adding it
      list.push_back(ws);
    }
  }
}

fw::StatusOr<fs::path> FindMap(std::string name) {
  fs::path p(fw::install_base_path() / "maps" / name);
  if (fs::is_directory(p))
    return p;

  p = fw::user_base_path() / "maps" / name;
  if (fs::is_directory(p))
    return p;

  return fw::ErrorStatus("could not find map: ") << name;
}

}

WorldSummary::WorldSummary() :
    name_(""), extra_loaded_(false), screenshot_(nullptr), width_(0), height_(0), num_players_(0) {
}

WorldSummary::WorldSummary(WorldSummary const &copy) :
    name_(copy.name_), extra_loaded_(false), screenshot_(nullptr), width_(0), height_(0),
    num_players_(0) {
}

WorldSummary::~WorldSummary() {
}

void WorldSummary::ensure_extra_loaded() const {
  if (extra_loaded_)
    return;

  auto full_path = FindMap(name_);
  if (!full_path.ok()) {
    LOG(ERR) << "map does not exist: " << name_ << ": " << full_path.status();
    return;
  }

  auto screenshot_path = (*full_path) / "screenshot.png";
  if (fs::exists(screenshot_path)) {
    auto screenshot = fw::LoadBitmap((*full_path) / "screenshot.png");
    if (screenshot.ok()) {
      screenshot_ = *screenshot;
    } else {
      LOG(ERR) << "error loading world screenshot: " << screenshot.status();
    }
  }

  auto status = ParseMapdescFile((*full_path) / (name_ + ".mapdesc"));
  if (!status.ok()) {
    LOG(ERR) << "error parsing mapdesc file: " << status;
  }

  extra_loaded_ = true;
}

fw::Status WorldSummary::ParseMapdescFile(fs::path const &filename) const {
  ASSIGN_OR_RETURN(fw::XmlElement xml, fw::LoadXml(filename, "mapdesc", 1));

  for (fw::XmlElement child : xml.children()) {
    if (child.get_value() == "description") {
      description_ = child.get_text();
    } else if (child.get_value() == "author") {
      author_ = child.get_text();
    } else if (child.get_value() == "size") {
      ASSIGN_OR_RETURN(width_, child.GetAttributei<int>("width"));
      ASSIGN_OR_RETURN(height_, child.GetAttributei<int>("height"));
    } else if (child.get_value() == "players") {
      num_players_ = 0;
      for (fw::XmlElement player : child.children()) {
        if (player.get_value() == "player") {
          num_players_ ++;
        } else {
          LOG(WARN) << "unknown child of <players>: " << child.get_value();
        }
      }
    } else {
      LOG(WARN) << "unknown child of <mapdesc>: " << child.get_value();
    }
  }
  return fw::OkStatus();
}

void WorldSummary::initialize(std::string map_file) {
  name_ = map_file;
}

//----------------------------------------------------------------------------

std::vector<WorldSummary> ListMaps() {
  std::vector<WorldSummary> list;
  PopulateMaps(list, fw::install_base_path() / "maps");
  PopulateMaps(list, fw::user_base_path() / "maps");

  // todo: sort

  return list;
}

fw::StatusOr<WorldFile> OpenWorldFile(std::string name, bool for_writing /*= false*/) {
  // check the user's profile directory first
  fs::path map_path = fw::user_base_path() / "maps";
  fs::path full_path = map_path / name;
  if (fs::is_directory(full_path) || for_writing) {
    // if it's for writing, we'll want to create the above directory and write the files
    // there anyway. At least for now, that's the easiest way....
    fs::create_directories(full_path);
    return WorldFile(full_path.string());
  }

  if (!for_writing) {
    // if it's not for writing, check the install directory as well
    map_path = fw::install_base_path() / "maps";
    fs::path full_path = map_path / name;
    return WorldFile(full_path.string());
  }

  // todo: not for writing, check other locations...
  return fw::ErrorStatus(absl::StrCat("map '", name, "' doesn't exist"));
}

//-------------------------------------------------------------------------

WorldFileEntry::WorldFileEntry(std::string full_path, bool for_write) :
    full_path_(full_path), for_write_(for_write) {
}

WorldFileEntry::WorldFileEntry(WorldFileEntry const &copy) : for_write_(false) {
  this->copy(copy);
}

WorldFileEntry::~WorldFileEntry() {
  close();
}

WorldFileEntry &WorldFileEntry::operator =(WorldFileEntry const &copy) {
  this->copy(copy);
  return (*this);
}

void WorldFileEntry::copy(WorldFileEntry const &copy) {
  close();

  full_path_ = copy.full_path_;
  stream_.copyfmt(copy.stream_);
  for_write_ = copy.for_write_;
}

fw::Status WorldFileEntry::EnsureOpen() {
  if (stream_.is_open()) {
    return fw::OkStatus();
  }

  if (for_write_) {
    stream_.open(full_path_.c_str(), std::ios::out | std::ifstream::binary);
  } else {
    stream_.open(full_path_.c_str(), std::ios::in | std::ifstream::binary);
  }
  if (stream_.fail()) {
    return fw::ErrorStatus("entry is not open: ") << full_path_;
  }

  return fw::OkStatus();
}

void WorldFileEntry::close() {
  if (stream_.is_open()) {
    stream_.close();
  }
}

void WorldFileEntry::write(void const *buffer, int num_bytes) {
  if (EnsureOpen().ok()) {
    stream_.write(reinterpret_cast<char const *>(buffer), num_bytes);
  }
}

void WorldFileEntry::write(std::string const &line) {
  std::string full_line(fw::StripTrailingSpaces(line));
  full_line += "\r\n";

  write(full_line.c_str(), full_line.length());
}

void WorldFileEntry::read(void *buffer, int num_bytes) {
  if (EnsureOpen().ok()) {
    stream_.read(reinterpret_cast<char *>(buffer), num_bytes);
  }
}

bool WorldFileEntry::exists() {
  if (!EnsureOpen().ok()) {
    return false;
  }
  return (stream_.is_open());
}

//-------------------------------------------------------------------------

WorldFile::WorldFile(std::string path) :
    path_(path) {
}

WorldFileEntry WorldFile::get_entry(std::string name, bool for_write) {
  fs::path file_path = fs::path(path_) / name;
  return WorldFileEntry(file_path.string(), for_write);
}

}  // namespace game
