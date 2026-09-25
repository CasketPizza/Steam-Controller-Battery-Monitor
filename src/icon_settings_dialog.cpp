#include "icon_settings_dialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <utility>

namespace {

QWidget* previewColumn(const QString& title, QLabel** preview, QWidget* parent) {
  auto* column = new QWidget(parent);
  auto* layout = new QVBoxLayout(column);
  layout->setContentsMargins(8, 4, 8, 4);
  layout->setAlignment(Qt::AlignCenter);

  *preview = new QLabel(column);
  (*preview)->setAlignment(Qt::AlignCenter);
  (*preview)->setFixedSize(80, 80);
  layout->addWidget(*preview);

  auto* label = new QLabel(title, column);
  label->setAlignment(Qt::AlignCenter);
  layout->addWidget(label);
  return column;
}

QString colorLabel(const QColor& color) {
  return color.alpha() == 255 ? color.name(QColor::HexRgb).toUpper() : color.name(QColor::HexArgb).toUpper();
}

}  // namespace

IconSettingsDialog::IconSettingsDialog(const IconAppearance& appearance, const IconAppearance& defaults,
                                       ApplyAppearance applyAppearanceCallback, QWidget* parent)
    : QDialog(parent),
      currentAppearance(appearance),
      defaultAppearance(defaults),
      appliedAppearance(appearance),
      applyAppearance(std::move(applyAppearanceCallback)) {
  setWindowTitle(QStringLiteral("Customize Tray Icon"));
  setMinimumWidth(560);
  resize(640, 700);

  auto* root = new QVBoxLayout(this);
  auto* scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  auto* scrollContent = new QWidget(scrollArea);
  auto* contentLayout = new QVBoxLayout(scrollContent);
  scrollArea->setWidget(scrollContent);
  root->addWidget(scrollArea, 1);
  auto* description = new QLabel(
      QStringLiteral("Adjust the tray artwork and typography. Transparent colors are supported, and previews use an "
                     "87% battery level. State colors apply to both number modes. Changes are saved and applied "
                     "immediately."),
      this);
  description->setWordWrap(true);
  contentLayout->addWidget(description);

  auto* previewGroup = new QGroupBox(QStringLiteral("Preview"), this);
  auto* previewGroupLayout = new QVBoxLayout(previewGroup);
  auto* previewStateLayout = new QHBoxLayout();
  previewStateLayout->addStretch();
  previewStateLayout->addWidget(new QLabel(QStringLiteral("State"), previewGroup));
  previewState = new QComboBox(previewGroup);
  previewState->addItems({QStringLiteral("Normal (87%)"), QStringLiteral("Low battery (8%)"),
                          QStringLiteral("Charging (87%)"), QStringLiteral("Disconnected")});
  previewStateLayout->addWidget(previewState);
  previewStateLayout->addStretch();
  previewGroupLayout->addLayout(previewStateLayout);
  auto* previewLayout = new QHBoxLayout();
  previewLayout->addStretch();
  previewLayout->addWidget(previewColumn(QStringLiteral("Battery"), &batteryPreview, previewGroup));
  previewLayout->addWidget(previewColumn(QStringLiteral("Number"), &numberPreview, previewGroup));
  previewLayout->addWidget(previewColumn(QStringLiteral("Battery + Number"), &combinedPreview, previewGroup));
  previewLayout->addStretch();
  previewGroupLayout->addLayout(previewLayout);
  contentLayout->addWidget(previewGroup);

  auto* colorsGroup = new QGroupBox(QStringLiteral("State Appearance"), this);
  auto* colorsGroupLayout = new QVBoxLayout(colorsGroup);
  auto* colorTabs = new QTabWidget(colorsGroup);

  auto* normalTab = new QWidget(colorTabs);
  auto* normalLayout = new QFormLayout(normalTab);
  addColorControl(normalLayout, QStringLiteral("Battery fill"), &IconAppearance::batteryFillColor);
  addColorControl(normalLayout, QStringLiteral("Battery stroke"), &IconAppearance::batteryOutlineColor);
  addColorControl(normalLayout, QStringLiteral("Text"), &IconAppearance::numberTextColor);
  addColorControl(normalLayout, QStringLiteral("Text stroke"), &IconAppearance::numberOutlineColor);
  colorTabs->addTab(normalTab, QStringLiteral("Normal"));

  auto* lowBatteryTab = new QWidget(colorTabs);
  auto* lowBatteryLayout = new QFormLayout(lowBatteryTab);
  addColorControl(lowBatteryLayout, QStringLiteral("Battery fill"), &IconAppearance::lowBatteryFillColor);
  addColorControl(lowBatteryLayout, QStringLiteral("Battery stroke"), &IconAppearance::lowBatteryOutlineColor);
  addColorControl(lowBatteryLayout, QStringLiteral("Text"), &IconAppearance::lowBatteryTextColor);
  addColorControl(lowBatteryLayout, QStringLiteral("Text stroke"), &IconAppearance::lowBatteryTextOutlineColor);
  colorTabs->addTab(lowBatteryTab, QStringLiteral("Low Battery"));

  auto* chargingTab = new QWidget(colorTabs);
  auto* chargingLayout = new QFormLayout(chargingTab);
  addColorControl(chargingLayout, QStringLiteral("Battery fill"), &IconAppearance::chargingBatteryFillColor);
  addColorControl(chargingLayout, QStringLiteral("Battery stroke"), &IconAppearance::chargingBatteryOutlineColor);
  addColorControl(chargingLayout, QStringLiteral("Text"), &IconAppearance::chargingTextColor);
  addColorControl(chargingLayout, QStringLiteral("Text stroke"), &IconAppearance::chargingTextOutlineColor);
  addColorControl(chargingLayout, QStringLiteral("Lightning bolt"), &IconAppearance::chargingColor);
  chargingAnimation = new QComboBox(chargingTab);
  chargingAnimation->addItem(QStringLiteral("Static (actual level)"),
                             static_cast<int>(ChargingAnimationMode::Static));
  chargingAnimation->addItem(QStringLiteral("Five-stage (empty / 25 / 50 / 75 / 100%)"),
                             static_cast<int>(ChargingAnimationMode::FiveStage));
  chargingAnimation->addItem(QStringLiteral("Smooth (0-100%)"),
                             static_cast<int>(ChargingAnimationMode::Smooth));
  chargingLayout->addRow(QStringLiteral("Meter animation"), chargingAnimation);
  colorTabs->addTab(chargingTab, QStringLiteral("Charging"));

  auto* otherTab = new QWidget(colorTabs);
  auto* otherLayout = new QFormLayout(otherTab);
  addColorControl(otherLayout, QStringLiteral("Battery empty background"),
                  &IconAppearance::batteryBackgroundColor);
  addColorControl(otherLayout, QStringLiteral("Disconnected slash"), &IconAppearance::disconnectedColor);
  colorTabs->addTab(otherTab, QStringLiteral("Other"));

  colorsGroupLayout->addWidget(colorTabs);
  contentLayout->addWidget(colorsGroup);

  auto* typographyGroup = new QGroupBox(QStringLiteral("Typography"), this);
  auto* typographyLayout = new QFormLayout(typographyGroup);
  fontCombo = new QFontComboBox(typographyGroup);
  numberSize = new QSpinBox(typographyGroup);
  numberSize->setRange(1, 9999);
  numberSize->setSuffix(QStringLiteral(" px"));
  numberOutlineWidth = new QSpinBox(typographyGroup);
  numberOutlineWidth->setRange(0, 32);
  numberOutlineWidth->setSuffix(QStringLiteral(" px"));
  combinedSize = new QSpinBox(typographyGroup);
  combinedSize->setRange(1, 9999);
  combinedSize->setSuffix(QStringLiteral(" px"));
  combinedOutlineWidth = new QSpinBox(typographyGroup);
  combinedOutlineWidth->setRange(0, 32);
  combinedOutlineWidth->setSuffix(QStringLiteral(" px"));
  combinedVerticalOffset = new QSpinBox(typographyGroup);
  combinedVerticalOffset->setRange(-48, 48);
  combinedVerticalOffset->setSuffix(QStringLiteral(" px"));
  combinedVerticalOffset->setToolTip(QStringLiteral("Positive values move the number down inside the battery."));
  batteryOutlineWidth = new QSpinBox(typographyGroup);
  batteryOutlineWidth->setRange(1, 32);
  batteryOutlineWidth->setSuffix(QStringLiteral(" px"));
  boldText = new QCheckBox(QStringLiteral("Bold"), typographyGroup);
  typographyLayout->addRow(QStringLiteral("Font family (both number modes)"), fontCombo);
  typographyLayout->addRow(QStringLiteral("Number-only text size"), numberSize);
  typographyLayout->addRow(QStringLiteral("Number-only stroke thickness"), numberOutlineWidth);
  typographyLayout->addRow(QStringLiteral("Battery + Number text size"), combinedSize);
  typographyLayout->addRow(QStringLiteral("Battery + Number stroke thickness"), combinedOutlineWidth);
  typographyLayout->addRow(QStringLiteral("Battery + Number vertical offset"), combinedVerticalOffset);
  typographyLayout->addRow(QStringLiteral("Battery stroke thickness"), batteryOutlineWidth);
  typographyLayout->addRow(QStringLiteral("Font weight (both number modes)"), boldText);
  contentLayout->addWidget(typographyGroup);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  QPushButton* restoreDefaults = buttons->addButton(QStringLiteral("Restore Defaults"), QDialogButtonBox::ResetRole);
  root->addWidget(buttons);

  connect(fontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont& font) {
    currentAppearance.fontFamily = font.family();
    applyChanges();
  });
  connect(numberSize, &QSpinBox::valueChanged, this, [this](int value) {
    currentAppearance.numberTextSize = value;
    applyChanges();
  });
  connect(numberOutlineWidth, &QSpinBox::valueChanged, this, [this](int value) {
    currentAppearance.numberOutlineWidth = value;
    applyChanges();
  });
  connect(combinedSize, &QSpinBox::valueChanged, this, [this](int value) {
    currentAppearance.combinedTextSize = value;
    applyChanges();
  });
  connect(combinedOutlineWidth, &QSpinBox::valueChanged, this, [this](int value) {
    currentAppearance.combinedOutlineWidth = value;
    applyChanges();
  });
  connect(combinedVerticalOffset, &QSpinBox::valueChanged, this, [this](int value) {
    currentAppearance.combinedVerticalOffset = value;
    applyChanges();
  });
  connect(batteryOutlineWidth, &QSpinBox::valueChanged, this, [this](int value) {
    currentAppearance.batteryOutlineWidth = value;
    applyChanges();
  });
  connect(boldText, &QCheckBox::toggled, this, [this](bool enabled) {
    currentAppearance.boldText = enabled;
    applyChanges();
  });
  connect(chargingAnimation, &QComboBox::currentIndexChanged, this, [this](int index) {
    currentAppearance.chargingAnimation =
        static_cast<ChargingAnimationMode>(chargingAnimation->itemData(index).toInt());
    previewAnimationClock.restart();
    applyChanges();
  });
  connect(restoreDefaults, &QPushButton::clicked, this, [this]() {
    currentAppearance = defaultAppearance;
    updateControls();
    applyChanges(true);
  });
  connect(previewState, &QComboBox::currentIndexChanged, this, [this]() { updatePreviews(); });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

  auto* animationTimer = new QTimer(this);
  animationTimer->setInterval(100);
  connect(animationTimer, &QTimer::timeout, this, [this]() {
    if (previewState->currentIndex() == 2 &&
        currentAppearance.chargingAnimation != ChargingAnimationMode::Static) {
      updatePreviews();
    }
  });
  previewAnimationClock.start();
  animationTimer->start();

  updateControls();
}

