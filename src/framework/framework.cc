#include <chrono>
#include <functional>
#include <iostream>
#include <stdint.h>
#include <thread>
#include <vector>

#include <absl/strings/match.h>
#include <cpptrace/from_current.hpp>
#include <cpptrace/exceptions_macros.hpp>
#include <SDL2/SDL.h>

#include <framework/framework.h>
#include <framework/audio.h>
#include <framework/logging.h>
#include <framework/camera.h>
#include <framework/bitmap.h>
#include <framework/settings.h>
#include <framework/graphics.h>
#include <framework/cursor.h>
#include <framework/debug_view.h>
#include <framework/font.h>
#include <framework/particle_manager.h>
#include <framework/model_manager.h>
#include <framework/scenegraph.h>
#include <framework/net.h>
#include <framework/http.h>
#include <framework/timer.h>
#include <framework/texture.h>
#include <framework/lang.h>
#include <framework/misc.h>
#include <framework/input.h>
#include <framework/gui/gui.h>

using namespace std::placeholders;

namespace fw {

namespace {

static std::thread::id g_update_thread_id;

}

struct ScreenshotRequest {
  int width;
  int height;
  bool include_gui;
  std::function<void(fw::Bitmap const &bmp)> callback;
  fw::Bitmap bitmap;
};

static Framework *only_instance = 0;

Framework::Framework(BaseApp* app) :
    app_(app), active_(true), camera_(nullptr), paused_(false), particle_mgr_(nullptr),
    timer_(nullptr), audio_manager_(nullptr), input_(nullptr), lang_(nullptr),
     model_manager_(nullptr), cursor_(nullptr),
    debug_view_(nullptr), scenegraph_manager_(nullptr), running_(true) {
  only_instance = this;
}

Framework::~Framework() {
  if (lang_ != nullptr)
    delete lang_;
  if (input_ != nullptr)
    delete input_;
  if (timer_ != nullptr)
    delete timer_;
  if (particle_mgr_ != nullptr)
    delete particle_mgr_;
  if (cursor_ != nullptr)
    delete cursor_;
  if (model_manager_ != nullptr)
    delete model_manager_;
  if (scenegraph_manager_ != nullptr)
    delete scenegraph_manager_;
  if (debug_view_ != nullptr)
    delete debug_view_;
  if (audio_manager_ != nullptr)
    delete audio_manager_;
}

Framework *Framework::get_instance() {
  return only_instance;
}

fw::StatusOr<bool> Framework::initialize(char const *title) {
  if (Settings::get<bool>("help")) {
    Settings::print_help();
    return false;
  }

  random_initialize();
  RETURN_IF_ERROR(LogInitialize());
  language_initialize();

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    return fw::ErrorStatus("error initalizing SDL: ") << SDL_GetError();
  }

  timer_ = new Timer();

  // initialize graphics
  if (app_->wants_graphics()) {
    RETURN_IF_ERROR(fw::Get<Graphics>().initialize(title));

    model_manager_ = new ModelManager();
    scenegraph_manager_ = new sg::ScenegraphManager();

    particle_mgr_ = new ParticleManager();
    RETURN_IF_ERROR(particle_mgr_->Initialize());

    cursor_ = new Cursor();
    cursor_->initialize();
  }

  // initialise audio
  audio_manager_ = new AudioManager();
  audio_manager_->initialize();

  input_ = new Input();
  input_->initialize();

  RETURN_IF_ERROR(fw::Get<FontManager>().initialize());

  if (app_->wants_graphics()) {
    RETURN_IF_ERROR(fw::Get<gui::Gui>().Initialize(audio_manager_));
  }

  if (Settings::get<bool>("debug-view") && app_->wants_graphics()) {
    debug_view_ = new DebugView();
    debug_view_->initialize();
  }

  RETURN_IF_ERROR(net::initialize());
  Http::initialize();

