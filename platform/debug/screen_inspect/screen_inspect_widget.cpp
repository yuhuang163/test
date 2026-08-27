#include "screen_inspect_widget.h"

#include "Abini.h"
#include "common_utils.h"
#include "screen_inspect_analyzer.h"
#include "screen_inspect_gige_capture.h"

#include <QCamera>
#include <QCameraImageCapture>
#include <QCameraInfo>
#include <QCameraViewfinder>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QFileDialog>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

struct ScreenInspectUi {
    QComboBox* comboBox_camera = nullptr;
    QComboBox* comboBox_expectedColor = nullptr;
    QWidget* viewfinderHost = nullptr;
    QVBoxLayout* verticalLayout_viewfinder = nullptr;
    QDoubleSpinBox* doubleSpinBox_minSsim = nullptr;
    QSpinBox* spinBox_deadDiff = nullptr;
    QSpinBox* spinBox_maxDead = nullptr;
    QDoubleSpinBox* doubleSpinBox_mura = nullptr;
    QLabel* label_currImage = nullptr;
    QLabel* label_refImage = nullptr;
    QLabel* label_screenInspectSimilarity = nullptr;
    QLabel* label_screenInspectVerdict = nullptr;
    QPlainTextEdit* plainTextEdit_screenInspectLog = nullptr;
    QCheckBox* checkBox_autoInspect = nullptr;
    QPushButton* btnCapture = nullptr;
    QPushButton* btnInspect = nullptr;
    QPushButton* btnOpenPreview = nullptr;
};

template <typename T>
T* screenInspectFind(QWidget* root, const char* name) {
    if (!root)
        return nullptr;
    return root->findChild<T*>(QString::fromUtf8(name));
}

ScreenInspectWidget::ScreenInspectWidget(QWidget* parent)
    : QWidget(parent) {
}

