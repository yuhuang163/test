#include "screen_inspect_widget.h"

#include "ui_screen_inspect_widget.h"

#include "Abini.h"
#include "common_utils.h"
#include "screen_inspect_analyzer.h"

#include <QCamera>
#include <QCameraImageCapture>
#include <QCameraInfo>
#include <QCameraViewfinder>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QLabel>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QtConcurrent>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

ScreenInspectWidget::ScreenInspectWidget(QWidget* parent)
    : QWidget(parent), ui(new Ui::ScreenInspectWidget) {
    ui->setupUi(this);
    setObjectName(QStringLiteral("ScreenInspectWidget"));

    ui->comboBox_expectedColor->addItem(QStringLiteral("自动判断"), -1);
    ui->comboBox_expectedColor->addItem(QStringLiteral("蓝"), 0);
    ui->comboBox_expectedColor->addItem(QStringLiteral("绿"), 1);
    ui->comboBox_expectedColor->addItem(QStringLiteral("红"), 2);
    ui->comboBox_expectedColor->addItem(QStringLiteral("白"), 3);
    ui->comboBox_expectedColor->addItem(QStringLiteral("黑"), 4);

    viewfinder_ = new QCameraViewfinder(ui->viewfinderHost);
    ui->verticalLayout_viewfinder->addWidget(viewfinder_);

    connect(ui->doubleSpinBox_minSsim, &QDoubleSpinBox::editingFinished, this,
            &ScreenInspectWidget::saveThresholdsToSettings);
    connect(ui->spinBox_deadDiff, &QSpinBox::editingFinished, this, &ScreenInspectWidget::saveThresholdsToSettings);
    connect(ui->spinBox_maxDead, &QSpinBox::editingFinished, this, &ScreenInspectWidget::saveThresholdsToSettings);
    connect(ui->doubleSpinBox_mura, &QDoubleSpinBox::editingFinished, this,
            &ScreenInspectWidget::saveThresholdsToSettings);

    loadThresholdsFromSettings();
    refreshCameraList();
    loadSavedReferenceIfAny();
}

ScreenInspectWidget::~ScreenInspectWidget() {
    stopPreview();
    delete ui;
}

void ScreenInspectWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    loadThresholdsFromSettings();
}

void ScreenInspectWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    refreshImageLabels();
}

void ScreenInspectWidget::loadThresholdsFromSettings() {
    ui->doubleSpinBox_minSsim->setValue(SETTINGS.value("ScreenInspect/MinSimilarity", 0.90).toDouble());
    ui->spinBox_deadDiff->setValue(SETTINGS.value("ScreenInspect/DeadPixelDiff", 35).toInt());
    ui->spinBox_maxDead->setValue(SETTINGS.value("ScreenInspect/MaxDeadPixels", 8).toInt());
    ui->doubleSpinBox_mura->setValue(SETTINGS.value("ScreenInspect/MuraStdMax", 22.0).toDouble());
    const int camIdx = SETTINGS.value("ScreenInspect/CameraIndex", 0).toInt();
    if (camIdx >= 0 && camIdx < ui->comboBox_camera->count())
        ui->comboBox_camera->setCurrentIndex(camIdx);
}

void ScreenInspectWidget::saveThresholdsToSettings() {
    SETTINGS.setValue("ScreenInspect/MinSimilarity", ui->doubleSpinBox_minSsim->value());
    SETTINGS.setValue("ScreenInspect/DeadPixelDiff", ui->spinBox_deadDiff->value());
    SETTINGS.setValue("ScreenInspect/MaxDeadPixels", ui->spinBox_maxDead->value());
    SETTINGS.setValue("ScreenInspect/MuraStdMax", ui->doubleSpinBox_mura->value());
    SETTINGS.setValue("ScreenInspect/CameraIndex", ui->comboBox_camera->currentIndex());
}

ScreenInspectWidget::InspectParams ScreenInspectWidget::currentParams() const {
    InspectParams p;
    p.minSsim = ui->doubleSpinBox_minSsim->value();
    p.deadDiff = ui->spinBox_deadDiff->value();
    p.maxDeadPixels = ui->spinBox_maxDead->value();
    p.maxMuraStd = ui->doubleSpinBox_mura->value();
    p.expectedColor = ui->comboBox_expectedColor->currentData().toInt();
    return p;
}

