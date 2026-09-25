#include "settings.h"

#include <algorithm>

namespace {

constexpr auto STEAM_WINDOW_GROUP = "windows/steam";
constexpr auto FRIENDS_WINDOW_GROUP = "windows/friends";

QString trayIconModeName(TrayIconMode mode) {
  switch (mode) {
    case TrayIconMode::Number:
      return QStringLiteral("number");
    case TrayIconMode::BatteryAndNumber:
      return QStringLiteral("battery-and-number");
    case TrayIconMode::Battery:
    default:
      return QStringLiteral("battery");
  }
}

QString chargingAnimationModeName(ChargingAnimationMode mode) {
  switch (mode) {
    case ChargingAnimationMode::FiveStage:
      return QStringLiteral("five-stage");
    case ChargingAnimationMode::Smooth:
      return QStringLiteral("smooth");
    case ChargingAnimationMode::Static:
    default:
      return QStringLiteral("static");
  }
}

ChargingAnimationMode chargingAnimationMode(const QString& name) {
  if (name == QStringLiteral("five-stage") || name == QStringLiteral("four-stage")) {
    return ChargingAnimationMode::FiveStage;
  }
  if (name == QStringLiteral("smooth")) return ChargingAnimationMode::Smooth;
  return ChargingAnimationMode::Static;
}

}  // namespace

TrayIconMode AppSettings::trayIconMode() const {
  const QString value = settings.value(QStringLiteral("tray/iconMode"), QStringLiteral("battery")).toString();
  if (value == QStringLiteral("number")) return TrayIconMode::Number;
  if (value == QStringLiteral("battery-and-number")) return TrayIconMode::BatteryAndNumber;
  return TrayIconMode::Battery;
}

bool AppSettings::setTrayIconMode(TrayIconMode mode) {
  settings.setValue(QStringLiteral("tray/iconMode"), trayIconModeName(mode));
  settings.sync();
  return settings.status() == QSettings::NoError;
}

