#include "icons.h"

#include <QApplication>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QSettings>
#include <QTransform>
#include <QtGlobal>
#include <algorithm>

#ifdef __linux__
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#endif

namespace {

constexpr int ICON_SIZE = 256;

int chargingFillPercentage(ChargingAnimationMode mode, int actualPercentage, qint64 elapsedMs) {
  switch (mode) {
    case ChargingAnimationMode::FiveStage: {
      constexpr int stages[] = {0, 25, 50, 75, 100};
      return stages[(elapsedMs / 600) % 5];
    }
    case ChargingAnimationMode::Smooth:
      return std::min(100, static_cast<int>((elapsedMs % 3000) * 100 / 2750));
    case ChargingAnimationMode::Static:
    default:
      return actualPercentage;
  }
}

void drawOutlinedText(QPainter& painter, const QRectF& bounds, const QString& text, const QFont& font,
                      const QColor& foreground, const QColor& outline, double outlineWidth) {
  const double inset = outlineWidth / 2.0;
  const QRectF safeBounds = bounds.adjusted(inset, inset, -inset, -inset);
  QPainterPath textPath;
  textPath.addText(0, 0, font, text);
  const QRectF glyphBounds = textPath.boundingRect();
  if (glyphBounds.isEmpty()) return;

  // Treat the configured pixel size as visible glyph height, then fit the result inside the icon if necessary.
  const double requestedScale = std::max(1, font.pixelSize()) / glyphBounds.height();
  const double widthScale = safeBounds.width() / glyphBounds.width();
  const double heightScale = safeBounds.height() / glyphBounds.height();
  const double scale = std::min({requestedScale, widthScale, heightScale});
  QTransform transform;
  transform.translate(safeBounds.center().x(), safeBounds.center().y());
  transform.scale(scale, scale);
  transform.translate(-glyphBounds.center().x(), -glyphBounds.center().y());
  textPath = transform.map(textPath);

  painter.setBrush(foreground);
  if (outlineWidth > 0) {
    painter.setPen(QPen(outline, outlineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  } else {
    painter.setPen(Qt::NoPen);
  }
  painter.drawPath(textPath);
}

}  // namespace

IconGenerator::IconGenerator() {
  QPixmapCache::setCacheLimit(1024);
#ifdef __linux__
  QDBusInterface portalSettings(QStringLiteral("org.freedesktop.portal.Desktop"),
                                QStringLiteral("/org/freedesktop/portal/desktop"),
                                QStringLiteral("org.freedesktop.portal.Settings"), QDBusConnection::sessionBus());
  if (portalSettings.isValid()) {
    const QDBusReply<QDBusVariant> reply =
        portalSettings.call(QStringLiteral("ReadOne"), QStringLiteral("org.freedesktop.appearance"),
                            QStringLiteral("color-scheme"));
    if (reply.isValid()) {
      const uint scheme = reply.value().variant().toUInt();
      if (scheme == 1) isDarkmode = true;
      if (scheme == 2) isDarkmode = false;
    }
  }
#elif defined(_WIN32)
  QSettings themeSettings(
      QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
      QSettings::NativeFormat);
  isDarkmode = themeSettings.value(QStringLiteral("SystemUsesLightTheme"), 1).toInt() == 0;
#endif

  const QColor foreground = isDarkmode ? Qt::white : Qt::black;
  const QColor contrast = isDarkmode ? Qt::black : Qt::white;
  defaultStyle.batteryOutlineColor = foreground;
  defaultStyle.batteryFillColor = QColor(0, 205, 55);
  defaultStyle.lowBatteryFillColor = QColor(210, 35, 35);
  defaultStyle.lowBatteryOutlineColor = foreground;
  defaultStyle.batteryBackgroundColor = QColor(0, 0, 0, 0);
  defaultStyle.chargingColor = contrast;
  defaultStyle.chargingBatteryFillColor = defaultStyle.batteryFillColor;
  defaultStyle.chargingBatteryOutlineColor = foreground;
  defaultStyle.disconnectedColor = Qt::red;
  defaultStyle.numberTextColor = foreground;
  defaultStyle.numberOutlineColor = contrast;
  defaultStyle.lowBatteryTextColor = foreground;
  defaultStyle.lowBatteryTextOutlineColor = contrast;
  defaultStyle.chargingTextColor = foreground;
  defaultStyle.chargingTextOutlineColor = contrast;
  defaultStyle.fontFamily = QApplication::font().family();
  defaultStyle.numberTextSize = 112;
  defaultStyle.numberOutlineWidth = 10;
  defaultStyle.combinedTextSize = 80;
  defaultStyle.combinedOutlineWidth = 8;
  defaultStyle.combinedVerticalOffset = 0;
  defaultStyle.batteryOutlineWidth = 12;
  defaultStyle.boldText = true;
  style = defaultStyle;
}

IconAppearance IconGenerator::defaultAppearance() const {
  return defaultStyle;
}

void IconGenerator::setAppearance(const IconAppearance& appearance) {
  style = appearance;
  cachedKey = -1;
  cachedIcon = QIcon();
}

QIcon IconGenerator::createIcon(const TritonBatteryStatus_t* battery, TrayIconMode mode, qint64 animationElapsedMs) {
  const bool disconnected = battery == nullptr || (battery->sSystemVoltage == 0 && battery->ucBatteryLevel == 0);
  const int percentage = disconnected ? 0 : std::clamp(static_cast<int>(battery->ucBatteryLevel), 0, 100);
  const bool charging = !disconnected && battery->ucChargeState == EChargeState::k_EChargeStateCharging;
  const int displayedFillPercentage = charging && mode != TrayIconMode::Number
                                          ? chargingFillPercentage(style.chargingAnimation, percentage, animationElapsedMs)
                                          : percentage;
  const int stateKey = static_cast<int>(mode) * 4 + (charging ? 2 : 0) + (disconnected ? 1 : 0);
  const int cacheKey = (stateKey * 101 + percentage) * 101 + displayedFillPercentage;

  if (cacheKey == cachedKey) return cachedIcon;

  cachedKey = cacheKey;
  cachedIcon = renderIcon(percentage, displayedFillPercentage, charging, disconnected, mode);
  return cachedIcon;
}

QIcon IconGenerator::renderIcon(int percentage, int displayedFillPercentage, bool charging, bool disconnected,
                                TrayIconMode mode) const {
  QPixmap pixmap(ICON_SIZE, ICON_SIZE);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);

  const QString percentageText = disconnected ? QStringLiteral("--") : QString::number(percentage);
  const bool lowBattery = !disconnected && !charging && percentage <= 10;
  const QColor batteryStrokeColor = charging   ? style.chargingBatteryOutlineColor
                                    : lowBattery ? style.lowBatteryOutlineColor
                                                 : style.batteryOutlineColor;
  const QColor batteryFillColor = charging   ? style.chargingBatteryFillColor
                                  : lowBattery ? style.lowBatteryFillColor
                                               : style.batteryFillColor;
  const QColor textColor = charging   ? style.chargingTextColor
                           : lowBattery ? style.lowBatteryTextColor
                                        : style.numberTextColor;
  const QColor textStrokeColor = charging   ? style.chargingTextOutlineColor
                                 : lowBattery ? style.lowBatteryTextOutlineColor
                                              : style.numberOutlineColor;

  if (mode == TrayIconMode::Number) {
    QFont font(style.fontFamily);
    font.setBold(style.boldText);
    font.setPixelSize(std::max(1, style.numberTextSize));
    drawOutlinedText(painter, QRectF(8, 20, 240, 216), percentageText, font, textColor, textStrokeColor,
                     std::clamp(style.numberOutlineWidth, 0, 32));
    return QIcon(pixmap);
  }

  const int batteryOutlineWidth = std::clamp(style.batteryOutlineWidth, 1, 32);
  const double halfOutline = batteryOutlineWidth / 2.0;
  const double bodyLeft = std::max(7.0, halfOutline + 1.0);
  const QRectF body(bodyLeft, 48, 228 - bodyLeft, 160);
  const QRectF terminal(228, 96, 22, 64);
  const double interiorInset = halfOutline + 2.0;
  const QRectF interior = body.adjusted(interiorInset, interiorInset, -interiorInset, -interiorInset);
  QPainterPath interiorPath;
  interiorPath.addRoundedRect(interior, 10, 10);

  painter.setPen(Qt::NoPen);
  painter.setBrush(batteryStrokeColor);
  painter.drawRoundedRect(terminal, 7, 7);

  if (style.batteryBackgroundColor.alpha() > 0) {
    painter.fillPath(interiorPath, style.batteryBackgroundColor);
  }

  if (!disconnected && displayedFillPercentage > 0) {
    painter.save();
    painter.setClipPath(interiorPath);
    painter.fillRect(QRectF(interior.left(), interior.top(), interior.width() * displayedFillPercentage / 100.0,
                            interior.height()),
                     batteryFillColor);
    painter.restore();
  }

  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(batteryStrokeColor, batteryOutlineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.drawRoundedRect(body, 20, 20);

  if (disconnected) {
    painter.setPen(QPen(style.disconnectedColor, batteryOutlineWidth, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(20, 240), QPointF(240, 20));
  } else if (charging) {
    QPainterPath bolt;
    if (mode == TrayIconMode::Battery) {
      bolt.moveTo(60, 15);
      bolt.lineTo(18, 135);
      bolt.lineTo(45, 135);
      bolt.lineTo(32, 240);
      bolt.lineTo(88, 105);
      bolt.lineTo(58, 105);
    } else {
      bolt.moveTo(38, 4);
      bolt.lineTo(19, 29);
      bolt.lineTo(31, 29);
      bolt.lineTo(24, 49);
      bolt.lineTo(50, 20);
      bolt.lineTo(38, 20);
    }
    bolt.closeSubpath();
    painter.setPen(Qt::NoPen);
    painter.setBrush(style.chargingColor);
    painter.drawPath(bolt);
  }

  if (mode == TrayIconMode::BatteryAndNumber && !disconnected) {
    QFont font(style.fontFamily);
    font.setBold(style.boldText);
    font.setPixelSize(std::max(1, style.combinedTextSize));
    QRectF textBounds = interior.adjusted(8, 0, -8, 0);
    textBounds.translate(0, std::clamp(style.combinedVerticalOffset, -48, 48));
    drawOutlinedText(painter, textBounds, percentageText, font, textColor, textStrokeColor,
                     std::clamp(style.combinedOutlineWidth, 0, 32));
  }

  return QIcon(pixmap);
}