void ScreenInspectWidget::refreshCameraList() {
    const int old = ui->comboBox_camera->currentIndex();
    ui->comboBox_camera->clear();
    const QList<QCameraInfo> cams = QCameraInfo::availableCameras();
    for (int i = 0; i < cams.size(); ++i) {
        const QString text = QStringLiteral("%1  %2").arg(i).arg(cams.at(i).description());
        ui->comboBox_camera->addItem(text, cams.at(i).deviceName());
    }
    if (ui->comboBox_camera->count() <= 0) {
        ui->comboBox_camera->addItem(QStringLiteral("未找到摄像头"));
        ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("未找到 USB 摄像头，请检查连接后点刷新。"));
        return;
    }
    const int want = SETTINGS.value("ScreenInspect/CameraIndex", 0).toInt();
    ui->comboBox_camera->setCurrentIndex(qBound(0, want >= 0 ? want : old, ui->comboBox_camera->count() - 1));
}

void ScreenInspectWidget::on_btnRefreshCameras_clicked() {
    refreshCameraList();
}

void ScreenInspectWidget::startPreview() {
    stopPreview();
    const QList<QCameraInfo> cams = QCameraInfo::availableCameras();
    const int idx = ui->comboBox_camera->currentIndex();
    if (idx < 0 || idx >= cams.size()) {
        ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("没有可用摄像头。"));
        return;
    }

    camera_ = new QCamera(cams.at(idx), this);
    capture_ = new QCameraImageCapture(camera_, this);
    camera_->setViewfinder(viewfinder_);
    camera_->setCaptureMode(QCamera::CaptureStillImage);
    if (capture_->isCaptureDestinationSupported(QCameraImageCapture::CaptureToBuffer))
        capture_->setCaptureDestination(QCameraImageCapture::CaptureToBuffer);
    else
        capture_->setCaptureDestination(QCameraImageCapture::CaptureToFile);

    connect(capture_, &QCameraImageCapture::imageCaptured, this, &ScreenInspectWidget::onStillImage);
    connect(capture_,
            static_cast<void (QCameraImageCapture::*)(int, QCameraImageCapture::Error, const QString&)>(
                &QCameraImageCapture::error),
            this, [this](int, QCameraImageCapture::Error, const QString& message) { onCaptureFailed(message); });
    connect(camera_, &QCamera::statusChanged, this, [this](QCamera::Status status) {
        if (status == QCamera::ActiveStatus && captureAfterReady_) {
            captureAfterReady_ = false;
            // 预热几帧，避免首帧发黑/模糊（对应 photo_analysis 读 3 帧）
            QTimer::singleShot(450, this, &ScreenInspectWidget::requestCapture);
        }
    });

    SETTINGS.setValue("ScreenInspect/CameraIndex", idx);
    camera_->start();
    ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("已打开预览：") + cams.at(idx).description());
}

void ScreenInspectWidget::stopPreview() {
    captureAfterReady_ = false;
    if (capture_) {
        capture_->disconnect(this);
        delete capture_;
        capture_ = nullptr;
    }
    if (camera_) {
        camera_->stop();
        delete camera_;
        camera_ = nullptr;
    }
}

void ScreenInspectWidget::on_btnOpenPreview_clicked() {
    startPreview();
}

void ScreenInspectWidget::on_btnClosePreview_clicked() {
    stopPreview();
    ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("已关闭预览。"));
}

void ScreenInspectWidget::requestCapture() {
    if (!camera_ || !capture_) {
        captureAfterReady_ = true;
        startPreview();
        return;
    }
    if (camera_->status() != QCamera::ActiveStatus) {
        captureAfterReady_ = true;
        return;
    }
    if (!capture_->isReadyForCapture()) {
        QTimer::singleShot(150, this, &ScreenInspectWidget::requestCapture);
        return;
    }
    if (capture_->captureDestination() == QCameraImageCapture::CaptureToFile) {
        const QString tmp = QDir::temp().filePath(QStringLiteral("screen_inspect_cap.png"));
        capture_->capture(tmp);
    } else {
        capture_->capture();
    }
}

void ScreenInspectWidget::on_btnCapture_clicked() {
    inspectAfterCapture_ = ui->checkBox_autoInspect->isChecked();
    setBusy(true);
    ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("正在采集（预热后抓拍）…"));
    requestCapture();
}

void ScreenInspectWidget::onStillImage(int, const QImage& image) {
    setBusy(false);
    if (image.isNull()) {
        ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("采集失败：空画面。"));
        return;
    }
    currImage_ = image.convertToFormat(QImage::Format_RGB888);
    annotatedImage_ = currImage_;
    refreshImageLabels();
    saveCaptureFiles(currImage_, QImage());
    ui->plainTextEdit_screenInspectLog->setPlainText(
        QStringLiteral("采集成功 %1x%2").arg(currImage_.width()).arg(currImage_.height()));
    if (inspectAfterCapture_) {
        inspectAfterCapture_ = false;
        runInspect();
    }
}

