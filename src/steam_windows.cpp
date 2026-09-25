#include "steam_windows.h"

#include <QGuiApplication>
#include <QMargins>
#include <QScreen>
#include <QString>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {

bool isSteamProcess(const QString& processName) {
  const QString name = processName.toLower();
  return name == QStringLiteral("steam") || name == QStringLiteral("steam.exe") ||
         name == QStringLiteral("steamwebhelper") || name == QStringLiteral("steamwebhelper.exe");
}

bool isBigPictureTitle(const QString& title) {
  const QString value = title.toLower();
  return value.contains(QStringLiteral("big picture")) || value.contains(QStringLiteral("gamepadui"));
}

bool isFriendsTitle(const QString& title) {
  const QString value = title.trimmed().toLower();
  return value == QStringLiteral("friends") || value.contains(QStringLiteral("friends list"));
}

bool isDesktopTitle(const QString& title) {
  return title.trimmed().compare(QStringLiteral("Steam"), Qt::CaseInsensitive) == 0;
}

QRect clampToAvailableGeometry(const QRect& relativeGeometry, const QRect& availableGeometry) {
  const int width = std::clamp(relativeGeometry.width(), 200, std::max(200, availableGeometry.width()));
  const int height = std::clamp(relativeGeometry.height(), 120, std::max(120, availableGeometry.height()));
  const int left = availableGeometry.left() + relativeGeometry.x();
  const int top = availableGeometry.top() + relativeGeometry.y();
  const int x = std::clamp(left, availableGeometry.left(), availableGeometry.right() - width + 1);
  const int y = std::clamp(top, availableGeometry.top(), availableGeometry.bottom() - height + 1);
  return QRect(x, y, width, height);
}

}  // namespace

#ifdef _WIN32

#include <windows.h>

namespace {

struct NativeWindow {
  HWND handle = nullptr;
  QString processName;
  QString title;
  bool visible = false;
};

QString processNameForWindow(HWND window) {
  DWORD processId = 0;
  GetWindowThreadProcessId(window, &processId);
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
  if (process == nullptr) return {};

  wchar_t path[32768] = {};
  DWORD length = 32768;
  QString result;
  if (QueryFullProcessImageNameW(process, 0, path, &length)) {
    const QString fullPath = QString::fromWCharArray(path, static_cast<qsizetype>(length));
    result = fullPath.mid(fullPath.lastIndexOf(u'\\') + 1);
  }
  CloseHandle(process);
  return result;
}

QString windowTitle(HWND window) {
  const int length = GetWindowTextLengthW(window);
  if (length <= 0) return {};
  std::vector<wchar_t> text(static_cast<size_t>(length) + 1);
  GetWindowTextW(window, text.data(), static_cast<int>(text.size()));
  return QString::fromWCharArray(text.data());
}

BOOL CALLBACK collectWindows(HWND window, LPARAM parameter) {
  auto* windows = reinterpret_cast<std::vector<NativeWindow>*>(parameter);
  const QString processName = processNameForWindow(window);
  if (!isSteamProcess(processName)) return TRUE;

  windows->push_back({window, processName, windowTitle(window), IsWindowVisible(window) != FALSE});
  return TRUE;
}

std::vector<NativeWindow> steamWindows() {
  std::vector<NativeWindow> windows;
  EnumWindows(collectWindows, reinterpret_cast<LPARAM>(&windows));
  return windows;
}

HWND findWindow(const std::vector<NativeWindow>& windows, bool (*matches)(const QString&), bool visibleOnly) {
  for (const NativeWindow& window : windows) {
    if ((!visibleOnly || window.visible) && matches(window.title)) return window.handle;
  }
  return nullptr;
}

MONITORINFOEXW monitorInfo(HMONITOR monitor) {
  MONITORINFOEXW info{};
  info.cbSize = sizeof(info);
  GetMonitorInfoW(monitor, &info);
  return info;
}

POINT workspaceOffset() {
  RECT workArea{};
  if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0)) return POINT{0, 0};
  return POINT{workArea.left, workArea.top};
}