void IconSettingsDialog::addColorControl(QFormLayout* layout, const QString& label, QColor IconAppearance::*member) {
  auto* button = new QPushButton(this);
  button->setMinimumWidth(130);
  colorControls.push_back({member, button, label});
  const size_t index = colorControls.size() - 1;
  connect(button, &QPushButton::clicked, this, [this, index]() {
    ColorControl& control = colorControls[index];
    const QColor selected = QColorDialog::getColor(currentAppearance.*(control.member), this, control.title,
                                                    QColorDialog::ShowAlphaChannel);
    if (!selected.isValid()) return;
    currentAppearance.*(control.member) = selected;
    updateColorButton(control);
    applyChanges();
  });
  layout->addRow(label, button);
}

void IconSettingsDialog::updateControls() {
  for (const ColorControl& control : colorControls) updateColorButton(control);
  const QSignalBlocker fontBlocker(fontCombo);
  const QSignalBlocker numberBlocker(numberSize);
  const QSignalBlocker numberOutlineBlocker(numberOutlineWidth);
  const QSignalBlocker combinedBlocker(combinedSize);
  const QSignalBlocker combinedOutlineBlocker(combinedOutlineWidth);
  const QSignalBlocker combinedOffsetBlocker(combinedVerticalOffset);
  const QSignalBlocker batteryOutlineBlocker(batteryOutlineWidth);
  const QSignalBlocker boldBlocker(boldText);
  const QSignalBlocker chargingAnimationBlocker(chargingAnimation);
  fontCombo->setCurrentFont(QFont(currentAppearance.fontFamily));
  numberSize->setValue(currentAppearance.numberTextSize);
  numberOutlineWidth->setValue(currentAppearance.numberOutlineWidth);
  combinedSize->setValue(currentAppearance.combinedTextSize);
  combinedOutlineWidth->setValue(currentAppearance.combinedOutlineWidth);
  combinedVerticalOffset->setValue(currentAppearance.combinedVerticalOffset);
  batteryOutlineWidth->setValue(currentAppearance.batteryOutlineWidth);
  boldText->setChecked(currentAppearance.boldText);
  chargingAnimation->setCurrentIndex(
      chargingAnimation->findData(static_cast<int>(currentAppearance.chargingAnimation)));
  updatePreviews();
}