void ScreenInspectWidget::bindDesignerUi() {
    if (uiBound_)
        return;
    ui = new ScreenInspectUi;
    ui->comboBox_camera = screenInspectFind<QComboBox>(this, "comboBox_camera");
    ui->comboBox_expectedColor = screenInspectFind<QComboBox>(this, "comboBox_expectedColor");
    ui->viewfinderHost = screenInspectFind<QWidget>(this, "viewfinderHost");
    ui->verticalLayout_viewfinder = screenInspectFind<QVBoxLayout>(this, "verticalLayout_viewfinder");
    if (!ui->verticalLayout_viewfinder && ui->viewfinderHost)
        ui->verticalLayout_viewfinder = qobject_cast<QVBoxLayout*>(ui->viewfinderHost->layout());
    if (!ui->viewfinderHost && ui->verticalLayout_viewfinder)
        ui->viewfinderHost = ui->verticalLayout_viewfinder->parentWidget();
    ui->doubleSpinBox_minSsim = screenInspectFind<QDoubleSpinBox>(this, "doubleSpinBox_minSsim");
    ui->spinBox_deadDiff = screenInspectFind<QSpinBox>(this, "spinBox_deadDiff");
    ui->spinBox_maxDead = screenInspectFind<QSpinBox>(this, "spinBox_maxDead");
    ui->doubleSpinBox_mura = screenInspectFind<QDoubleSpinBox>(this, "doubleSpinBox_mura");
    ui->label_currImage = screenInspectFind<QLabel>(this, "label_currImage");
    ui->label_refImage = screenInspectFind<QLabel>(this, "label_refImage");
    ui->label_screenInspectSimilarity = screenInspectFind<QLabel>(this, "label_screenInspectSimilarity");
    ui->label_screenInspectVerdict = screenInspectFind<QLabel>(this, "label_screenInspectVerdict");
    ui->plainTextEdit_screenInspectLog = screenInspectFind<QPlainTextEdit>(this, "plainTextEdit_screenInspectLog");
    ui->checkBox_autoInspect = screenInspectFind<QCheckBox>(this, "checkBox_autoInspect");
    ui->btnCapture = screenInspectFind<QPushButton>(this, "btnCapture");
    ui->btnInspect = screenInspectFind<QPushButton>(this, "btnInspect");
    ui->btnOpenPreview = screenInspectFind<QPushButton>(this, "btnOpenPreview");
    if (!ui->comboBox_camera || !ui->comboBox_expectedColor || !ui->viewfinderHost || !ui->verticalLayout_viewfinder
        || !ui->doubleSpinBox_minSsim || !ui->spinBox_deadDiff || !ui->spinBox_maxDead || !ui->doubleSpinBox_mura
        || !ui->label_currImage || !ui->label_refImage || !ui->label_screenInspectSimilarity
        || !ui->label_screenInspectVerdict || !ui->plainTextEdit_screenInspectLog || !ui->checkBox_autoInspect
        || !ui->btnCapture || !ui->btnInspect || !ui->btnOpenPreview) {
        qWarning() << "ScreenInspectWidget: 界面控件未绑定完整，跳过屏幕测试页初始化";
        return;
    }

    ui->comboBox_expectedColor->addItem(QStringLiteral("自动判断"), -1);
    ui->comboBox_expectedColor->addItem(QStringLiteral("蓝"), 0);
    ui->comboBox_expectedColor->addItem(QStringLiteral("绿"), 1);
    ui->comboBox_expectedColor->addItem(QStringLiteral("红"), 2);
    ui->comboBox_expectedColor->addItem(QStringLiteral("白"), 3);
    ui->comboBox_expectedColor->addItem(QStringLiteral("黑"), 4);
    ui->comboBox_expectedColor->addItem(QStringLiteral("灰"), 5);

    viewfinder_ = new QCameraViewfinder(ui->viewfinderHost);
    viewfinder_->setMaximumHeight(ui->viewfinderHost->maximumHeight());
    viewfinder_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->verticalLayout_viewfinder->addWidget(viewfinder_);

    connect(ui->doubleSpinBox_minSsim, &QDoubleSpinBox::editingFinished, this,
            &ScreenInspectWidget::saveThresholdsToSettings);
    connect(ui->spinBox_deadDiff, &QSpinBox::editingFinished, this, &ScreenInspectWidget::saveThresholdsToSettings);
    connect(ui->spinBox_maxDead, &QSpinBox::editingFinished, this, &ScreenInspectWidget::saveThresholdsToSettings);
    connect(ui->doubleSpinBox_mura, &QDoubleSpinBox::editingFinished, this,
            &ScreenInspectWidget::saveThresholdsToSettings);

    loadThresholdsFromSettings();
    updateCameraSourceUi();
    refreshCameraList();
    loadSavedReferenceIfAny();
    ScreenInspectAnalyzer::cleanupStoredImages(inspectDir());

    ui->label_currImage->installEventFilter(this);
    ui->label_currImage->setMouseTracking(true);
    ui->label_currImage->setCursor(Qt::CrossCursor);
    manualRoi_ = ScreenInspectAnalyzer::parseManualRoi(
        SETTINGS.value(QStringLiteral("ScreenInspect/Roi")).toString());

    QMetaObject::connectSlotsByName(this);
    uiBound_ = true;
}

ScreenInspectWidget::~ScreenInspectWidget() {
    if (ui && ui->label_currImage)
        ui->label_currImage->releaseMouse();
    stopPreview();
    delete ui;
}

void ScreenInspectWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    loadThresholdsFromSettings();
    updateCameraSourceUi();
}

void ScreenInspectWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (uiBound_)
        refreshImageLabels();
}