BOOL CALLBACK findMonitorByName(HMONITOR monitor, HDC, LPRECT, LPARAM parameter) {
  auto* result = reinterpret_cast<std::pair<QString, HMONITOR>*>(parameter);
  const MONITORINFOEXW info = monitorInfo(monitor);
  if (QString::fromWCharArray(info.szDevice).compare(result->first, Qt::CaseInsensitive) == 0) {
    result->second = monitor;
    return FALSE;
  }
  return TRUE;
}

WindowLayout captureLayout(HWND window) {
  WindowLayout layout;
  if (IsIconic(window)) return layout;
  WINDOWPLACEMENT placement{};
  placement.length = sizeof(placement);
  if (!GetWindowPlacement(window, &placement)) return layout;

  RECT rect{};
  if (placement.showCmd == SW_SHOWMAXIMIZED) {
    rect = placement.rcNormalPosition;
    const POINT offset = workspaceOffset();
    OffsetRect(&rect, offset.x, offset.y);
  } else if (!GetWindowRect(window, &rect)) {
    return layout;
  }

  const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
  const MONITORINFOEXW info = monitorInfo(monitor);
  layout.monitorId = QString::fromWCharArray(info.szDevice);
  layout.geometry = QRect(rect.left - info.rcWork.left, rect.top - info.rcWork.top, rect.right - rect.left,
                          rect.bottom - rect.top);
  layout.maximized = placement.showCmd == SW_SHOWMAXIMIZED;
  layout.valid = layout.geometry.isValid();
  return layout;
}

bool restoreLayout(HWND window, const WindowLayout& layout) {
  if (window == nullptr || !layout.valid) return false;

  std::pair<QString, HMONITOR> monitorSearch{layout.monitorId, nullptr};
  EnumDisplayMonitors(nullptr, nullptr, findMonitorByName, reinterpret_cast<LPARAM>(&monitorSearch));
  HMONITOR monitor = monitorSearch.second;
  if (monitor == nullptr) monitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);

  const MONITORINFOEXW info = monitorInfo(monitor);
  const QRect available(info.rcWork.left, info.rcWork.top, info.rcWork.right - info.rcWork.left,
                        info.rcWork.bottom - info.rcWork.top);
  const QRect target = clampToAvailableGeometry(layout.geometry, available);
  const POINT offset = workspaceOffset();
  WINDOWPLACEMENT placement{};
  placement.length = sizeof(placement);
  if (!GetWindowPlacement(window, &placement)) return false;
  placement.flags = 0;
  placement.showCmd = layout.maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
  placement.rcNormalPosition = RECT{target.left() - offset.x, target.top() - offset.y,
                                    target.right() + 1 - offset.x, target.bottom() + 1 - offset.y};
  return SetWindowPlacement(window, &placement) != FALSE;
}

std::atomic_bool* activeWindowChangeFlag = nullptr;

void CALLBACK windowEventCallback(HWINEVENTHOOK, DWORD, HWND, LONG objectId, LONG childId, DWORD, DWORD) {
  if (objectId == OBJID_WINDOW && childId == CHILDID_SELF && activeWindowChangeFlag != nullptr) {
    activeWindowChangeFlag->store(true);
  }
}

}  // namespace

class SteamWindows::Impl {
 public:
  Impl() {
    activeWindowChangeFlag = &windowChanged;
    hook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_NAMECHANGE, nullptr, windowEventCallback, 0, 0,
                           WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
  }

  ~Impl() {
    if (hook != nullptr) UnhookWinEvent(hook);
    if (activeWindowChangeFlag == &windowChanged) activeWindowChangeFlag = nullptr;
  }

  std::atomic_bool windowChanged{true};
  HWINEVENTHOOK hook = nullptr;
};

SteamWindows::SteamWindows() : impl(std::make_unique<Impl>()) {}
SteamWindows::~SteamWindows() = default;

bool SteamWindows::positionManagementAvailable() const {
  return true;
}

bool SteamWindows::lifecycleObservationAvailable() const {
  return true;
}

bool SteamWindows::bigPictureIsOpen() const {
  const auto windows = steamWindows();
  return findWindow(windows, isBigPictureTitle, false) != nullptr;
}

bool SteamWindows::takeWindowChange() {
  return impl->windowChanged.exchange(false);
}

