#pragma once

#include <functional>
#include <mutex>

#include <GL/glew.h>

#include <framework/color.h>
#include <framework/logging.h>
#include <framework/signals.h>
#include <framework/status.h>

typedef void *SDL_GLContext;
struct SDL_Window;

namespace fw {
class Framebuffer;

#if defined(DEBUG)
#define FW_ENSURE_RENDER_THREAD() \
  fw::Graphics::ensure_render_thread()
#else
#define FW_ENSURE_RENDER_THREAD()
#endif

class Graphics {
public:
	static std::string service_name;

private:
  SDL_Window *wnd_;
  SDL_GLContext context_;
  std::mutex run_queue_mutex_;
  std::vector<std::function<void()>> run_queue_;
  std::shared_ptr<fw::Framebuffer> framebuffer_;

  int width_;
  int height_;
  bool windowed_;

public:
  Graphics();
  ~Graphics();

  fw::Status initialize(char const *title);
  void destroy();

  void begin_scene(fw::Color clear_color = fw::Color(1, 0, 0, 0));
  void end_scene();
  void present();
  void after_render();

  // toggle between windowed and fullscreen mode.
  void toggle_fullscreen();

  // These are called by the CEGUI system just before & just after it renders.
  void before_gui();
  void after_gui();

  // Sets the render target to the given framebuffer.
  void set_render_target(std::shared_ptr<fw::Framebuffer> fb);

  inline int get_width() { return width_; }
  inline int get_height() { return height_; }

  // This signal is fired just before the graphics present().
  fw::Signal<> sig_before_present;

  /**
   * Schedules the given function to run on the render thread, just after the scene has finished
   * drawing.
   */
  void run_on_render_thread(std::function<void()> fn);

  // For things that must run on the render thread, this can be sure to enforce it.
  static bool is_render_thread();
  static void ensure_render_thread();
};

/** An index buffer holds lists of indices into a vertex_buffer. */
class IndexBuffer {
private:
  GLuint id_;
  int num_indices_;
  bool dynamic_;

public:
  IndexBuffer(bool dynamic = false);
  IndexBuffer(const IndexBuffer&) = delete;
  ~IndexBuffer();

  static std::shared_ptr<IndexBuffer> create();

  void set_data(int num_indices, uint16_t const *indices, int flags = -1);

  inline int get_num_indices() const {
    return num_indices_;
  }

  // Called just before and just after we're going to render with this index buffer.
  void begin();
  void end();
};

/** A wrapper around a vertex buffer. */
class VertexBuffer {
public:
  typedef std::function<void()> setup_fn;

private:
  GLuint id_;
  int num_vertices_;
  size_t vertex_size_;
  bool dynamic_;

  setup_fn setup_;

public:
  VertexBuffer(setup_fn setup, size_t vertex_size, bool dynamic = false);
  VertexBuffer(const VertexBuffer&) = delete;
  virtual ~VertexBuffer();

  // Helper function that makes it easier to create vertex buffers by assuming that you're passing a
  // type defined in fw::vertex::xxx (or something compatible).
  template<typename T>
  static inline std::shared_ptr<VertexBuffer> create(bool dynamic = false) {
    return std::shared_ptr<VertexBuffer>(new VertexBuffer(
        T::get_setup_function(), sizeof(T), dynamic));
  }

  void set_data(int num_vertices, const void *vertices, int flags = -1);

  inline int get_num_vertices() const {
    return num_vertices_;
  }

  void begin();
  void end();
};

namespace vertex {

struct xyz {
  inline xyz() :
      x(0), y(0), z(0) {
  }

  inline xyz(float x, float y, float z) :
      x(x), y(y), z(z) {
  }

  inline xyz(xyz const &copy) :
      x(copy.x), y(copy.y), z(copy.z) {
  }

  float x, y, z;

  // returns a function that'll set up a vertex buffer (i.e. with calls to glXyzPointer)
  static std::function<void()> get_setup_function();
};

struct xyz_c {
  inline xyz_c() :
      x(0), y(0), z(0), color(0) {
  }

  inline xyz_c(float x, float y, float z, uint32_t color) :
      x(x), y(y), z(z), color(color) {
  }

  inline xyz_c(float x, float y, float z, fw::Color const &color) :
      x(x), y(y), z(z), color(color.to_abgr()) {
  }

  inline xyz_c(xyz_c const &copy) :
      x(copy.x), y(copy.y), z(copy.z), color(copy.color) {
  }

  float x, y, z;
  uint32_t color;

  // returns a function that'll set up a vertex buffer (i.e. with calls to glXyzPointer)
  static std::function<void()> get_setup_function();
};

struct xyz_uv {
  inline xyz_uv() :
      x(0), y(0), z(0), u(0), v(0) {
  }

  inline xyz_uv(float x, float y, float z, float u, float v) :
      x(x), y(y), z(z), u(u), v(v) {
  }

  inline xyz_uv(xyz_uv const &copy) :
      x(copy.x), y(copy.y), z(copy.z), u(copy.u), v(copy.v) {
  }

  float x, y, z;
  float u, v;

  // returns a function that'll set up a vertex buffer (i.e. with calls to glXyzPointer)
  static std::function<void()> get_setup_function();
};

struct xyz_c_uv {
  inline xyz_c_uv() :
      x(0), y(0), z(0), color(0), u(0), v(0) {
  }

  inline xyz_c_uv(float x, float y, float z, uint32_t color, float u, float v) :
      x(x), y(y), z(z), color(color), u(u), v(v) {
  }

  inline xyz_c_uv(xyz_c_uv const &copy) :
      x(copy.x), y(copy.y), z(copy.z), color(copy.color), u(copy.u), v(copy.v) {
  }

  float x, y, z;
  uint32_t color;
  float u, v;

  // returns a function that'll set up a vertex buffer (i.e. with calls to glXyzPointer)
  static std::function<void()> get_setup_function();
};

struct xyz_n_uv {
  inline xyz_n_uv() :
      x(0), y(0), z(0), nx(0), ny(0), nz(0), u(0), v(0) {
  }

  inline xyz_n_uv(float x, float y, float z, float nx, float ny, float nz,
      float u, float v) :
      x(x), y(y), z(z), nx(nx), ny(ny), nz(nz), u(u), v(v) {
  }

  inline xyz_n_uv(xyz_n_uv const &copy) :
      x(copy.x), y(copy.y), z(copy.z), nx(copy.nx), ny(copy.ny), nz(copy.nz), u(copy.u), v(copy.v) {
  }

  float x, y, z;
  float nx, ny, nz;
  float u, v;

  // returns a function that'll set up a vertex buffer (i.e. with calls to glXyzPointer)
  static std::function<void()> get_setup_function();
};

struct xyz_n {
  inline xyz_n() :
      x(0), y(0), z(0), nx(0), ny(0), nz(0) {
  }

  inline xyz_n(float x, float y, float z, float nx, float ny, float nz) :
      x(x), y(y), z(z), nx(nx), ny(ny), nz(nz) {
  }

  inline xyz_n(xyz_n const &copy) :
      x(copy.x), y(copy.y), z(copy.z), nx(copy.nx), ny(copy.ny), nz(copy.nz) {
  }

  float x, y, z;
  float nx, ny, nz;

  // returns a function that'll set up a vertex buffer (i.e. with calls to glXyzPointer)
  static std::function<void()> get_setup_function();
};

}
}