void IconSettingsDialog::updateColorButton(const ColorControl& control) {
  const QColor color = currentAppearance.*(control.member);
  const QColor buttonColor = palette().color(QPalette::Button);
  const qreal alpha = color.alphaF();
  const QColor displayColor(qRound(color.red() * alpha + buttonColor.red() * (1.0 - alpha)),
                            qRound(color.green() * alpha + buttonColor.green() * (1.0 - alpha)),
                            qRound(color.blue() * alpha + buttonColor.blue() * (1.0 - alpha)));
  const QColor labelColor = displayColor.lightness() < 128 ? Qt::white : Qt::black;
  control.button->setText(colorLabel(color));
  control.button->setStyleSheet(
      QStringLiteral("QPushButton { background-color: rgb(%1, %2, %3); color: %4; padding: 4px 10px; }")
          .arg(displayColor.red())
          .arg(displayColor.green())
          .arg(displayColor.blue())
          .arg(labelColor.name()));
}

void IconSettingsDialog::updatePreviews() {
  previewGenerator.setAppearance(currentAppearance);
  TritonBatteryStatus_t battery{};
  battery.ucBatteryLevel = previewState->currentIndex() == 1 ? 8 : 87;
  battery.ucChargeState = previewState->currentIndex() == 2 ? EChargeState::k_EChargeStateCharging
                                                            : EChargeState::k_EChargeStateDischarging;
  battery.sSystemVoltage = 1;
  const TritonBatteryStatus_t* previewBattery = previewState->currentIndex() == 3 ? nullptr : &battery;
  const qint64 animationElapsedMs = previewAnimationClock.isValid() ? previewAnimationClock.elapsed() : 0;
  batteryPreview->setPixmap(
      previewGenerator.createIcon(previewBattery, TrayIconMode::Battery, animationElapsedMs).pixmap(72, 72));
  numberPreview->setPixmap(
      previewGenerator.createIcon(previewBattery, TrayIconMode::Number, animationElapsedMs).pixmap(72, 72));
  combinedPreview->setPixmap(previewGenerator
                                 .createIcon(previewBattery, TrayIconMode::BatteryAndNumber, animationElapsedMs)
                                 .pixmap(72, 72));
}

void IconSettingsDialog::applyChanges(bool restoreDefaults) {
  updatePreviews();
  if (!applyAppearance || applyAppearance(currentAppearance, restoreDefaults)) {
    appliedAppearance = currentAppearance;
    return;
  }
  currentAppearance = appliedAppearance;
  updateControls();
}