WindowCaptureResult SteamWindows::captureWindowLayouts() const {
  WindowCaptureResult result;
  const auto windows = steamWindows();
  const HWND steam = findWindow(windows, isDesktopTitle, true);
  const HWND friends = findWindow(windows, isFriendsTitle, true);
  if (steam != nullptr) {
    result.steam = captureLayout(steam);
    result.steamFound = result.steam.valid;
  }
  if (friends != nullptr) {
    result.friends = captureLayout(friends);
    result.friendsFound = result.friends.valid;
  }
  return result;
}

WindowRestoreResult SteamWindows::restoreWindowLayouts(const WindowLayout& steamLayout,
                                                       const WindowLayout& friendsLayout) const {
  WindowRestoreResult result;
  const auto windows = steamWindows();
  result.steamRestored = restoreLayout(findWindow(windows, isDesktopTitle, false), steamLayout);
  result.friendsRestored = restoreLayout(findWindow(windows, isFriendsTitle, false), friendsLayout);
  return result;
}

#elif defined(__linux__)

#include <QFileInfo>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace {

struct NativeWindow {
  Window handle = 0;
  QString processName;
  QString title;
  bool visible = false;
};

QByteArray windowProperty(Display* display, Window window, Atom property, Atom requestedType) {
  Atom actualType = None;
  int format = 0;
  unsigned long itemCount = 0;
  unsigned long bytesRemaining = 0;
  unsigned char* data = nullptr;
  if (XGetWindowProperty(display, window, property, 0, 4096, False, requestedType, &actualType, &format, &itemCount,
                         &bytesRemaining, &data) != Success ||
      data == nullptr) {
    return {};
  }
  const int byteCount = format == 32 ? static_cast<int>(itemCount * sizeof(long))
                                     : static_cast<int>(itemCount * static_cast<unsigned long>(format / 8));
  const QByteArray result(reinterpret_cast<const char*>(data), byteCount);
  XFree(data);
  return result;
}

QString titleForWindow(Display* display, Window window) {
  const Atom utf8 = XInternAtom(display, "UTF8_STRING", False);
  const Atom netName = XInternAtom(display, "_NET_WM_NAME", False);
  const QByteArray name = windowProperty(display, window, netName, utf8);
  if (!name.isEmpty()) return QString::fromUtf8(name.constData(), name.size());

  char* legacyName = nullptr;
  if (XFetchName(display, window, &legacyName) && legacyName != nullptr) {
    const QString result = QString::fromLocal8Bit(legacyName);
    XFree(legacyName);
    return result;
  }
  return {};
}

unsigned long processIdForWindow(Display* display, Window window) {
  const Atom pidAtom = XInternAtom(display, "_NET_WM_PID", False);
  const QByteArray value = windowProperty(display, window, pidAtom, XA_CARDINAL);
  if (value.size() < static_cast<int>(sizeof(unsigned long))) return 0;
  return *reinterpret_cast<const unsigned long*>(value.constData());
}

QString processNameForWindow(Display* display, Window window) {
  const unsigned long processId = processIdForWindow(display, window);
  if (processId == 0) return {};
  return QFileInfo(QStringLiteral("/proc/%1/exe").arg(processId)).symLinkTarget().section(u'/', -1);
}

std::vector<NativeWindow> steamWindows(Display* display) {
  std::vector<NativeWindow> result;
  const Window root = DefaultRootWindow(display);
  const Atom clientList = XInternAtom(display, "_NET_CLIENT_LIST", False);
  const QByteArray data = windowProperty(display, root, clientList, XA_WINDOW);
  const auto* windows = reinterpret_cast<const Window*>(data.constData());
  const int count = data.size() / static_cast<int>(sizeof(Window));
  for (int index = 0; index < count; ++index) {
    const QString processName = processNameForWindow(display, windows[index]);
    if (!isSteamProcess(processName)) continue;
    XWindowAttributes attributes{};
    XGetWindowAttributes(display, windows[index], &attributes);
    result.push_back({windows[index], processName, titleForWindow(display, windows[index]),
                      attributes.map_state == IsViewable});
  }
  return result;
}

Window findWindow(const std::vector<NativeWindow>& windows, bool (*matches)(const QString&), bool visibleOnly) {
  for (const NativeWindow& window : windows) {
    if ((!visibleOnly || window.visible) && matches(window.title)) return window.handle;
  }
  return 0;
}

