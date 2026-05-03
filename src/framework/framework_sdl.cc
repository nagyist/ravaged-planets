#include <framework/framework.h>

// Make sure we won't include windows.h from SDL.h
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include <framework/input.h>
#include <framework/logging.h>
#include <framework/status.h>

namespace fw {

fw::Status Framework::InitializeSDL() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    return fw::ErrorStatus("error initalizing SDL: ") << SDL_GetError();
  }
  return fw::OkStatus();
}

bool Framework::poll_events() {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    input_->process_event(e);
    if (e.type == SDL_QUIT) {
      LOG(INFO) << "Got quit signal, exiting.";
      return false;
    }
  }
  return true;
}

bool Framework::wait_events() {
  SDL_Event e;
  while (SDL_WaitEvent(&e)) {
    input_->process_event(e);
    if (e.type == SDL_QUIT) {
      LOG(INFO) << "Got quit signal, exiting.";
      return false;
    }
  }
  return true;
}

}  // namespace fw