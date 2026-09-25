#pragma once

#include "icons.h"
#include "settings.h"

#include <QDialog>
#include <QElapsedTimer>
#include <QString>
#include <functional>
#include <vector>

class QCheckBox;
class QComboBox;
class QFontComboBox;
class QFormLayout;
class QLabel;
class QPushButton;
class QSpinBox;

class IconSettingsDialog : public QDialog {
 public:
  using ApplyAppearance = std::function<bool(const IconAppearance&, bool)>;

  IconSettingsDialog(const IconAppearance& appearance, const IconAppearance& defaults,
                     ApplyAppearance applyAppearance, QWidget* parent = nullptr);

 private:
  struct ColorControl {
    QColor IconAppearance::*member;
    QPushButton* button;
    QString title;
  };

  void addColorControl(QFormLayout* layout, const QString& label, QColor IconAppearance::*member);
  void updateControls();
  void updateColorButton(const ColorControl& control);
  void updatePreviews();
  void applyChanges(bool restoreDefaults = false);

  IconAppearance currentAppearance;
  IconAppearance defaultAppearance;
  IconAppearance appliedAppearance;
  ApplyAppearance applyAppearance;
  IconGenerator previewGenerator;
  std::vector<ColorControl> colorControls;
  QFontComboBox* fontCombo = nullptr;
  QSpinBox* numberSize = nullptr;
  QSpinBox* numberOutlineWidth = nullptr;
  QSpinBox* combinedSize = nullptr;
  QSpinBox* combinedOutlineWidth = nullptr;
  QSpinBox* combinedVerticalOffset = nullptr;
  QSpinBox* batteryOutlineWidth = nullptr;
  QCheckBox* boldText = nullptr;
  QComboBox* chargingAnimation = nullptr;
  QComboBox* previewState = nullptr;
  QLabel* batteryPreview = nullptr;
  QLabel* numberPreview = nullptr;
  QLabel* combinedPreview = nullptr;
  QElapsedTimer previewAnimationClock;
};