void ScreenInspectWidget::loadThresholdsFromSettings() {
    ui->doubleSpinBox_minSsim->setValue(SETTINGS.value("ScreenInspect/MinSimilarity", 0.90).toDouble());
    ui->spinBox_deadDiff->setValue(SETTINGS.value("ScreenInspect/DeadPixelDiff", 35).toInt());
    ui->spinBox_maxDead->setValue(SETTINGS.value("ScreenInspect/MaxDeadPixels", 8).toInt());
    ui->doubleSpinBox_mura->setValue(SETTINGS.value("ScreenInspect/MuraStdMax", 22.0).toDouble());
    const QString src = SETTINGS.value(QStringLiteral("ScreenInspect/CameraSource"), QStringLiteral("usb"))
                            .toString()
                            .trimmed()
                            .toLower();
    ui->comboBox_cameraSource->setCurrentIndex(src == QLatin1String("gige") ? 1 : 0);
    ui->lineEdit_gigeIp->setText(
        SETTINGS.value(QStringLiteral("ScreenInspect/GigEIp"), QStringLiteral("169.254.64.10")).toString());
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
    SETTINGS.setValue(QStringLiteral("ScreenInspect/CameraSource"),
                      isGigESource() ? QStringLiteral("gige") : QStringLiteral("usb"));
    SETTINGS.setValue(QStringLiteral("ScreenInspect/GigEIp"), ui->lineEdit_gigeIp->text().trimmed());
}

bool ScreenInspectWidget::isGigESource() const {
    return ui->comboBox_cameraSource->currentIndex() == 1;
}

void ScreenInspectWidget::updateCameraSourceUi() {
    const bool gige = isGigESource();
    ui->label_camera->setVisible(!gige);
    ui->comboBox_camera->setVisible(!gige);
    ui->label_gigeIp->setVisible(gige);
    ui->lineEdit_gigeIp->setVisible(gige);
    ui->btnOpenPreview->setText(gige ? QStringLiteral("测试采图") : QStringLiteral("打开预览"));
    ui->btnClosePreview->setEnabled(!gige);
    if (viewfinder_)
        viewfinder_->setVisible(!gige);
    if (gige)
        stopPreview();
}

void ScreenInspectWidget::on_comboBox_cameraSource_currentIndexChanged(int) {
    updateCameraSourceUi();
    saveThresholdsToSettings();
    if (!isGigESource())
        refreshCameraList();
}

ScreenInspectWidget::InspectParams ScreenInspectWidget::currentParams() const {
    InspectParams p;
    p.minSsim = ui->doubleSpinBox_minSsim->value();
    p.deadDiff = ui->spinBox_deadDiff->value();
    p.maxDeadPixels = ui->spinBox_maxDead->value();
    p.maxMuraStd = ui->doubleSpinBox_mura->value();
    p.expectedColor = ui->comboBox_expectedColor->currentData().toInt();
    p.manualRoi = manualRoi_;
    return p;
}

