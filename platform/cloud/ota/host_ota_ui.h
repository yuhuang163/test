#ifndef HOST_OTA_UI_H
#define HOST_OTA_UI_H
#include "host_ota_service.h"
#include <functional>
#include <QString>

class QWidget;

class HostOtaUi {
  public:
    static void showRestartCountdown(QWidget* parent);
    static bool uploadCurrentExe(QString* message = nullptr, const QString& releaseNotes = QString());
    static bool showVersionPicker(QWidget* parent,
                                  const std::function<void(const QString&)>& logFn = nullptr);
    static bool tryInteractiveUpdate(QWidget* parent,
                                     const std::function<void(const QString&)>& logFn = nullptr);
};
#endif // HOST_OTA_UI_H