  // start the game Timer that'll record fps, elapsed time, etc.
  timer_->start();

  input_->bind_function("toggle-fullscreen", std::bind(&Framework::on_fullscreen_toggle, this, _1, _2));

  return true;
}

void Framework::on_fullscreen_toggle(std::string keyname, bool is_down) {
  if (!is_down) {
    fw::Get<Graphics>().toggle_fullscreen();
  }
}

void Framework::language_initialize() {
  const std::vector<LangDescription> langs = fw::get_languages();
  LOG(INFO) << langs.size() << " installed language(s):";
  for(const LangDescription &l : langs) {
    LOG(INFO) << "  " << l.name << " (" << l.display_name << ")";
  }

  std::string lang_name = Settings::get<std::string>("lang");
  if (!absl::EndsWithIgnoreCase(lang_name, ".lang")) {
    lang_name += ".lang";
  }

  lang_ = new Lang(lang_name);
}

void Framework::destroy() {
  app_->destroy();

  LOG(INFO) << "framework is shutting down...";
  if (debug_view_ != nullptr) {
    debug_view_->destroy();
  }

	fw::Get<Graphics>().destroy();

  Http::destroy();
  net::destroy();
  if (cursor_ != nullptr) {
    cursor_->destroy();
  }
  audio_manager_->destroy();
}

void Framework::deactivate() {
  active_ = false;
}

void Framework::reactivate() {
  active_ = true;
}

void Framework::run() {
  // kick off the update thread
  std::thread update_thread(std::bind(&Framework::update_proc, this));
  try {
		if (app_->wants_graphics()) {
      // do the event/render loop
      while (running_) {
        if (!poll_events()) {
          running_ = false;
          break;
        }

        render();
      }
    } else {
      wait_events();
      running_ = false;
    }

    // wait for the update thread to exit (once we set running_ to false, it'll stop as well)
    update_thread.join();
  } catch(...) {
    update_thread.detach();
    throw;
  }
}

void Framework::exit() {
  LOG(INFO) << "Exiting.";
  running_ = false;
}

void Framework::set_camera(Camera *cam) {
  if (camera_ != nullptr)
    camera_->disable();

  camera_ = cam;

  if (camera_ != nullptr)
    camera_->enable();
}

void Framework::update_proc_impl() {
  LOG(INFO) << "framework initialization complete, application initialization starting...";
  auto status = app_->initialize(this);
  if (!status.ok()) {
    LOG(ERR) << "app did not iniatilize: " << status;
    return;
  }

  LOG(INFO) << "application initialization complete, running...";

  int64_t accum_micros = 0;
  int64_t timestep_micros = 1000000 / 40; // 40 frames per second update frequency.
  while (running_) {
    timer_->update();
    accum_micros += std::chrono::duration_cast<std::chrono::microseconds>(
        timer_->get_frame_duration()).count();

    int64_t remaining_micros = timestep_micros - accum_micros;
    if (remaining_micros > 1000) {
      std::this_thread::sleep_for(std::chrono::microseconds(remaining_micros));
      continue;
    }

    while (accum_micros > timestep_micros && running_) {
      float dt = static_cast<float>(timestep_micros) / 1000000.f;
      update(dt);
      accum_micros -= timestep_micros;
    }

    // TODO: should we yield or sleep for a while?
    std::this_thread::yield();
  }
}

void Framework::update_proc() {
  g_update_thread_id = std::this_thread::get_id();

  CPPTRACE_TRY {
    update_proc_impl();
  } CPPTRACE_CATCH(std::exception const &e) {
    LOG(ERR) << "--------------------------------------------------------------------------------";
    LOG(ERR) << "UNHANDLED EXCEPTION!";
    LOG(ERR) << e.what();
    LOG(ERR) << cpptrace::from_current_exception().to_string();
    throw;
  }/* CPPTRACE_CATCH (...) {
    LOG(ERR) << "--------------------------------------------------------------------------------";
    LOG(ERR) << "UNHANDLED EXCEPTION! (unknown exception)";
    LOG(ERR) << cpptrace::from_current_exception().to_string();
    throw;
  }*/
}