QMargins frameExtents(Display* display, Window window) {
  const Atom property = XInternAtom(display, "_NET_FRAME_EXTENTS", False);
  const QByteArray data = windowProperty(display, window, property, XA_CARDINAL);
  if (data.size() < static_cast<int>(sizeof(long) * 4)) return {};
  const auto* values = reinterpret_cast<const long*>(data.constData());
  return QMargins(static_cast<int>(values[0]), static_cast<int>(values[2]), static_cast<int>(values[1]),
                  static_cast<int>(values[3]));
}

QRect geometryForWindow(Display* display, Window window) {
  XWindowAttributes attributes{};
  if (!XGetWindowAttributes(display, window, &attributes)) return {};
  Window child = 0;
  int rootX = 0;
  int rootY = 0;
  XTranslateCoordinates(display, window, DefaultRootWindow(display), 0, 0, &rootX, &rootY, &child);
  const QMargins frame = frameExtents(display, window);
  return QRect(rootX - frame.left(), rootY - frame.top(), attributes.width + frame.left() + frame.right(),
               attributes.height + frame.top() + frame.bottom());
}

bool windowIsMaximized(Display* display, Window window) {
  const Atom state = XInternAtom(display, "_NET_WM_STATE", False);
  const Atom horizontal = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
  const Atom vertical = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
  const QByteArray data = windowProperty(display, window, state, XA_ATOM);
  const auto* atoms = reinterpret_cast<const Atom*>(data.constData());
  const int count = data.size() / static_cast<int>(sizeof(Atom));
  bool hasHorizontal = false;
  bool hasVertical = false;
  for (int index = 0; index < count; ++index) {
    hasHorizontal = hasHorizontal || atoms[index] == horizontal;
    hasVertical = hasVertical || atoms[index] == vertical;
  }
  return hasHorizontal && hasVertical;
}

QRect toNativeGeometry(QScreen* screen, const QRect& geometry) {
  const qreal scale = screen->devicePixelRatio();
  return QRect(qRound(geometry.x() * scale), qRound(geometry.y() * scale), qRound(geometry.width() * scale),
               qRound(geometry.height() * scale));
}

QScreen* screenForNativeGeometry(const QRect& geometry) {
  for (QScreen* screen : QGuiApplication::screens()) {
    if (toNativeGeometry(screen, screen->geometry()).contains(geometry.center())) return screen;
  }
  return QGuiApplication::primaryScreen();
}

void setMaximized(Display* display, Window window, bool maximized);

WindowLayout captureLayout(Display* display, Window window) {
  WindowLayout layout;
  const bool maximized = windowIsMaximized(display, window);
  if (maximized) {
    setMaximized(display, window, false);
    XFlush(display);
    for (int attempt = 0; attempt < 20 && windowIsMaximized(display, window); ++attempt) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      XSync(display, False);
    }
    if (windowIsMaximized(display, window)) {
      setMaximized(display, window, true);
      XFlush(display);
      return layout;
    }
  }

  const QRect geometry = geometryForWindow(display, window);
  QScreen* screen = screenForNativeGeometry(geometry);
  if (screen == nullptr || !geometry.isValid()) {
    if (maximized) setMaximized(display, window, true);
    return layout;
  }
  const QRect available = screen->availableGeometry();
  const QRect nativeAvailable = toNativeGeometry(screen, available);
  const qreal scale = screen->devicePixelRatio();
  layout.monitorId = screen->name();
  layout.geometry = QRect(qRound((geometry.x() - nativeAvailable.x()) / scale),
                          qRound((geometry.y() - nativeAvailable.y()) / scale), qRound(geometry.width() / scale),
                          qRound(geometry.height() / scale));
  layout.maximized = maximized;
  layout.valid = true;
  if (maximized) {
    setMaximized(display, window, true);
    XFlush(display);
  }
  return layout;
}

void setMaximized(Display* display, Window window, bool maximized) {
  XEvent event{};
  event.xclient.type = ClientMessage;
  event.xclient.window = window;
  event.xclient.message_type = XInternAtom(display, "_NET_WM_STATE", False);
  event.xclient.format = 32;
  event.xclient.data.l[0] = maximized ? 1 : 0;
  event.xclient.data.l[1] = static_cast<long>(XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False));
  event.xclient.data.l[2] = static_cast<long>(XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False));
  XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
}

