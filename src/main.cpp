#include "controller_activity.h"
#include "icon_settings_dialog.h"
#include "icons.h"
#include "settings.h"
#include "startup.h"
#include "steam_windows.h"

#include <TritonController.h>
#include <TritonFinder.h>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QMenu>
#include <QSignalBlocker>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <memory>

namespace {

QString getStatusMessage(const TritonBatteryStatus_t* battery) {
  if (battery == nullptr) return QStringLiteral("Status: Disconnected.");
  switch (static_cast<EChargeState>(battery->ucChargeState)) {
    case EChargeState::k_EChargeStateCharging:
      return QStringLiteral("Status: Charging.");
    case EChargeState::k_EChargeStateChargingDone:
      return QStringLiteral("Status: Finished charging.");
    case EChargeState::k_EChargeStateDischarging:
      return QStringLiteral("Status: Discharging.");
    case EChargeState::k_EChargeStateReset:
      return QStringLiteral("Status: Charge state reset.");
    case EChargeState::k_EChargeStateSrcValidate:
      return QStringLiteral("Status: Validating power source.");
    default:
      return QStringLiteral("Status: Unknown state.");
  }
}

bool openSteamPage(const QString& page) {
  return QDesktopServices::openUrl(QUrl(QStringLiteral("steam://open/%1").arg(page)));
}

QString captureMessage(const WindowCaptureResult& result) {
  if (result.steamFound && result.friendsFound) return QStringLiteral("Saved Steam and Friends window positions.");
  if (result.steamFound) return QStringLiteral("Saved Steam. Open Friends and try again to save both windows.");
  if (result.friendsFound) return QStringLiteral("Saved Friends. Open the main Steam window and try again to save both windows.");
  return QStringLiteral("No visible Steam or Friends window was found.");
}

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication::setOrganizationName(QStringLiteral("CasketPizza"));
  QCoreApplication::setApplicationName(QStringLiteral("Steam Controller Utility"));

  QApplication app(argc, argv);
  app.setQuitOnLastWindowClosed(false);
#ifndef SANITIZER_BUILD
  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    qCritical() << "No system tray is available";
    return 1;
  }
