# Extra Changes in This Fork

- Added direct support for the Steam Controller (2026) wired interface and wireless puck.
- Added optional Big Picture launch on controller wake and reliable Big Picture exit handling.
- Added Steam and Friends launch actions with saved, monitor-relative window layouts.
- Added battery, number, and combined tray modes with live appearance customization.
- Added state-specific battery fill/stroke and text/stroke colors for normal, low-battery, and charging states.
- Added static, five-stage, and smooth charging-meter animations.
- Added native Windows and X11 window-management backends with safe Wayland degradation.
- Added a custom Windows application icon and portable Windows release bundle.

---

# Steam Controller Utility

A lightweight Qt 6 tray utility for the Steam Controller (2026), based on
[Pixel1011/Steam-Controller-Battery-Monitor](https://github.com/Pixel1011/Steam-Controller-Battery-Monitor).

The utility monitors the controller through HIDAPI, displays its battery level, and coordinates Steam's desktop and
Big Picture interfaces.

## Features

- Can open `steam://open/bigpicture` when Raw HID controller reports resume after a connect or wake. This is opt-in and
  defaults off so upgrading retains battery-monitor-first behavior. The activity monitor handles controller-state
  reports (`0x42`, `0x45`, `0x47`) and puck connect statuses (`0x46`, `0x79`, state `2`).
- Detects the Big Picture window lifecycle independently of controller disconnects.
- Opens the normal Steam desktop UI after Big Picture exits.
- Optionally opens Friends after Big Picture exits. This defaults to enabled and is persisted.
- Saves and restores monitor-relative layouts for the Steam and Friends windows.
- Provides battery, numeric percentage, and combined battery/percentage tray icon modes.
- Provides a tray appearance editor with live previews, separate battery fill/stroke and text/stroke colors for normal,
  low-battery, and charging states, plus empty/disconnected colors, typography, and combined-number alignment. Changes
  are saved and applied to the tray icon immediately.
- Supports a static charging meter or optional five-stage (empty/25/50/75/100%) and smooth fill animations while
  retaining the real percentage in numeric icon modes.
- Preserves the original battery percentage, charge status, tooltip, and start-with-system functionality.

The tray menu includes:

- **Open Big Picture**
- **Open Big Picture on Controller Wake**
- **Open Steam**
- **Open Friends**
- **Open Friends on Big Picture Exit**
- **Tray Icon > Battery / Number / Battery + Number**
- **Tray Icon > Customize...**
- **Set Default Window Positions**
- **Clear Saved Window Positions**
- **Start with system**
- **Exit**

Settings are stored with Qt's platform-native `QSettings` backend. Selecting **Set Default Window Positions** again
overwrites each currently visible Steam/Friends layout. If only one of those windows is visible, only that saved layout
is updated. The **Open Steam** and **Open Friends** actions apply the corresponding saved layout after opening the
window.

### Recommended Controller-Off Chord

If **Open Big Picture on Controller Wake** is enabled, configure Steam's controller power-off chord to exit Big Picture
at the same time. In Steam, open **Settings > Controller > Guide Button Chord Layout**, edit the chord used to turn off
the controller (commonly **Guide/Steam + Y**), and add **Exit Big Picture** as an extra command. Steam's wording may vary
between client versions.

This makes the workflow symmetrical: turning on the controller opens Big Picture, while the power-off chord exits Big
Picture and allows this utility to reopen the desktop Steam and Friends windows according to your saved preferences.

## Platform Support

**Supported device:** Steam Controller (2026), including the tested Valve `28DE:1302` wired interface and `28DE:1304`
wireless puck. Other controllers are unsupported.

### Windows

Windows has full intended support. A WinEvent hook tracks Steam top-level window creation, destruction, visibility, and
title changes. Steam and Friends layouts are captured/restored with Win32 APIs and clamped to an available display when
a saved monitor no longer exists.

### Linux X11

Battery monitoring, Steam URI actions, Big Picture lifecycle detection, and Steam/Friends layout management are
supported. Layout management uses EWMH/X11 APIs and monitor identities exposed by Qt.

### Linux Wayland

Battery monitoring, tray icon modes, settings, Steam URI actions, and start-with-system remain available. Generic
Wayland does not permit applications to enumerate or move other applications' windows, so Big Picture exit detection
and the automatic/manual external-window layout features are disabled for a native Wayland session. XWayland behavior
depends on how the desktop session exposes Steam's windows.

Big Picture identification uses Steam's top-level window title (`Big Picture`/`GamepadUI`) while restricting candidates
to `steam` and `steamwebhelper` processes. Alt-tabbing, minimizing, or hiding the window does not count as an exit; the
window must disappear for a sustained 2.5 seconds.

## Running

### Windows

Extract the Windows release and run `Steam-Controller-Utility.exe`. Steam must be registered as the handler for
`steam://` URLs.

### Linux

Install the Qt 6, HIDAPI, and X11 runtime libraries used by your distribution, then run `./scbattery-monitor`.
HID permissions may require a suitable udev rule for Valve devices.

## Compiling

Clone with submodules:

```bash
git clone --recurse-submodules https://github.com/CasketPizza/Steam-Controller-Battery-Monitor.git
cd Steam-Controller-Battery-Monitor
```

### Debian/Ubuntu

```bash
sudo apt update
sudo apt install build-essential pkg-config libhidapi-dev libqt6core6 libqt6gui6 \
  qt6-base-dev qt6-base-dev-tools libx11-dev
make
```

Install `qt6-wayland` as well when running Qt applications in a native Wayland session.

### Arch Linux

```bash
sudo pacman -S --needed base-devel hidapi qt6-base libx11
make
```

Install `qt6-wayland` as well for native Wayland sessions.

### Windows (MSYS2 UCRT64)

```bash
pacman -Syu
pacman -S mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-hidapi \
  mingw-w64-ucrt-x86_64-make \
  mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-qt6-base
mingw32-make release
```

The project uses C++20 and builds directly through the included `Makefile`.

## Credits

The original battery monitor and its TritonLib HID support were created by
[Pixel1011](https://github.com/Pixel1011). TritonLib includes controller data structures derived from SDL under SDL's
zlib license notice. See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) for dependency licensing details.