bool moveResizeWindow(Display* display, Window window, const QRect& target) {
  XEvent event{};
  event.xclient.type = ClientMessage;
  event.xclient.window = window;
  event.xclient.message_type = XInternAtom(display, "_NET_MOVERESIZE_WINDOW", False);
  event.xclient.format = 32;
  event.xclient.data.l[0] = StaticGravity | (0x0F << 8) | (1 << 12);
  event.xclient.data.l[1] = target.x();
  event.xclient.data.l[2] = target.y();
  event.xclient.data.l[3] = target.width();
  event.xclient.data.l[4] = target.height();
  return XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask,
                    &event) != 0;
}

bool restoreLayout(Display* display, Window window, const WindowLayout& layout) {
  if (window == 0 || !layout.valid) return false;
  QScreen* targetScreen = nullptr;
  for (QScreen* screen : QGuiApplication::screens()) {
    if (screen->name() == layout.monitorId) {
      targetScreen = screen;
      break;
    }
  }
  if (targetScreen == nullptr) targetScreen = QGuiApplication::primaryScreen();
  if (targetScreen == nullptr) return false;

  const QRect target = clampToAvailableGeometry(layout.geometry, targetScreen->availableGeometry());
  const QRect nativeTarget = toNativeGeometry(targetScreen, target);
  setMaximized(display, window, false);
  if (!moveResizeWindow(display, window, nativeTarget)) return false;
  if (layout.maximized) setMaximized(display, window, true);
  XFlush(display);
  return true;
}

}  // namespace

class SteamWindows::Impl {
 public:
  Impl() {
    if (!QGuiApplication::platformName().contains(QStringLiteral("wayland"), Qt::CaseInsensitive)) {
      display = XOpenDisplay(nullptr);
    }
  }
  ~Impl() {
    if (display != nullptr) XCloseDisplay(display);
  }

  Display* display = nullptr;
};

SteamWindows::SteamWindows() : impl(std::make_unique<Impl>()) {}
SteamWindows::~SteamWindows() = default;

bool SteamWindows::positionManagementAvailable() const {
  return impl->display != nullptr;
}

bool SteamWindows::lifecycleObservationAvailable() const {
  return impl->display != nullptr;
}

bool SteamWindows::bigPictureIsOpen() const {
  if (impl->display == nullptr) return false;
  const auto windows = steamWindows(impl->display);
  return findWindow(windows, isBigPictureTitle, false) != 0;
}

bool SteamWindows::takeWindowChange() {
  return false;
}

WindowCaptureResult SteamWindows::captureWindowLayouts() const {
  WindowCaptureResult result;
  if (impl->display == nullptr) return result;
  const auto windows = steamWindows(impl->display);
  const Window steam = findWindow(windows, isDesktopTitle, true);
  const Window friends = findWindow(windows, isFriendsTitle, true);
  if (steam != 0) {
    result.steam = captureLayout(impl->display, steam);
    result.steamFound = result.steam.valid;
  }
  if (friends != 0) {
    result.friends = captureLayout(impl->display, friends);
    result.friendsFound = result.friends.valid;
  }
  return result;
}

WindowRestoreResult SteamWindows::restoreWindowLayouts(const WindowLayout& steamLayout,
                                                       const WindowLayout& friendsLayout) const {
  WindowRestoreResult result;
  if (impl->display == nullptr) return result;
  const auto windows = steamWindows(impl->display);
  result.steamRestored = restoreLayout(impl->display, findWindow(windows, isDesktopTitle, false), steamLayout);
  result.friendsRestored = restoreLayout(impl->display, findWindow(windows, isFriendsTitle, false), friendsLayout);
  return result;
}

#else

class SteamWindows::Impl {};

SteamWindows::SteamWindows() : impl(std::make_unique<Impl>()) {}
SteamWindows::~SteamWindows() = default;
bool SteamWindows::positionManagementAvailable() const { return false; }
bool SteamWindows::lifecycleObservationAvailable() const { return false; }
bool SteamWindows::bigPictureIsOpen() const { return false; }
bool SteamWindows::takeWindowChange() { return false; }
WindowCaptureResult SteamWindows::captureWindowLayouts() const { return {}; }
WindowRestoreResult SteamWindows::restoreWindowLayouts(const WindowLayout&, const WindowLayout&) const { return {}; }

#endif
