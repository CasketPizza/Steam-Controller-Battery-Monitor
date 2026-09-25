#pragma once

#include <atomic>
#include <cstdint>
#include <thread>

class ControllerActivityMonitor {
 public:
  ControllerActivityMonitor();
  ~ControllerActivityMonitor();

  ControllerActivityMonitor(const ControllerActivityMonitor&) = delete;
  ControllerActivityMonitor& operator=(const ControllerActivityMonitor&) = delete;

  uint64_t wakeGeneration() const;

 private:
  void pollLoop();

  std::atomic_bool running{true};
  std::atomic<uint64_t> generation{0};
  std::thread pollThread;
};
