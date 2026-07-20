#ifndef LABEL_PRINT_SERVICE_H
#define LABEL_PRINT_SERVICE_H

#include <QString>

class QComboBox;

/** 整机 SN 二维码标签打印（Windows QPrinter + QR） */
class LabelPrintService {
  public:
    struct Config {
        QString printerName;
        int qrModulePixels = 24;
        double pageWidthMm = 10.0;
        double pageHeightMm = 10.0;
        /** 小于 0 表示页面居中；>=0 为左上角边距（设备像素） */
        int marginPixels = -1;
    };

    static Config loadFromSettings();
    static void saveToSettings(const Config& config);

    static void populatePrinterCombo(QComboBox* combo, const QString& selectedName);
    static bool printQrText(const QString& text, QString* errorMessage = nullptr);
    static bool printTestQr(QString* errorMessage = nullptr);
};

#endif // LABEL_PRINT_SERVICE_H