#endif

  AppSettings settings;
  IconGenerator iconGenerator;
  IconAppearance iconAppearance = settings.iconAppearance(iconGenerator.defaultAppearance());
  iconGenerator.setAppearance(iconAppearance);
  SteamWindows steamWindows;
  TritonFinder finder;
  ControllerActivityMonitor controllerActivity;
  std::unique_ptr<TritonController> controller;
  TrayIconMode iconMode = settings.trayIconMode();
  bool openBigPictureOnWake = settings.openBigPictureOnControllerWake();
  TritonBatteryStatus_t currentBattery{};
  bool hasBattery = false;
  QElapsedTimer chargingAnimationClock;

  QMenu menu;
  QAction* percentageAction = menu.addAction(QStringLiteral("Monitoring battery"));
  QAction* statusAction = menu.addAction(QStringLiteral("Status: Starting monitor..."));
  percentageAction->setEnabled(false);
  statusAction->setEnabled(false);

  menu.addSeparator();
  QAction* openBigPictureAction = menu.addAction(QStringLiteral("Open Big Picture"));
  QAction* openBigPictureOnWakeAction = menu.addAction(QStringLiteral("Open Big Picture on Controller Wake"));
  openBigPictureOnWakeAction->setCheckable(true);
  openBigPictureOnWakeAction->setChecked(openBigPictureOnWake);
  QAction* openSteamAction = menu.addAction(QStringLiteral("Open Steam"));
  QAction* openFriendsAction = menu.addAction(QStringLiteral("Open Friends"));
  QAction* friendsOnExitAction = menu.addAction(QStringLiteral("Open Friends on Big Picture Exit"));
  friendsOnExitAction->setCheckable(true);
  friendsOnExitAction->setChecked(settings.openFriendsOnBigPictureExit());
  const bool canObserveBigPicture = steamWindows.lifecycleObservationAvailable();
  friendsOnExitAction->setEnabled(canObserveBigPicture);
  if (!canObserveBigPicture) {
    friendsOnExitAction->setStatusTip(
        QStringLiteral("Big Picture lifecycle detection is unavailable in this desktop session."));
  }

  QMenu* trayIconMenu = menu.addMenu(QStringLiteral("Tray Icon"));
  QActionGroup* iconModeGroup = new QActionGroup(&menu);
  iconModeGroup->setExclusive(true);
  QAction* batteryIconAction = trayIconMenu->addAction(QStringLiteral("Battery"));
  QAction* numberIconAction = trayIconMenu->addAction(QStringLiteral("Number"));
  QAction* combinedIconAction = trayIconMenu->addAction(QStringLiteral("Battery + Number"));
  for (QAction* action : {batteryIconAction, numberIconAction, combinedIconAction}) {
    action->setCheckable(true);
    iconModeGroup->addAction(action);
  }
  batteryIconAction->setChecked(iconMode == TrayIconMode::Battery);
  numberIconAction->setChecked(iconMode == TrayIconMode::Number);
  combinedIconAction->setChecked(iconMode == TrayIconMode::BatteryAndNumber);
  trayIconMenu->addSeparator();
  QAction* customizeIconAction = trayIconMenu->addAction(QStringLiteral("Customize..."));

  QAction* setPositionsAction = menu.addAction(QStringLiteral("Set Default Window Positions"));
  QAction* clearPositionsAction = menu.addAction(QStringLiteral("Clear Saved Window Positions"));
  const bool canManageWindows = steamWindows.positionManagementAvailable();
  setPositionsAction->setEnabled(canManageWindows);
  if (!canManageWindows) {
    const QString unavailable = QStringLiteral("External window positioning is unavailable in this desktop session.");
    setPositionsAction->setStatusTip(unavailable);
  }

  QAction* startupAction = menu.addAction(QStringLiteral("Start with system"));
  startupAction->setCheckable(true);
  startupAction->setChecked(Startup::startsAtLogin());
  menu.addSeparator();
  QAction* exitAction = menu.addAction(QStringLiteral("Exit"));

  QSystemTrayIcon trayIcon;
  trayIcon.setToolTip(QStringLiteral("Loading..."));
  trayIcon.setContextMenu(&menu);
  trayIcon.setIcon(iconGenerator.createIcon(nullptr, iconMode));
  trayIcon.show();

  const auto updateTrayIcon = [&]() {
    const qint64 animationElapsedMs = chargingAnimationClock.isValid() ? chargingAnimationClock.elapsed() : 0;
    const QIcon nextIcon = iconGenerator.createIcon(hasBattery ? &currentBattery : nullptr, iconMode,
                                                    animationElapsedMs);
    if (trayIcon.icon().cacheKey() != nextIcon.cacheKey()) trayIcon.setIcon(nextIcon);
  };

  const auto showTrayMessage = [&](const QString& message, QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information) {
    trayIcon.showMessage(QStringLiteral("Steam Controller Utility"), message, icon);
  };

  QObject::connect(exitAction, &QAction::triggered, &app, &QCoreApplication::quit);
  QObject::connect(openBigPictureAction, &QAction::triggered, &app,
                   []() { openSteamPage(QStringLiteral("bigpicture")); });
  QObject::connect(openBigPictureOnWakeAction, &QAction::toggled, &app, [&](bool enabled) {
    if (settings.setOpenBigPictureOnControllerWake(enabled)) {
      openBigPictureOnWake = enabled;
      return;
    }
    QSignalBlocker block(openBigPictureOnWakeAction);
    openBigPictureOnWakeAction->setChecked(!enabled);
    showTrayMessage(QStringLiteral("Could not save the Big Picture wake preference."), QSystemTrayIcon::Warning);
  });
  QObject::connect(friendsOnExitAction, &QAction::toggled, &app, [&](bool enabled) {
    if (settings.setOpenFriendsOnBigPictureExit(enabled)) return;
    QSignalBlocker block(friendsOnExitAction);
    friendsOnExitAction->setChecked(!enabled);
    showTrayMessage(QStringLiteral("Could not save the Friends preference."), QSystemTrayIcon::Warning);
  });

  const auto setIconMode = [&](TrayIconMode mode) {
    const TrayIconMode previousMode = iconMode;
    if (!settings.setTrayIconMode(mode)) {
      QSignalBlocker block(iconModeGroup);
      batteryIconAction->setChecked(previousMode == TrayIconMode::Battery);
      numberIconAction->setChecked(previousMode == TrayIconMode::Number);
      combinedIconAction->setChecked(previousMode == TrayIconMode::BatteryAndNumber);
      showTrayMessage(QStringLiteral("Could not save the tray icon preference."), QSystemTrayIcon::Warning);
      return;
    }
    iconMode = mode;
    updateTrayIcon();
  };
  QObject::connect(batteryIconAction, &QAction::triggered, &app,
                   [&]() { setIconMode(TrayIconMode::Battery); });
  QObject::connect(numberIconAction, &QAction::triggered, &app, [&]() { setIconMode(TrayIconMode::Number); });
  QObject::connect(combinedIconAction, &QAction::triggered, &app,
                   [&]() { setIconMode(TrayIconMode::BatteryAndNumber); });
  QObject::connect(customizeIconAction, &QAction::triggered, &app, [&]() {
    IconSettingsDialog dialog(
        iconAppearance, iconGenerator.defaultAppearance(),
        [&](const IconAppearance& nextAppearance, bool restoreDefaults) {
          const bool saved = restoreDefaults ? settings.resetIconAppearance()
                                             : settings.setIconAppearance(nextAppearance);
          if (!saved) {
            showTrayMessage(QStringLiteral("Could not save the tray icon appearance."),
                            QSystemTrayIcon::Warning);
            return false;
          }
          iconAppearance = nextAppearance;
          iconGenerator.setAppearance(iconAppearance);
          if (hasBattery && currentBattery.ucChargeState == EChargeState::k_EChargeStateCharging) {
            chargingAnimationClock.restart();
          }
          updateTrayIcon();
          return true;
        });
    dialog.exec();
  });

  QObject::connect(startupAction, &QAction::toggled, &app, [&](bool enabled) {
    if (Startup::setStartsAtLogin(enabled)) return;
    QSignalBlocker block(startupAction);
    startupAction->setChecked(!enabled);
    showTrayMessage(QStringLiteral("Could not change the startup setting."), QSystemTrayIcon::Warning);
  });

  QObject::connect(setPositionsAction, &QAction::triggered, &app, [&]() {
    const WindowCaptureResult captured = steamWindows.captureWindowLayouts();
    bool saved = true;
    if (captured.steamFound) saved = settings.setSteamWindowLayout(captured.steam) && saved;
    if (captured.friendsFound) saved = settings.setFriendsWindowLayout(captured.friends) && saved;
    if (!saved) {
      showTrayMessage(QStringLiteral("Could not save the window positions."), QSystemTrayIcon::Warning);
      return;
    }
    showTrayMessage(captureMessage(captured),
                    captured.steamFound || captured.friendsFound ? QSystemTrayIcon::Information
                                                                 : QSystemTrayIcon::Warning);
  });

  QTimer layoutRestoreTimer;
  layoutRestoreTimer.setInterval(750);
  int layoutRestoreAttempts = 0;
  bool restoreSteamWindow = false;
  bool restoreFriendsWindow = false;
  QObject::connect(&layoutRestoreTimer, &QTimer::timeout, &app, [&]() {
    const WindowLayout steamLayout = restoreSteamWindow ? settings.steamWindowLayout() : WindowLayout{};
    const WindowLayout friendsLayout = restoreFriendsWindow ? settings.friendsWindowLayout() : WindowLayout{};
    const WindowRestoreResult result = steamWindows.restoreWindowLayouts(steamLayout, friendsLayout);
    const bool steamDone = !steamLayout.valid || result.steamRestored;
    const bool friendsDone = !friendsLayout.valid || result.friendsRestored;
    ++layoutRestoreAttempts;
    if ((steamDone && friendsDone) || layoutRestoreAttempts >= 80) layoutRestoreTimer.stop();
  });

  const auto beginLayoutRestore = [&](bool includeSteam, bool includeFriends, bool openWindows) {
    restoreSteamWindow = includeSteam;
    restoreFriendsWindow = includeFriends;
    layoutRestoreAttempts = 0;
    if (openWindows) {
      if (includeSteam) openSteamPage(QStringLiteral("main"));
      if (includeFriends) openSteamPage(QStringLiteral("friends"));
    }
    if (canManageWindows) layoutRestoreTimer.start();
  };

  QObject::connect(openSteamAction, &QAction::triggered, &app, [&]() { beginLayoutRestore(true, false, true); });
  QObject::connect(openFriendsAction, &QAction::triggered, &app, [&]() { beginLayoutRestore(false, true, true); });
  QObject::connect(clearPositionsAction, &QAction::triggered, &app, [&]() {
    layoutRestoreTimer.stop();
    restoreSteamWindow = false;
    restoreFriendsWindow = false;
    if (settings.clearWindowLayouts()) {
      showTrayMessage(QStringLiteral("Cleared the saved Steam and Friends window positions."));
    } else {
      showTrayMessage(QStringLiteral("Could not clear the saved window positions."), QSystemTrayIcon::Warning);
    }
  });

  bool bigPictureObserved = false;
  qint64 bigPictureMissingSince = -1;
  QElapsedTimer runTime;
  runTime.start();
  qint64 nextWindowFallbackCheck = 0;

  const auto handleBigPictureExit = [&]() {
    const bool includeFriends = settings.openFriendsOnBigPictureExit();
    openSteamPage(QStringLiteral("main"));
    if (includeFriends) openSteamPage(QStringLiteral("friends"));
    if (canManageWindows) beginLayoutRestore(true, includeFriends, false);
  };

  const auto updateBigPictureState = [&]() {
    if (!canObserveBigPicture) return;
    const qint64 now = runTime.elapsed();
    if (!steamWindows.takeWindowChange() && now < nextWindowFallbackCheck) return;
    nextWindowFallbackCheck = now + 1000;

    if (steamWindows.bigPictureIsOpen()) {
      bigPictureObserved = true;
      bigPictureMissingSince = -1;
      return;
    }
    if (!bigPictureObserved) return;
    if (bigPictureMissingSince < 0) {
      bigPictureMissingSince = now;
      return;
    }
    // Steam may briefly rebuild its Chromium window. Require a sustained absence before treating that as an exit.
    if (now - bigPictureMissingSince >= 2500) {
      bigPictureObserved = false;
      bigPictureMissingSince = -1;
      handleBigPictureExit();
    }
  };

  uint64_t lastWakeGeneration = 0;
  bool pendingControllerWake = false;
  qint64 nextBigPictureLaunchAttempt = 0;
  bool launchFailureShown = false;
  qint64 nextControllerScan = 0;

  const auto markDisconnected = [&]() {
    hasBattery = false;
    chargingAnimationClock.invalidate();
    updateTrayIcon();
    percentageAction->setText(QStringLiteral("Steam Controller disconnected."));
    statusAction->setText(QStringLiteral("Status: Disconnected."));
    trayIcon.setToolTip(QStringLiteral("Steam Controller disconnected."));
  };

  const auto attachController = [&]() {
    controller.reset(finder.getController());
    if (controller == nullptr) return false;
    controller->startPoll();
    hasBattery = false;
    statusAction->setText(QStringLiteral("Status: Waiting for battery report..."));
    return true;
  };

  QTimer updateTimer;
  updateTimer.setInterval(250);
  QObject::connect(&updateTimer, &QTimer::timeout, &app, [&]() {
    updateBigPictureState();
    const qint64 now = runTime.elapsed();

    const uint64_t wakeGeneration = controllerActivity.wakeGeneration();
    if (wakeGeneration != lastWakeGeneration) {
      lastWakeGeneration = wakeGeneration;
      pendingControllerWake = openBigPictureOnWake;
      nextBigPictureLaunchAttempt = 0;
      launchFailureShown = false;
    }
    if (!openBigPictureOnWake) pendingControllerWake = false;
    if (pendingControllerWake && bigPictureObserved) pendingControllerWake = false;
    if (pendingControllerWake && now >= nextBigPictureLaunchAttempt) {
      if (openSteamPage(QStringLiteral("bigpicture"))) {
        pendingControllerWake = false;
        launchFailureShown = false;
      } else {
        nextBigPictureLaunchAttempt = now + 3000;
        if (!launchFailureShown) {
          showTrayMessage(QStringLiteral("Could not open Big Picture. Is Steam installed?"),
                          QSystemTrayIcon::Warning);
          launchFailureShown = true;
        }
      }
    }

    if (controller != nullptr && controller->disconnected.load()) {
      controller.reset();
      markDisconnected();
      nextControllerScan = now + 750;
    }

    if (controller == nullptr && now >= nextControllerScan) {
      nextControllerScan = now + 1000;
      if (!attachController()) {
        markDisconnected();
        return;
      }
    }

    if (controller == nullptr) return;
    if (controller->batteryCounter.load() == 0) return;
    const bool wasCharging = hasBattery &&
                             currentBattery.ucChargeState == EChargeState::k_EChargeStateCharging;
    currentBattery = controller->getBatteryStatus();
    hasBattery = true;
    const bool isCharging = currentBattery.ucChargeState == EChargeState::k_EChargeStateCharging;
    if (isCharging && !wasCharging) chargingAnimationClock.restart();
    if (!isCharging) chargingAnimationClock.invalidate();
    const QString percentage = QStringLiteral("Steam Controller battery: %1%").arg(currentBattery.ucBatteryLevel);
    percentageAction->setText(percentage);
    statusAction->setText(getStatusMessage(&currentBattery));
    trayIcon.setToolTip(percentage);
    updateTrayIcon();
  });
  updateTimer.start();

  QTimer chargingAnimationTimer;
  chargingAnimationTimer.setInterval(100);
  QObject::connect(&chargingAnimationTimer, &QTimer::timeout, &app, [&]() {
    if (!hasBattery || currentBattery.ucChargeState != EChargeState::k_EChargeStateCharging ||
        iconAppearance.chargingAnimation == ChargingAnimationMode::Static || iconMode == TrayIconMode::Number) {
      return;
    }
    updateTrayIcon();
  });
  chargingAnimationTimer.start();

#ifdef SANITIZER_BUILD
  QTimer::singleShot(120000, &app, &QCoreApplication::quit);
#endif

  const int result = app.exec();
  controller.reset();
  return result;
}
