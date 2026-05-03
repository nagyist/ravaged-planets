// Because cpptrace/from_current includes windows.h, we don't want that to pollute the rest of our
// stuff, so we include just this method in it's own file.

#include <framework/framework.h>

#include <thread>

#include <cpptrace/from_current.hpp>
#include <cpptrace/exceptions_macros.hpp>

#include <framework/logging.h>

namespace fw {

namespace {

static std::thread::id g_update_thread_id;

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

void Framework::update_proc() {
  g_update_thread_id = std::this_thread::get_id();

  CPPTRACE_TRY{
    update_proc_impl();
  } CPPTRACE_CATCH(std::exception const& e) {
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

} // namespace