void ScreenInspectWidget::refreshCameraList() {
    if (isGigESource()) {
        ui->plainTextEdit_screenInspectLog->setPlainText(
            QStringLiteral("当前为 GigE：填写 IP 后点「测试采图」或「采集」。请先关闭 MVS 客户端以免占用相机。"));
        return;
    }
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
    if (isGigESource()) {
        inspectAfterCapture_ = false;
        captureGigEStill();
        return;
    }
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
    connect(capture_, &QCameraImageCapture::imageSaved, this, [this](int, const QString& path) {
        if (busy_)
            onStillImage(0, QImage(path));
    });
    connect(capture_,
            static_cast<void (QCameraImageCapture::*)(int, QCameraImageCapture::Error, const QString&)>(
                &QCameraImageCapture::error),
            this, [this](int, QCameraImageCapture::Error, const QString& message) { onCaptureFailed(message); });
    connect(camera_, &QCamera::statusChanged, this, [this](QCamera::Status status) {
        if (status == QCamera::ActiveStatus && captureAfterReady_) {
            captureAfterReady_ = false;
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

void ScreenInspectWidget::captureGigEStill() {
    saveThresholdsToSettings();
    setBusy(true);
    const QString ip = ui->lineEdit_gigeIp->text().trimmed();
    ui->plainTextEdit_screenInspectLog->setPlainText(
        QStringLiteral("GigE 采图中：%1 …（后台线程，请关闭 MVS 客户端）\nUI线程=%2")
            .arg(ip.isEmpty() ? QStringLiteral("(自动第一台)") : ip)
            .arg(quintptr(QThread::currentThreadId()), 0, 16));
    // 枚举/开流/预热会阻塞数秒，放到后台以免调试页卡死
    QtConcurrent::run([this, ip]() {
        QElapsedTimer wall;
        wall.start();
        QImage img;
        QString err;
        QString stages;
        const bool ok = ScreenInspectGigECapture::grabStill(ip, QString(), 200, &img, &err, &stages);
        const qint64 wallMs = wall.elapsed();
        QMetaObject::invokeMethod(
            this,
            [this, ok, img, err, stages, wallMs]() {
                if (!ok) {
                    onCaptureFailed(err + QStringLiteral("\n") + stages);
                    return;
                }
                onStillImage(0, img);
                ui->plainTextEdit_screenInspectLog->appendPlainText(
                    QStringLiteral("[耗时] GigE墙钟=%1ms\n%2").arg(wallMs).arg(stages));
            },
            Qt::QueuedConnection);
    });
}

void ScreenInspectWidget::requestCapture() {
    if (isGigESource()) {
        captureGigEStill();
        return;
    }
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
    if (isGigESource()) {
        ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("正在 GigE 采集…"));
        captureGigEStill();
        return;
    }
    ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("正在采集（预热后抓拍）…"));
    requestCapture();
}

void ScreenInspectWidget::onStillImage(int, const QImage& image) {
    QElapsedTimer t;
    t.start();
    setBusy(false);
    if (image.isNull()) {
        ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("采集失败：空画面。"));
        return;
    }
    currImage_ = image.convertToFormat(QImage::Format_RGB888);
    const qint64 msConv = t.restart();
    annotatedImage_ = currImage_;
    refreshImageLabels();
    const qint64 msUi = t.restart();
    // PNG 压缩高分辨率图很慢，勿堵 UI
    saveCaptureFiles(currImage_, QImage());
    const qint64 msSaveKick = t.elapsed();
    const QString uiLog =
        QStringLiteral("采集成功 %1x%2\n[耗时] UI线程 convert=%3ms 刷新预览=%4ms 触发存盘=%5ms")
            .arg(currImage_.width())
            .arg(currImage_.height())
            .arg(msConv)
            .arg(msUi)
            .arg(msSaveKick);
    qDebug().noquote() << QStringLiteral("[ScreenInspectUI]") << uiLog;
    ui->plainTextEdit_screenInspectLog->setPlainText(uiLog);
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

void ScreenInspectWidget::on_btnClearRoi_clicked() {
    manualRoi_ = QRect();
    roiDragging_ = false;
    SETTINGS.setValue(QStringLiteral("ScreenInspect/Roi"), QString());
    refreshImageLabels();
    ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("已清除划定范围，检测改回自动找屏。"));
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
    busy_ = busy;
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
    // 先按控件尺寸缩小再转 QPixmap，避免整幅高分辨率进 UI
    const Qt::TransformationMode mode =
        (image.width() * image.height() > 1920 * 1080) ? Qt::FastTransformation : Qt::SmoothTransformation;
    const QImage scaled = image.scaled(sz, Qt::KeepAspectRatio, mode);
    label->setPixmap(QPixmap::fromImage(scaled));
}

/** 在缩略显示图上叠划定 ROI（坐标按原图像素换算）。 */
QImage paintRoiOverlay(const QImage& src, const QSize& labelSize, const QRect& roiImage) {
    if (src.isNull() || labelSize.width() < 8 || labelSize.height() < 8)
        return src;
    const Qt::TransformationMode mode =
        (src.width() * src.height() > 1920 * 1080) ? Qt::FastTransformation : Qt::SmoothTransformation;
    QImage display = src.scaled(labelSize, Qt::KeepAspectRatio, mode);
    if (display.isNull())
        return display;
    QRect overlay = roiImage.intersected(src.rect());
    if (overlay.width() < 4 || overlay.height() < 4 || src.width() <= 0 || src.height() <= 0)
        return display;
    const QRect od(overlay.x() * display.width() / src.width(),
                   overlay.y() * display.height() / src.height(),
                   qMax(1, overlay.width() * display.width() / src.width()),
                   qMax(1, overlay.height() * display.height() / src.height()));
    display = display.convertToFormat(QImage::Format_RGB32);
    QPainter p(&display);
    p.setPen(QPen(QColor(0, 220, 255), 3));
    p.drawRect(od.adjusted(0, 0, -1, -1));
    p.end();
    return display;
}

void ScreenInspectWidget::refreshImageLabels() {
    QRect overlay;
    if (roiDragging_)
        overlay = QRect(roiDragStart_, roiDragCur_).normalized();
    else
        overlay = manualRoi_;

    // 参考图只显示原图，不叠坏点/划定框（坏点只标在拍摄图上）
    if (!refImage_.isNull())
        showPixmapOnLabel(refImage_, ui->label_refImage);

    const QImage src = annotatedImage_.isNull() ? currImage_ : annotatedImage_;
    if (src.isNull() || !ui->label_currImage)
        return;
    const QSize sz = ui->label_currImage->size();
    if (sz.width() < 8 || sz.height() < 8)
        return;
    // 拍摄图：分析结果（绿框/圆/红圈）+ 青色划定框
    ui->label_currImage->setPixmap(QPixmap::fromImage(paintRoiOverlay(src, sz, overlay)));
}

QRect ScreenInspectWidget::labelPosToImage(const QPoint& pos) const {
    if (currImage_.isNull() || !ui->label_currImage->pixmap() || ui->label_currImage->pixmap()->isNull())
        return QRect();
    const QPixmap pm = *ui->label_currImage->pixmap();
    const QRect cr = ui->label_currImage->contentsRect();
    const int x0 = cr.x() + (cr.width() - pm.width()) / 2;
    const int y0 = cr.y() + (cr.height() - pm.height()) / 2;
    const QRect target(x0, y0, pm.width(), pm.height());
    if (!target.contains(pos) || pm.width() < 1 || pm.height() < 1)
        return QRect();
    const int ix = (pos.x() - x0) * currImage_.width() / pm.width();
    const int iy = (pos.y() - y0) * currImage_.height() / pm.height();
    return QRect(ix, iy, 1, 1);
}

void ScreenInspectWidget::saveManualRoi(const QRect& r) {
    const QRect clipped = r.normalized().intersected(currImage_.rect());
    if (clipped.width() < 8 || clipped.height() < 8) {
        ui->plainTextEdit_screenInspectLog->setPlainText(QStringLiteral("划定范围太小，请在拍摄图上拖出更大的框。"));
        return;
    }
    manualRoi_ = clipped;
    SETTINGS.setValue(QStringLiteral("ScreenInspect/Roi"), ScreenInspectAnalyzer::formatManualRoi(manualRoi_));
    ui->plainTextEdit_screenInspectLog->setPlainText(
        QStringLiteral("已划定检测范围：%1,%2 %3x%4（工站步骤共用此范围）")
            .arg(manualRoi_.x())
            .arg(manualRoi_.y())
            .arg(manualRoi_.width())
            .arg(manualRoi_.height()));
}

bool ScreenInspectWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == ui->label_currImage && !currImage_.isNull()) {
        if (event->type() == QEvent::MouseButtonPress) {
            const auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                const QRect hit = labelPosToImage(me->pos());
                if (hit.isValid()) {
                    roiDragging_ = true;
                    roiDragStart_ = hit.topLeft();
                    roiDragCur_ = roiDragStart_;
                    ui->label_currImage->grabMouse();
                    refreshImageLabels();
                    return true;
                }
            }
        } else if (event->type() == QEvent::MouseMove && roiDragging_) {
            const auto* me = static_cast<QMouseEvent*>(event);
            const QRect hit = labelPosToImage(me->pos());
            if (hit.isValid()) {
                roiDragCur_ = hit.topLeft();
                refreshImageLabels();
            }
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && roiDragging_) {
            const auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                const QRect hit = labelPosToImage(me->pos());
                if (hit.isValid())
                    roiDragCur_ = hit.topLeft();
                roiDragging_ = false;
                ui->label_currImage->releaseMouse();
                saveManualRoi(QRect(roiDragStart_, roiDragCur_));
                refreshImageLabels();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
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
    ui->plainTextEdit_screenInspectLog->setPlainText(
        QStringLiteral("正在识别 %1x%2（后台线程）…").arg(curr.width()).arg(curr.height()));
    QtConcurrent::run([this, curr, ref, p]() {
        QElapsedTimer t;
        t.start();
        const InspectReport report = analyze(curr, ref, p);
        const qint64 ms = t.elapsed();
        QMetaObject::invokeMethod(
            this,
            [this, report, ms]() {
                inspectRunning_ = false;
                setBusy(false);
                applyReport(report);
                ui->plainTextEdit_screenInspectLog->appendPlainText(
                    QStringLiteral("[耗时] 识别墙钟=%1ms（详见进程后台 log 中 [ScreenInspectAnalyze]）").arg(ms));
            },
            Qt::QueuedConnection);
    });
}

