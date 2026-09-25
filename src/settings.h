#pragma once

#include <QColor>
#include <QRect>
#include <QSettings>
#include <QString>

enum class TrayIconMode {
  Battery,
  Number,
  BatteryAndNumber,
};

enum class ChargingAnimationMode {
  Static,
  FiveStage,
  Smooth,
};

struct IconAppearance {
  QColor batteryOutlineColor;
  QColor batteryFillColor;
  QColor lowBatteryFillColor;
  QColor lowBatteryOutlineColor;
  QColor batteryBackgroundColor;
  QColor chargingColor;
  QColor chargingBatteryFillColor;
  QColor chargingBatteryOutlineColor;
  QColor disconnectedColor;
  QColor numberTextColor;
  QColor numberOutlineColor;
  QColor lowBatteryTextColor;
  QColor lowBatteryTextOutlineColor;
  QColor chargingTextColor;
  QColor chargingTextOutlineColor;
  QString fontFamily;
  int numberTextSize = 112;
  int numberOutlineWidth = 10;
  int combinedTextSize = 80;
  int combinedOutlineWidth = 8;
  int combinedVerticalOffset = 0;
  int batteryOutlineWidth = 12;
  bool boldText = true;
  ChargingAnimationMode chargingAnimation = ChargingAnimationMode::Static;
};

struct WindowLayout {
  QString monitorId;
  QRect geometry;
  bool maximized = false;
  bool valid = false;
};

class AppSettings {
 public:
  TrayIconMode trayIconMode() const;
  bool setTrayIconMode(TrayIconMode mode);

  IconAppearance iconAppearance(const IconAppearance& defaults) const;
  bool setIconAppearance(const IconAppearance& appearance);
  bool resetIconAppearance();

  bool openFriendsOnBigPictureExit() const;
  bool setOpenFriendsOnBigPictureExit(bool enabled);

  bool openBigPictureOnControllerWake() const;
  bool setOpenBigPictureOnControllerWake(bool enabled);

  WindowLayout steamWindowLayout() const;
  WindowLayout friendsWindowLayout() const;
  bool setSteamWindowLayout(const WindowLayout& layout);
  bool setFriendsWindowLayout(const WindowLayout& layout);
  bool clearWindowLayouts();

 private:
  WindowLayout windowLayout(const QString& group) const;
  bool setWindowLayout(const QString& group, const WindowLayout& layout);

  mutable QSettings settings;
};