void Framework::update(float dt) {
  fw::Get<gui::Gui>().update(dt);
  fw::Get<FontManager>().update(dt);
  audio_manager_->update(dt);
  if (!paused_) {
    app_->update(dt);
  }
  particle_mgr_->update(dt);
  if (debug_view_ != nullptr) {
    debug_view_->update(dt);
  }

  if (camera_ != nullptr)
    camera_->update(dt);

  input_->update(dt);
}

/* static */
void Framework::ensure_update_thread() {
  std::thread::id this_thread_id = std::this_thread::get_id();
  if (this_thread_id != g_update_thread_id) {
    LOG(ERR) << fw::ErrorStatus("expected to be running on the update thread.");
    // TODO: something else?
    std::terminate();
  }
}

void Framework::render() {
  Camera* cam = fw::Framework::get_instance()->get_camera();
  if (!app_->wants_graphics() || cam == nullptr) {
    return;
  }

  timer_->render();

  scenegraph_manager_->before_render();
  
  auto& scenegraph = scenegraph_manager_->get_scenegraph();
  scenegraph.push_camera(cam->get_render_state());
  fw::render(scenegraph);
  scenegraph.pop_camera();

  // if we've been asked for some screenshots, take them after we've done the normal render.
  if (screenshot_requests_.size() > 0) {
    take_screenshots(scenegraph);
  }

  fw::Get<Graphics>().after_render();
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

void Framework::take_screenshot(
    int width, int height, std::function<void(fw::Bitmap const &bmp)> callback_fn,
    bool include_gui /*= true */) {
  if (width == 0 || height == 0) {
    auto &graphics = fw::Get<Graphics>();
    width = graphics.get_width();
    height = graphics.get_height();
  }

  auto request = std::make_shared<ScreenshotRequest>();
  request->width = width;
  request->height = height;
  request->include_gui = include_gui;
  request->callback = callback_fn;

  screenshot_requests_.push_back(request);
}

/**
 * This is called on another thread to call the callbacks that were registered against the
 * "screenshot" request.
 */
void call_callacks_thread_proc(
    std::shared_ptr<std::list<std::shared_ptr<ScreenshotRequest>>> requests) {
  for(auto request : *requests) {
    request->callback(request->bitmap);
  }
}

/**
 * This is called at the end of a frame if there are pending screenshot callbacks. We'll grab the
 * contents of the, frame buffer, then (on another thread) call the callbacks.
 */
void Framework::take_screenshots(sg::Scenegraph &scenegraph) {
  if (screenshot_requests_.empty()) {
    return;
  }

  auto requests = std::make_shared<std::list<std::shared_ptr<ScreenshotRequest>>>();
  for(auto request : screenshot_requests_) {
    // render the scene to a separate render target first
    std::shared_ptr<fw::Texture> color_target(new fw::Texture());
    color_target->create(request->width, request->height);

    std::shared_ptr<fw::Texture> depth_target(new fw::Texture());
    depth_target->create_depth(request->width, request->height);

    std::shared_ptr<fw::Framebuffer> framebuffer(new fw::Framebuffer());
    framebuffer->set_color_buffer(color_target);
    framebuffer->set_depth_buffer(depth_target);

    scenegraph.push_camera(fw::Framework::get_instance()->get_camera()->get_render_state());
    fw::render(scenegraph, framebuffer, request->include_gui);
    scenegraph.pop_camera();

    auto bitmap = load_bitmap(*color_target.get());
    request->bitmap = load_bitmap(*color_target.get());
    requests->push_back(request);
  }

  // create a thread to finish the job
  std::thread t(std::bind(&call_callacks_thread_proc, requests));
  t.detach();

  screenshot_requests_.clear();
}

}
