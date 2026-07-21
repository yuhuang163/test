#include "label_print_service.h"

#include "my_set/my_typedef.h"
#include "qrcodegen.hpp"

#include <QComboBox>
#include <QSignalBlocker>
#include <QImage>
#include <QPainter>
#include <QPageSize>
#include <QPrinter>
#include <QPrinterInfo>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

LabelPrintService::Config defaultConfig() {
    LabelPrintService::Config cfg;
    cfg.qrModulePixels = 24;
    cfg.pageWidthMm = 10.0;
    cfg.pageHeightMm = 10.0;
    cfg.marginPixels = -1;
    return cfg;
}

QImage makeQrImage(const QString& text, int modulePixels, QString* errorMessage) {
    if (text.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("打印内容为空");
        return {};
    }
    try {
        const qrcodegen::QrCode qr =
            qrcodegen::QrCode::encodeText(text.trimmed().toUtf8().constData(), qrcodegen::QrCode::Ecc::LOW);
        const int modules = qr.getSize();
        const int pixelSize = qMax(1, modulePixels);
        const int imageSize = modules * pixelSize;
        QImage image(imageSize, imageSize, QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black);
        for (int y = 0; y < modules; ++y) {
            for (int x = 0; x < modules; ++x) {
                if (qr.getModule(x, y))
                    painter.fillRect(x * pixelSize, y * pixelSize, pixelSize, pixelSize, Qt::black);
            }
        }
        return image;
    } catch (const std::exception& ex) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8(ex.what());
        return {};
    }
}

bool printImage(const LabelPrintService::Config& config, const QImage& image, QString* errorMessage) {
    if (config.printerName.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("未选择打印机，请在功能设置中配置");
        return false;
    }
    if (image.isNull()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("二维码生成失败");
        return false;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setPrinterName(config.printerName.trimmed());
    printer.setCopyCount(1);
    printer.setFullPage(true);
    printer.setPageSize(QPageSize(QSizeF(config.pageWidthMm, config.pageHeightMm), QPageSize::Millimeter));

    QPainter painter;
    if (!painter.begin(&printer)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法连接打印机：%1").arg(config.printerName);
        return false;
    }

    const QRectF pageRect = printer.pageRect(QPrinter::DevicePixel);
    qreal drawX = 0;
    qreal drawY = 0;
    if (config.marginPixels >= 0) {
        drawX = config.marginPixels;
        drawY = config.marginPixels;
    } else {
        drawX = pageRect.x() + qMax<qreal>(0, (pageRect.width() - image.width()) / 2.0);
        drawY = pageRect.y() + qMax<qreal>(0, (pageRect.height() - image.height()) / 2.0);
    }
    painter.drawImage(QPointF(drawX, drawY), image);
    painter.end();
    return true;
}

} // namespace

LabelPrintService::Config LabelPrintService::loadFromSettings() {
    Config cfg = defaultConfig();
    cfg.printerName = SETTINGS.value(QStringLiteral("Printer/WindowsName")).toString().trimmed();
    cfg.qrModulePixels = SETTINGS.value(QStringLiteral("Printer/QrModulePixels"), cfg.qrModulePixels).toInt();
    cfg.pageWidthMm = SETTINGS.value(QStringLiteral("Printer/PageWidthMm"), cfg.pageWidthMm).toDouble();
    cfg.pageHeightMm = SETTINGS.value(QStringLiteral("Printer/PageHeightMm"), cfg.pageHeightMm).toDouble();
    cfg.marginPixels = SETTINGS.value(QStringLiteral("Printer/MarginPixels"), cfg.marginPixels).toInt();
    cfg.qrModulePixels = qBound(4, cfg.qrModulePixels, 128);
    cfg.pageWidthMm = qBound(5.0, cfg.pageWidthMm, 200.0);
    cfg.pageHeightMm = qBound(5.0, cfg.pageHeightMm, 200.0);
    return cfg;
}

void LabelPrintService::saveToSettings(const Config& config) {
    Config cfg = config;
    cfg.qrModulePixels = qBound(4, cfg.qrModulePixels, 128);
    cfg.pageWidthMm = qBound(5.0, cfg.pageWidthMm, 200.0);
    cfg.pageHeightMm = qBound(5.0, cfg.pageHeightMm, 200.0);
    SETTINGS.setValue(QStringLiteral("Printer/WindowsName"), cfg.printerName.trimmed());
    SETTINGS.setValue(QStringLiteral("Printer/QrModulePixels"), cfg.qrModulePixels);
    SETTINGS.setValue(QStringLiteral("Printer/PageWidthMm"), cfg.pageWidthMm);
    SETTINGS.setValue(QStringLiteral("Printer/PageHeightMm"), cfg.pageHeightMm);
    SETTINGS.setValue(QStringLiteral("Printer/MarginPixels"), cfg.marginPixels);
    SETTINGS.sync();
}

void LabelPrintService::populatePrinterCombo(QComboBox* combo, const QString& selectedName) {
    if (!combo)
        return;
    QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(QStringLiteral("请选择打印机"), QString());
    for (const QPrinterInfo& info : QPrinterInfo::availablePrinters()) {
        const QString name = info.printerName();
        if (!name.isEmpty())
            combo->addItem(name, name);
    }
    const int idx = combo->findData(selectedName.trimmed());
    combo->setCurrentIndex(idx >= 0 ? idx : 0);
}

bool LabelPrintService::printQrText(const QString& text, QString* errorMessage) {
    const Config config = loadFromSettings();
    const QImage image = makeQrImage(text, config.qrModulePixels, errorMessage);
    if (image.isNull())
        return false;
    return printImage(config, image, errorMessage);
}

bool LabelPrintService::printTestQr(QString* errorMessage) {
    return printQrText(QStringLiteral("PRINT_TEST"), errorMessage);
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
