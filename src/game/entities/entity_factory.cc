#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>

#include <framework/xml.h>
#include <framework/logging.h>
#include <framework/paths.h>

#include <game/entities/entity.h>
#include <game/entities/entity_factory.h>
#include <game/entities/buildable_component.h>

namespace fs = boost::filesystem;

namespace ent {

typedef std::map<std::string, fw::lua_context *> entity_template_map;
static entity_template_map *entity_templates = nullptr;
static std::map<std::string, std::function<entity_component *()>> *comp_registry = nullptr;

entity_factory::entity_factory() {
  if (entity_templates == nullptr) {
    load_entities();
  }
}

entity_factory::~entity_factory() {
}

void entity_factory::populate(std::shared_ptr<entity> ent, std::string name) {
  // first, find the template we'll use for creating the entity
  luabind::object entity_template = get_template(name);
  if (!entity_template) {
    fw::debug << boost::format("  warning: unknown entity: %1%") % name << std::endl;
    return;
  }

  // add all of the attributes before we add any of the components
  for (luabind::iterator it(entity_template), end; it != end; ++it) {
    if (it.key() == "components") {
      continue;
    }
    boost::any value;
    int type = luabind::type(*it);
    if (type == LUA_TNUMBER) {
      value = luabind::object_cast<float>(*it);
    } else if (type == LUA_TSTRING) {
      value = luabind::object_cast<std::string>(*it);
    } else {
      // table? maybe a vector?
    }
    entity_attribute attr(luabind::object_cast<std::string>(it.key()), value);
    ent->add_attribute(attr);
  }

  // then add all of the components as well
  for (luabind::iterator it(entity_template["components"]), end; it != end; ++it) {
    luabind::object comp_tmpl(*it);
    entity_component *comp = create_component(luabind::object_cast<std::string>(it.key()));
    if (comp != nullptr) {
      comp->apply_template(comp_tmpl);
      ent->add_component(comp);
      comp->set_entity(ent);
    }
  }
}

luabind::object entity_factory::get_template(std::string name) {
  entity_template_map::iterator it = entity_templates->find(name);
  if (it == entity_templates->end()) {
    return luabind::object();
  }

  fw::lua_context *ctx = it->second;
  return luabind::globals(*ctx)["Entity"];
}

// gets the complete list of entity_templates
void entity_factory::get_templates(std::vector<luabind::object> &templates) {
  BOOST_FOREACH(auto &kvp, *entity_templates){
    fw::lua_context *ctx = kvp.second;
    templates.push_back(luabind::globals(*ctx)["Entity"]);
  }
}

// helper method that populates a vector with entities that are buildable (and in the given build_group)
void entity_factory::get_buildable_templates(std::string const &build_group,
      std::vector<luabind::object> &templates) {
  BOOST_FOREACH(entity_template_map::value_type & kvp, *entity_templates) {
    fw::lua_context *ctx = kvp.second;
    luabind::object const &tmpl = luabind::globals(*ctx)["Entity"];

    luabind::object buildable_tmpl = tmpl["components"]["Buildable"];
    if (buildable_tmpl) {
      if (luabind::object_cast<std::string>(buildable_tmpl["BuildGroup"]) == build_group) {
        templates.push_back(tmpl);
      }
    }
  }
}

 // loads all of the *.entity files in the .\data\entities folder one by one, and
 // registers them in the entity_template_map
void entity_factory::load_entities() {
  entity_templates = new entity_template_map();

  fs::path base_path = fw::install_base_path() / "entities";
  fs::directory_iterator end_it;
  for (fs::directory_iterator it(base_path); it != end_it; ++it) {
    if (fs::is_regular_file(it->status()) && it->path().extension() == ".entity") {
      std::string tmpl_name = it->path().stem().string();

      fw::lua_context *ctx = new fw::lua_context();
      ctx->load_script(it->path());

      luabind::object tmpl = luabind::globals(*ctx)["Entity"];
      tmpl["name"] = tmpl_name;

      // TODO: loop through components and register their identifier(?)
      (*entity_templates)[tmpl_name] = ctx;
    }
  }
}

entity_component *entity_factory::create_component(std::string component_type_name) {
  std::function<entity_component *()> fn = (*comp_registry)[component_type_name];
  if (!fn) {
    fw::debug << boost::format("  warning: skipping unknown component \"%1%\"") % component_type_name << std::endl;
    return nullptr;
  }

  return fn();
}

//-------------------------------------------------------------------------
component_register::component_register(char const *name, std::function<entity_component *()> fn) {
  if (comp_registry == nullptr) {
    comp_registry = new std::map<std::string, std::function<entity_component *()>>();
  }

  (*comp_registry)[name] = fn;
}

}