void ScreenInspectWidget::onCaptureFailed(const QString& message) {
    setBusy(false);
    inspectAfterCapture_ = false;
    ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("采集失败：") + message);
}

void ScreenInspectWidget::on_btnLoadRef_clicked() {
    const QString startDir = inspectDir();
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择参考屏幕图片"), startDir,
                                                      QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty())
        return;
    QImage img(path);
    if (img.isNull()) {
        ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("参考图加载失败。"));
        return;
    }
    applyReferenceImage(img, path);
    ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("已加载参考图：") + path);
}

void ScreenInspectWidget::on_btnSaveAsRef_clicked() {
    if (currImage_.isNull()) {
        ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("请先采集当前图，再保存为参考图。"));
        return;
    }
    const QString dir = inspectDir();
    CommonUtils::ensureDirectory(dir);
    const QString path = dir + QLatin1Char('/') + QStringLiteral("reference.png");
    if (!currImage_.save(path, "PNG")) {
        ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("参考图保存失败：") + path);
        return;
    }
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    currImage_.save(dir + QLatin1Char('/') + stamp + QStringLiteral("_reference.png"), "PNG");
    applyReferenceImage(currImage_, path);
    ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("当前图已保存为参考图：") + path);
}

void ScreenInspectWidget::on_btnInspect_clicked() {
    runInspect();
}

void ScreenInspectWidget::on_btnColorBlue_clicked() {
    ui->comboBox_expectedColor->setCurrentIndex(ui->comboBox_expectedColor->findData(0));
}

void ScreenInspectWidget::on_btnColorGreen_clicked() {
    ui->comboBox_expectedColor->setCurrentIndex(ui->comboBox_expectedColor->findData(1));
}

void ScreenInspectWidget::on_btnColorRed_clicked() {
    ui->comboBox_expectedColor->setCurrentIndex(ui->comboBox_expectedColor->findData(2));
}

void ScreenInspectWidget::on_btnColorWhite_clicked() {
    ui->comboBox_expectedColor->setCurrentIndex(ui->comboBox_expectedColor->findData(3));
}

void ScreenInspectWidget::on_btnColorBlack_clicked() {
    ui->comboBox_expectedColor->setCurrentIndex(ui->comboBox_expectedColor->findData(4));
}

void ScreenInspectWidget::setBusy(bool busy) {
    ui->btnCapture->setEnabled(!busy);
    ui->btnInspect->setEnabled(!busy);
    ui->btnOpenPreview->setEnabled(!busy);
    ui->btnCapture->setText(busy ? QStringLiteral("正在采集...") : QStringLiteral("采集当前图"));
}

