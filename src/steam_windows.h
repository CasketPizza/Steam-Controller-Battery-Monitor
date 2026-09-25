#pragma once

#include "settings.h"

#include <memory>

struct WindowCaptureResult {
  bool steamFound = false;
  bool friendsFound = false;
  WindowLayout steam;
  WindowLayout friends;
};

struct WindowRestoreResult {
  bool steamRestored = false;
  bool friendsRestored = false;
};

class SteamWindows {
 public:
  SteamWindows();
  ~SteamWindows();

  SteamWindows(const SteamWindows&) = delete;
  SteamWindows& operator=(const SteamWindows&) = delete;

  bool positionManagementAvailable() const;
  bool lifecycleObservationAvailable() const;
  bool bigPictureIsOpen() const;
  bool takeWindowChange();
  WindowCaptureResult captureWindowLayouts() const;
  WindowRestoreResult restoreWindowLayouts(const WindowLayout& steam, const WindowLayout& friends) const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl;
};
