#include <iostream>
#include <fstream>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

#include <framework/lang.h>
#include <framework/paths.h>
#include <framework/logging.h>

namespace fs = boost::filesystem;

namespace fw {

//-------------------------------------------------------------------------
// reads a line from the given .lang file and returns the key/value pair
static bool get_lang_line(std::fstream &fs, std::string &key,
    std::string &value, std::string const &file_name, int &line_num);

//-------------------------------------------------------------------------

lang::lang(std::string const &lang_name) :
    _lang_name(lang_name) {
  std::fstream ins;

  fs::path lang_path = fw::resolve("lang/" + lang_name);
  if (fs::is_regular_file(lang_path)) {
    ins.open(lang_path.string().c_str());
  }
  if (!ins) {
    debug << boost::format("WARN: could not find language file: %1%, loading en.lang instead.") % lang_name
        << std::endl;
  } else {
    debug << boost::format("loading language: \"%1%\"") % lang_path.string() << std::endl;

    std::string key, value;
    int line_num = 0;
    while (get_lang_line(ins, key, value, lang_path.string(), line_num)) {
      _strings[key] = value;
    }
  }

  // always load English (if lang_name isn't already English, of course...)
  if (lang_name != "en.lang") {
    lang_path = fw::resolve("lang/en.lang");
    ins.open(lang_path.string().c_str());

    std::string key, value;
    int line_num = 0;
    while (get_lang_line(ins, key, value, lang_path.string(), line_num)) {
      _def_strings[key] = value;
    }
  }
}

std::string lang::get_string(std::string const &name) {
  auto it = _strings.find(name);
  if (it == _strings.end()) {
    it = _def_strings.find(name);
    if (it == _def_strings.end()) {
      debug << boost::format("WARN: string \"%1%\" does not exist in %2% *or* in en.lang!") % name % _lang_name
          << std::endl;
      return name;
    } else {
      debug << boost::format("WARN: string \"%1%\" does not exist in %2%") % name % _lang_name << std::endl;
    }
  }

  return it->second;
}

//-------------------------------------------------------------------------
std::vector<lang_description> g_langs;

static void populate_lang_description(lang_description &desc, fs::path file_name) {
  desc.name = file_name.leaf().string();
  std::fstream ins(file_name.string().c_str());

  int line_num = 0;
  std::string key, value;
  while (get_lang_line(ins, key, value, file_name.string(), line_num)) {
    if (key == "lang.name") {
      // once we find the "lang-name" line, we can ignore everything else
      desc.display_name = value;
      return;
    }
  }
}

std::vector<lang_description> get_languages() {
  if (g_langs.size() > 0)
    return g_langs;

  auto lang_path = fw::resolve("lang");

  fs::directory_iterator end;
  for (fs::directory_iterator it(lang_path); it != end; ++it) {
    if (it->path().extension() != ".lang")
      continue;

    lang_description desc;
    populate_lang_description(desc, it->path());
    g_langs.push_back(desc);
  }

  return g_langs;
}

//-------------------------------------------------------------------------
bool get_lang_line(std::fstream &fs, std::string &key, std::string &value,
    std::string const &file_name, int &line_num) {
  key.clear();
  value.clear();

  std::string line;
  while (std::getline(fs, line)) {
    line_num++;

    // remove comments (everything after "#") - split the line by the first '#'
    // and discard the second part (if any)
    std::vector<std::string> parts;
    boost::split(parts, line, boost::is_any_of("#"));
    line = parts[0];

    // check for a BOM and remove it as well...
    if (line.size() >= 3 && line[0] == '\xEF' && line[1] == '\xBB' && line[2] == '\xBF') {
      line = line.substr(3);
    }

    // remove whitespace before and after, and check for empty string (skip it)
    line = boost::trim_copy(line);
    if (line == "")
      continue;

    if (key.size() == 0) {
      // if this is the first line in a sequence, we'll need to split on the first "="
      int equals = line.find('=');
      if (equals == std::string::npos) {
        debug << boost::format("WARN: invalid line in %1%(%2%), expected to find '='") % file_name % line_num
            << std::endl;
        continue;
      }

      key = boost::trim_right_copy(line.substr(0, equals));
      value = boost::trim_left_copy(line.substr(equals + 1));
    } else {
      // if this is a continuation from a previous line, just append it to the value
      value += line;
    }

    if (value[value.size() - 1] == '\\') {
      // if the last character in the line is a '\' it means the line
      // continues on to the next line. strip off the \ and keep looping
      value = value.substr(0, value.size() - 1);
    } else {
      // otherwise, we've found a valid string!
      return true;
    }
  }

  // if we get here, it means we got to the end of the file before we found
  // a valid string.
  return false;
}

}