void ScreenInspectWidget::showPixmapOnLabel(const QImage& image, QLabel* label) {
    if (!label)
        return;
    if (image.isNull()) {
        label->setPixmap(QPixmap());
        return;
    }
    const QSize sz = label->size();
    if (sz.width() < 8 || sz.height() < 8)
        return;
    label->setPixmap(QPixmap::fromImage(image).scaled(sz, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void ScreenInspectWidget::refreshImageLabels() {
    if (!refImage_.isNull())
        showPixmapOnLabel(refImage_, ui->label_refImage);
    const QImage& show = annotatedImage_.isNull() ? currImage_ : annotatedImage_;
    if (!show.isNull())
        showPixmapOnLabel(show, ui->label_currImage);
}

void ScreenInspectWidget::runInspect() {
    if (currImage_.isNull()) {
        ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("请先采集当前图。"));
        return;
    }
    if (inspectRunning_)
        return;
    inspectRunning_ = true;
    setBusy(true);
    saveThresholdsToSettings();
    const InspectParams p = currentParams();
    const QImage curr = currImage_.copy();
    const QImage ref = refImage_.copy();
    QtConcurrent::run([this, curr, ref, p]() {
        const InspectReport report = analyze(curr, ref, p);
        QMetaObject::invokeMethod(
            this,
            [this, report]() {
                inspectRunning_ = false;
                setBusy(false);
                applyReport(report);
            },
            Qt::QueuedConnection);
    });
}

ScreenInspectWidget::InspectReport ScreenInspectWidget::analyze(const QImage& currRgb, const QImage& refRgb,
                                                               const InspectParams& p) {
    ScreenInspectAnalyzer::Params ap;
    ap.deadDiff = p.deadDiff;
    ap.expectedColor = p.expectedColor;
    const ScreenInspectAnalyzer::Report raw = ScreenInspectAnalyzer::analyze(currRgb, refRgb, ap);

    InspectReport report;
    report.ssim = raw.ssim;
    report.deadPixels = raw.deadPixels;
    report.muraStd = raw.muraStd;
    report.roi = raw.roi;
    report.annotated = raw.annotated;

    QStringList reasons;
    bool ok = true;
    if (report.deadPixels > p.maxDeadPixels) {
        ok = false;
        reasons.append(QStringLiteral("坏点%1超过上限%2").arg(report.deadPixels).arg(p.maxDeadPixels));
    }
    if (report.muraStd > p.maxMuraStd) {
        ok = false;
        reasons.append(QStringLiteral("亮度不均%1超过上限%2")
                           .arg(report.muraStd, 0, 'f', 1)
                           .arg(p.maxMuraStd, 0, 'f', 1));
    }
    if (report.ssim >= 0 && report.ssim < p.minSsim) {
        ok = false;
        reasons.append(QStringLiteral("相似度%1低于%2")
                           .arg(report.ssim, 0, 'f', 3)
                           .arg(p.minSsim, 0, 'f', 2));
    }

    report.pass = ok;
    if (ok)
        report.summary = QStringLiteral("正常：坏点%1，亮度不均%2")
                             .arg(report.deadPixels)
                             .arg(report.muraStd, 0, 'f', 1);
    else
        report.summary = QStringLiteral("异常：") + reasons.join(QStringLiteral("；"));
    if (refRgb.isNull())
        report.summary += QStringLiteral("（未加载参考图，未比相似度）");
    return report;
}

void ScreenInspectWidget::applyReport(const InspectReport& report) {
    annotatedImage_ = report.annotated;
    refreshImageLabels();
    saveCaptureFiles(currImage_, annotatedImage_);

    if (report.ssim >= 0)
        ui->label_screenInspectSimilarity->setText(
            QStringLiteral("相似度：%1%  坏点：%2  亮度不均：%3")
                .arg(report.ssim * 100.0, 0, 'f', 2)
                .arg(report.deadPixels)
                .arg(report.muraStd, 0, 'f', 1));
    else
        ui->label_screenInspectSimilarity->setText(
            QStringLiteral("相似度：未比（无参考图）  坏点：%1  亮度不均：%2")
                .arg(report.deadPixels)
                .arg(report.muraStd, 0, 'f', 1));

    ui->label_screenInspectVerdict->setText(report.pass ? QStringLiteral("结果：正常")
                                                        : QStringLiteral("结果：异常"));
    // 结论色随检测结果变化，无法写死在 QSS
    ui->label_screenInspectVerdict->setStyleSheet(
        report.pass ? QStringLiteral("font-size:22px;font-weight:bold;color:#0a7a0a;")
                    : QStringLiteral("font-size:22px;font-weight:bold;color:#c00000;"));
    ui->plainTextEdit_screenInspectLog->setPlainText(
        report.summary + QStringLiteral("\nROI=%1,%2 %3x%4")
                             .arg(report.roi.x())
                             .arg(report.roi.y())
                             .arg(report.roi.width())
                             .arg(report.roi.height()));
}

void ScreenInspectWidget::saveCaptureFiles(const QImage& raw, const QImage& annotated) const {
    const QString dir = inspectDir();
    CommonUtils::ensureDirectory(dir);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    if (!raw.isNull())
        raw.save(dir + QLatin1Char('/') + stamp + QStringLiteral("_capture.png"), "PNG");
    if (!annotated.isNull())
        annotated.save(dir + QLatin1Char('/') + stamp + QStringLiteral("_mark.png"), "PNG");
}

QString ScreenInspectWidget::inspectDir() const {
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("screen_inspect"));
}

void ScreenInspectWidget::applyReferenceImage(const QImage& img, const QString& sourcePath) {
    refImage_ = img.convertToFormat(QImage::Format_RGB888);
    refreshImageLabels();
    if (!sourcePath.isEmpty())
        SETTINGS.setValue(QStringLiteral("ScreenInspect/ReferencePath"), sourcePath);
}

void ScreenInspectWidget::loadSavedReferenceIfAny() {
    QString path = SETTINGS.value(QStringLiteral("ScreenInspect/ReferencePath")).toString();
    if (path.isEmpty())
        path = inspectDir() + QLatin1Char('/') + QStringLiteral("reference.png");
    QImage img(path);
    if (img.isNull())
        return;
    applyReferenceImage(img, path);
    ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("已载入上次参考图：") + path);
}
