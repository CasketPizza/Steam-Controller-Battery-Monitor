#include "controller_activity.h"

#include <hidapi.h>
#include <chrono>
#include <vector>

namespace {

constexpr unsigned short VALVE_VENDOR_ID = 0x28DE;
constexpr unsigned short WIRED_CONTROLLER_PRODUCT_ID = 0x1302;
constexpr unsigned short WIRELESS_PUCK_PRODUCT_ID = 0x1304;
constexpr unsigned short RAW_HID_USAGE_PAGE = 0xFF00;
constexpr unsigned short CONTROLLER_STATE_USAGE = 0x0001;
constexpr unsigned short PUCK_STATUS_USAGE = 0x0002;
constexpr auto ACTIVITY_TIMEOUT = std::chrono::milliseconds(1500);

std::vector<hid_device*> openInterfaces(unsigned short productId) {
  hid_device_info* devices = hid_enumerate(VALVE_VENDOR_ID, productId);
  std::vector<hid_device*> result;

  for (hid_device_info* device = devices; device != nullptr; device = device->next) {
    const bool missingUsageMetadata = device->usage_page == 0 && device->usage == 0;
    if (device->usage_page != RAW_HID_USAGE_PAGE && !missingUsageMetadata) continue;
    // Some Linux HIDAPI backends cannot provide usage metadata and report zero for every raw HID collection.
    if (device->usage != CONTROLLER_STATE_USAGE && device->usage != PUCK_STATUS_USAGE && device->usage != 0) continue;
    hid_device* handle = hid_open_path(device->path);
    if (handle == nullptr) continue;
    hid_set_nonblocking(handle, 1);
    result.push_back(handle);
  }

  hid_free_enumeration(devices);
  return result;
}

std::vector<hid_device*> openControllerInterfaces() {
  std::vector<hid_device*> handles = openInterfaces(WIRED_CONTROLLER_PRODUCT_ID);
  std::vector<hid_device*> puckHandles = openInterfaces(WIRELESS_PUCK_PRODUCT_ID);
  handles.insert(handles.end(), puckHandles.begin(), puckHandles.end());
  return handles;
}

bool isControllerStateReport(unsigned char reportId) {
  return reportId == 0x42 || reportId == 0x45 || reportId == 0x47;
}

bool isWirelessStatusReport(unsigned char reportId) {
  return reportId == 0x46 || reportId == 0x79;
}

}  // namespace

ControllerActivityMonitor::ControllerActivityMonitor() : pollThread([this]() { pollLoop(); }) {}

ControllerActivityMonitor::~ControllerActivityMonitor() {
  running.store(false);
  if (pollThread.joinable()) pollThread.join();
}

uint64_t ControllerActivityMonitor::wakeGeneration() const {
  return generation.load();
}

void ControllerActivityMonitor::pollLoop() {
  std::vector<hid_device*> handles;
  bool active = false;
  auto lastActivity = std::chrono::steady_clock::now();
  auto ignoreStateUntil = std::chrono::steady_clock::time_point::min();

  while (running.load()) {
    if (handles.empty()) {
      handles = openControllerInterfaces();
      if (handles.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        continue;
      }
    }

    const auto now = std::chrono::steady_clock::now();
    if (active && now - lastActivity >= ACTIVITY_TIMEOUT) active = false;

    bool reopenInterfaces = false;
    for (hid_device* handle : handles) {
      bool readFailed = false;
      while (true) {
        unsigned char report[64] = {};
        const int bytesRead = hid_read(handle, report, sizeof(report));
        if (bytesRead < 0) {
          readFailed = true;
          break;
        }
        if (bytesRead == 0) break;

        bool activity = isControllerStateReport(report[0]) && now >= ignoreStateUntil;
        if (isWirelessStatusReport(report[0]) && bytesRead >= 2) {
          if (report[1] == 1) {
            active = false;
            ignoreStateUntil = now + std::chrono::milliseconds(500);
          }
          if (report[1] == 2) {
            activity = true;
            ignoreStateUntil = std::chrono::steady_clock::time_point::min();
          }
        }

        if (!activity) continue;
        lastActivity = now;
        if (!active) {
          active = true;
          generation.fetch_add(1);
        }
      }

      if (readFailed) {
        reopenInterfaces = true;
        break;
      }
    }

    if (reopenInterfaces) {
      for (hid_device* handle : handles) hid_close(handle);
      handles.clear();
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      active = false;
      continue;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  for (hid_device* handle : handles) hid_close(handle);
}