IconAppearance AppSettings::iconAppearance(const IconAppearance& defaults) const {
  settings.beginGroup(QStringLiteral("tray/appearance"));
  IconAppearance appearance;
  appearance.batteryOutlineColor = settings.value(QStringLiteral("batteryOutlineColor"), defaults.batteryOutlineColor).value<QColor>();
  appearance.batteryFillColor = settings.value(QStringLiteral("batteryFillColor"), defaults.batteryFillColor).value<QColor>();
  appearance.lowBatteryFillColor = settings.value(QStringLiteral("lowBatteryFillColor"), defaults.lowBatteryFillColor).value<QColor>();
  appearance.lowBatteryOutlineColor =
      settings.value(QStringLiteral("lowBatteryOutlineColor"), defaults.lowBatteryOutlineColor).value<QColor>();
  appearance.batteryBackgroundColor =
      settings.value(QStringLiteral("batteryBackgroundColor"), defaults.batteryBackgroundColor).value<QColor>();
  appearance.chargingColor = settings.value(QStringLiteral("chargingColor"), defaults.chargingColor).value<QColor>();
  appearance.chargingBatteryFillColor =
      settings.value(QStringLiteral("chargingBatteryFillColor"), defaults.chargingBatteryFillColor).value<QColor>();
  appearance.chargingBatteryOutlineColor =
      settings.value(QStringLiteral("chargingBatteryOutlineColor"), defaults.chargingBatteryOutlineColor).value<QColor>();
  appearance.disconnectedColor =
      settings.value(QStringLiteral("disconnectedColor"), defaults.disconnectedColor).value<QColor>();
  appearance.numberTextColor = settings.value(QStringLiteral("numberTextColor"), defaults.numberTextColor).value<QColor>();
  appearance.numberOutlineColor =
      settings.value(QStringLiteral("numberOutlineColor"), defaults.numberOutlineColor).value<QColor>();
  appearance.lowBatteryTextColor =
      settings.value(QStringLiteral("lowBatteryTextColor"), defaults.lowBatteryTextColor).value<QColor>();
  appearance.lowBatteryTextOutlineColor =
      settings.value(QStringLiteral("lowBatteryTextOutlineColor"), defaults.lowBatteryTextOutlineColor).value<QColor>();
  appearance.chargingTextColor =
      settings.value(QStringLiteral("chargingTextColor"), defaults.chargingTextColor).value<QColor>();
  appearance.chargingTextOutlineColor =
      settings.value(QStringLiteral("chargingTextOutlineColor"), defaults.chargingTextOutlineColor).value<QColor>();
  appearance.fontFamily = settings.value(QStringLiteral("fontFamily"), defaults.fontFamily).toString();
  appearance.numberTextSize =
      std::clamp(settings.value(QStringLiteral("numberTextSize"), defaults.numberTextSize).toInt(), 1, 9999);
  appearance.numberOutlineWidth = std::clamp(
      settings.value(QStringLiteral("numberOutlineWidth"), defaults.numberOutlineWidth).toInt(), 0, 32);
  appearance.combinedTextSize = std::clamp(
      settings
          .value(QStringLiteral("combinedTextSize"),
                 settings.value(QStringLiteral("percentageTextSize"), defaults.combinedTextSize))
          .toInt(),
      1, 9999);
  appearance.combinedOutlineWidth = std::clamp(
      settings
          .value(QStringLiteral("combinedOutlineWidth"),
                 settings.value(QStringLiteral("percentageOutlineWidth"), defaults.combinedOutlineWidth))
          .toInt(),
      0, 32);
  appearance.combinedVerticalOffset = std::clamp(
      settings
          .value(QStringLiteral("combinedVerticalOffset"),
                 settings.value(QStringLiteral("percentageVerticalOffset"), defaults.combinedVerticalOffset))
          .toInt(),
      -48, 48);
  appearance.batteryOutlineWidth = std::clamp(
      settings.value(QStringLiteral("batteryOutlineWidth"), defaults.batteryOutlineWidth).toInt(), 1, 32);
  appearance.boldText = settings.value(QStringLiteral("boldText"), defaults.boldText).toBool();
  const QString storedChargingAnimation =
      settings.value(QStringLiteral("chargingAnimation"), chargingAnimationModeName(defaults.chargingAnimation)).toString();
  appearance.chargingAnimation = chargingAnimationMode(storedChargingAnimation);
  if (storedChargingAnimation == QStringLiteral("four-stage")) {
    settings.setValue(QStringLiteral("chargingAnimation"), QStringLiteral("five-stage"));
  }
  settings.endGroup();
  if (storedChargingAnimation == QStringLiteral("four-stage")) settings.sync();
  return appearance;
}

bool AppSettings::setIconAppearance(const IconAppearance& appearance) {
  settings.beginGroup(QStringLiteral("tray/appearance"));
  settings.setValue(QStringLiteral("batteryOutlineColor"), appearance.batteryOutlineColor);
  settings.setValue(QStringLiteral("batteryFillColor"), appearance.batteryFillColor);
  settings.setValue(QStringLiteral("lowBatteryFillColor"), appearance.lowBatteryFillColor);
  settings.setValue(QStringLiteral("lowBatteryOutlineColor"), appearance.lowBatteryOutlineColor);
  settings.setValue(QStringLiteral("batteryBackgroundColor"), appearance.batteryBackgroundColor);
  settings.setValue(QStringLiteral("chargingColor"), appearance.chargingColor);
  settings.setValue(QStringLiteral("chargingBatteryFillColor"), appearance.chargingBatteryFillColor);
  settings.setValue(QStringLiteral("chargingBatteryOutlineColor"), appearance.chargingBatteryOutlineColor);
  settings.setValue(QStringLiteral("disconnectedColor"), appearance.disconnectedColor);
  settings.setValue(QStringLiteral("numberTextColor"), appearance.numberTextColor);
  settings.setValue(QStringLiteral("numberOutlineColor"), appearance.numberOutlineColor);
  settings.setValue(QStringLiteral("lowBatteryTextColor"), appearance.lowBatteryTextColor);
  settings.setValue(QStringLiteral("lowBatteryTextOutlineColor"), appearance.lowBatteryTextOutlineColor);
  settings.setValue(QStringLiteral("chargingTextColor"), appearance.chargingTextColor);
  settings.setValue(QStringLiteral("chargingTextOutlineColor"), appearance.chargingTextOutlineColor);
  settings.setValue(QStringLiteral("fontFamily"), appearance.fontFamily);
  settings.setValue(QStringLiteral("numberTextSize"), appearance.numberTextSize);
  settings.setValue(QStringLiteral("numberOutlineWidth"), appearance.numberOutlineWidth);
  settings.setValue(QStringLiteral("combinedTextSize"), appearance.combinedTextSize);
  settings.setValue(QStringLiteral("combinedOutlineWidth"), appearance.combinedOutlineWidth);
  settings.setValue(QStringLiteral("combinedVerticalOffset"), appearance.combinedVerticalOffset);
  settings.remove(QStringLiteral("percentageTextColor"));
  settings.remove(QStringLiteral("percentageOutlineColor"));
  settings.remove(QStringLiteral("percentageTextSize"));
  settings.remove(QStringLiteral("percentageOutlineWidth"));
  settings.remove(QStringLiteral("percentageVerticalOffset"));
  settings.setValue(QStringLiteral("batteryOutlineWidth"), appearance.batteryOutlineWidth);
  settings.setValue(QStringLiteral("boldText"), appearance.boldText);
  settings.setValue(QStringLiteral("chargingAnimation"), chargingAnimationModeName(appearance.chargingAnimation));
  settings.endGroup();
  settings.sync();
  return settings.status() == QSettings::NoError;
}

