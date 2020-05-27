#pragma once

#include <torch/csrc/autograd/profiler.h>

namespace torch {
namespace distributed {
namespace rpc {
namespace profiler {
namespace processglobal {

using namespace torch::autograd::profiler;

// Process global profiler state.
//
// This class holds information about a profiling range, from "enable" to
// "disable".
// An instance of this ``State`` will be
// pushed into a global stack, so nested profiling range is supported.
//
// It has 2 members.
// One is ``autograd::profiler::ProfilerConfig``. It's set by user and
// will be copied to thread-local profiler state of RPC threads.
// The other is a container that aggregates recorded
// ``autograd::profiler::Event``s from all thread-local profilers on RPC
// threads.
class State {
 public:
  explicit State(const ProfilerConfig& config) : config_(config) {}
  ~State() = default;

  const ProfilerConfig& config() const {
    return config_;
  }

  void pushProfileRange(thread_event_lists profileRange) {
    std::unique_lock<std::mutex> lock(profileRangesMutex_);

    // NB: When a thread wants to push an entry into the this container,
    // main control logic might have exited the process-global profile range.
    profileRanges_.emplace_back(std::move(profileRange));
  }

  std::vector<thread_event_lists> profileRanges() {
    std::unique_lock<std::mutex> lock(profileRangesMutex_);

    std::vector<thread_event_lists> results;
    results.swap(profileRanges_);
    return results;
  }

 private:
  // Name it profileRanges_ to emphasize on the fact that each element is the
  // results of a profile range. In each profile range, there is a
  // "__profiler_start" marker event that all following events calculate time
  // relative to it, so it's required to call
  // parse_cpu_trace(profileRange) for every profile range.
  std::mutex profileRangesMutex_;
  std::vector<thread_event_lists> profileRanges_;
  const ProfilerConfig config_ =
      ProfilerConfig(ProfilerState::Disabled, false, false);
};

// User-facing API.
//
// Enter a server-side process-global profiling range.
// Profiling range can be neste, so it's ok to call this API for multiple
// times. This enables all RPC threads running server-side request callbacks.
TORCH_API void enableServer(const ProfilerConfig& new_config);
//
// Exit a server-side process-global profiling range.
// Profiling range can be neste, so it's possible that profiler is still on
// after calling this API.
// This enables all RPC threads running server-side request callbacks.
TORCH_API std::vector<thread_event_lists> disableServer();

// Internal API.
//
// Return a bool indicating whether the server-side process-global profiling is
// on. All RPC threads running server-side request callbacks queries this.
TORCH_API bool serverEnabled();

// If server-side process-global profiling is on, use this API to get server
// process global profiler state to set thread-local profiling state.
TORCH_API std::shared_ptr<State> serverState();

} // namespace processglobal
} // namespace profiler
} // namespace rpc
} // namespace distributed
} // namespace torch
