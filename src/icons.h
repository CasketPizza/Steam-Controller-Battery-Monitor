#pragma once
#include "settings.h"

#include <QIcon>
#include <QtGlobal>
#include <TritonController.h>

class IconGenerator {
 private:
  // cache required otherwise setIcon will increase in memory over time (assuming it has it's own cache or something)
  int cachedKey = -1;
  QIcon cachedIcon;
  bool isDarkmode = true;
  IconAppearance defaultStyle;
  IconAppearance style;

 public:
  IconGenerator();

  QIcon createIcon(const TritonBatteryStatus_t* battery, TrayIconMode mode, qint64 animationElapsedMs = 0);
  IconAppearance defaultAppearance() const;
  void setAppearance(const IconAppearance& appearance);

 private:
  QIcon renderIcon(int percentage, int displayedFillPercentage, bool charging, bool disconnected,
                   TrayIconMode mode) const;
};