bool AppSettings::resetIconAppearance() {
  settings.remove(QStringLiteral("tray/appearance"));
  settings.sync();
  return settings.status() == QSettings::NoError;
}

bool AppSettings::openFriendsOnBigPictureExit() const {
  return settings.value(QStringLiteral("steam/openFriendsOnBigPictureExit"), true).toBool();
}

bool AppSettings::setOpenFriendsOnBigPictureExit(bool enabled) {
  settings.setValue(QStringLiteral("steam/openFriendsOnBigPictureExit"), enabled);
  settings.sync();
  return settings.status() == QSettings::NoError;
}

bool AppSettings::openBigPictureOnControllerWake() const {
  return settings.value(QStringLiteral("steam/openBigPictureOnControllerWake"), false).toBool();
}

bool AppSettings::setOpenBigPictureOnControllerWake(bool enabled) {
  settings.setValue(QStringLiteral("steam/openBigPictureOnControllerWake"), enabled);
  settings.sync();
  return settings.status() == QSettings::NoError;
}

WindowLayout AppSettings::steamWindowLayout() const {
  return windowLayout(QString::fromLatin1(STEAM_WINDOW_GROUP));
}

WindowLayout AppSettings::friendsWindowLayout() const {
  return windowLayout(QString::fromLatin1(FRIENDS_WINDOW_GROUP));
}

bool AppSettings::setSteamWindowLayout(const WindowLayout& layout) {
  return setWindowLayout(QString::fromLatin1(STEAM_WINDOW_GROUP), layout);
}

bool AppSettings::setFriendsWindowLayout(const WindowLayout& layout) {
  return setWindowLayout(QString::fromLatin1(FRIENDS_WINDOW_GROUP), layout);
}

bool AppSettings::clearWindowLayouts() {
  settings.remove(QStringLiteral("windows"));
  settings.sync();
  return settings.status() == QSettings::NoError;
}

WindowLayout AppSettings::windowLayout(const QString& group) const {
  settings.beginGroup(group);
  WindowLayout layout;
  layout.monitorId = settings.value(QStringLiteral("monitorId")).toString();
  layout.geometry = settings.value(QStringLiteral("geometry")).toRect();
  layout.maximized = settings.value(QStringLiteral("maximized"), false).toBool();
  layout.valid = settings.value(QStringLiteral("valid"), false).toBool() && layout.geometry.isValid();
  settings.endGroup();
  return layout;
}

bool AppSettings::setWindowLayout(const QString& group, const WindowLayout& layout) {
  settings.beginGroup(group);
  settings.setValue(QStringLiteral("monitorId"), layout.monitorId);
  settings.setValue(QStringLiteral("geometry"), layout.geometry);
  settings.setValue(QStringLiteral("maximized"), layout.maximized);
  settings.setValue(QStringLiteral("valid"), layout.valid);
  settings.endGroup();
  settings.sync();
  return settings.status() == QSettings::NoError;
}
