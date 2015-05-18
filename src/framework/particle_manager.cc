
#include <framework/particle_manager.h>
#include <framework/particle_emitter.h>
#include <framework/particle_effect.h>
#include <framework/particle_config.h>
#include <framework/particle.h>
#include <framework/particle_renderer.h>
#include <framework/framework.h>
#include <framework/timer.h>
#include <framework/scenegraph.h>

namespace fw {

particle_manager::particle_manager() :
    _renderer(nullptr), _graphics(nullptr), _wrap_x(0.0f), _wrap_z(0.0f) {
  _renderer = new particle_renderer(this);
}

particle_manager::~particle_manager() {
  delete _renderer;
}

void particle_manager::initialise(graphics *g) {
  _graphics = g;
  _renderer->initialize(g);
}

void particle_manager::set_world_wrap(float x, float z) {
  _wrap_x = x;
  _wrap_z = z;
}

void particle_manager::update(float dt) {
  for (std::vector<particle_effect *>::iterator dit = _dead_effects.begin(); dit != _dead_effects.end(); dit++) {
    particle_effect *effect = *dit;
    for (effect_list::iterator it = _effects.begin(); it != _effects.end(); it++) {
      if ((*it).get() == effect) {
        _effects.erase(it);
        break;
      }
    }
  }
  _dead_effects.clear();

  for (effect_list::iterator it = _effects.begin(); it != _effects.end(); ++it) {
    (*it)->update(dt);
  }
}

void particle_manager::render(sg::scenegraph &scenegraph) {
  _renderer->render(scenegraph, _particles);
}

std::shared_ptr<particle_effect> particle_manager::create_effect(std::string const &name) {
  std::shared_ptr<particle_effect_config> config = particle_effect_config::load(name);
  std::shared_ptr <particle_effect> effect(new particle_effect(this, config));
  effect->initialize();
  _effects.push_back(effect);
  return effect;
}

void particle_manager::remove_effect(particle_effect *effect) {
  _dead_effects.push_back(effect);
}

void particle_manager::add_particle(particle *p) {
  _particles.push_back(p);
}

void particle_manager::remove_particle(particle *p) {
  // this is not very efficient...
  particle_list::iterator it = std::find(_particles.begin(), _particles.end(), p);
  if (it != _particles.end()) {
    _particles.erase(it);
  }
}

}