ScreenInspectWidget::InspectReport ScreenInspectWidget::analyze(const QImage& currRgb, const QImage& refRgb,
                                                               const InspectParams& p) {
    ScreenInspectAnalyzer::Params ap;
    ap.deadDiff = p.deadDiff;
    ap.expectedColor = p.expectedColor;
    ap.manualRoi = p.manualRoi;
    const ScreenInspectAnalyzer::Report raw = ScreenInspectAnalyzer::analyze(currRgb, refRgb, ap);

    InspectReport report;
    report.ssim = raw.ssim;
    report.deadPixels = raw.deadPixels;
    report.muraStd = raw.muraStd;
    report.roi = raw.roi;
    report.annotated = raw.annotated;

    QStringList reasons;
    bool ok = true;
    if (p.expectedColor >= 0 && raw.colorMatch == 0) {
        ok = false;
        reasons.append(QStringLiteral("期望%1，实测%2")
                           .arg(ScreenInspectAnalyzer::colorName(p.expectedColor),
                                ScreenInspectAnalyzer::colorName(raw.detectedColor)));
    }
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
        report.summary = QStringLiteral("正常：实测%1，坏点%2，亮度起伏%3")
                             .arg(ScreenInspectAnalyzer::colorName(raw.detectedColor))
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
    // 高分辨率 PNG 压缩极慢，后台写盘
    QtConcurrent::run([dir, stamp, raw, annotated]() {
        QElapsedTimer t;
        t.start();
        if (!raw.isNull())
            raw.save(dir + QLatin1Char('/') + stamp + QStringLiteral("_capture.png"), "PNG");
        const qint64 msRaw = t.restart();
        if (!annotated.isNull())
            annotated.save(dir + QLatin1Char('/') + stamp + QStringLiteral("_mark.png"), "PNG");
        const qint64 msAnno = t.elapsed();
        ScreenInspectAnalyzer::cleanupStoredImages(dir);
        qDebug().noquote() << QStringLiteral("[ScreenInspectSave]")
                           << QStringLiteral("raw=%1ms anno=%2ms size=%3x%4")
                                  .arg(msRaw)
                                  .arg(msAnno)
                                  .arg(raw.width())
                                  .arg(raw.height());
    });
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
