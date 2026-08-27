#ifndef SCREEN_INSPECT_WIDGET_H
#define SCREEN_INSPECT_WIDGET_H

#include <QImage>
#include <QRect>
#include <QWidget>

class QCamera;
class QCameraImageCapture;
class QCameraViewfinder;

struct ScreenInspectUi;

/**
 * 调试工站外设「屏幕测试」页：USB 摄像头采集 + 坏点/花屏/亮度不均检测。
 * 界面在 mainwindow.ui 的 screenInspectPage 内，构造后需 bindDesignerUi。
 */
class ScreenInspectWidget : public QWidget {
    Q_OBJECT
  public:
    explicit ScreenInspectWidget(QWidget* parent = nullptr);
    ~ScreenInspectWidget() override;
    void bindDesignerUi();

  protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private slots:
    void on_btnRefreshCameras_clicked();
    void on_btnOpenPreview_clicked();
    void on_btnClosePreview_clicked();
    void on_btnCapture_clicked();
    void on_btnLoadRef_clicked();
    void on_btnSaveAsRef_clicked();
    void on_btnInspect_clicked();
    void on_btnClearRoi_clicked();
    void on_btnColorBlue_clicked();
    void on_btnColorGreen_clicked();
    void on_btnColorRed_clicked();
    void on_btnColorWhite_clicked();
    void on_btnColorBlack_clicked();
    void on_comboBox_cameraSource_currentIndexChanged(int index);

  private:
    struct InspectParams {
        double minSsim = 0.90;
        int deadDiff = 35;
        int maxDeadPixels = 8;
        double maxMuraStd = 22.0;
        int expectedColor = -1; // -1 自动；0~5 蓝绿红白黑灰
        QRect manualRoi;
    };

    struct InspectReport {
        double ssim = -1.0;
        int deadPixels = 0;
        double muraStd = 0.0;
        QRect roi;
        bool pass = false;
        QString summary;
        QImage annotated;
    };

    void loadThresholdsFromSettings();
    void saveThresholdsToSettings();
    InspectParams currentParams() const;
    void refreshCameraList();
    void updateCameraSourceUi();
    bool isGigESource() const;
    void startPreview();
    void stopPreview();
    void requestCapture();
    void captureGigEStill();
    void onStillImage(int id, const QImage& image);
    void onCaptureFailed(const QString& message);
    void setBusy(bool busy);
    void showPixmapOnLabel(const QImage& image, class QLabel* label);
    void refreshImageLabels();
    void runInspect();
    static InspectReport analyze(const QImage& currRgb, const QImage& refRgb, const InspectParams& p);
    void applyReport(const InspectReport& report);
    void saveCaptureFiles(const QImage& raw, const QImage& annotated) const;
    QString inspectDir() const;
    void applyReferenceImage(const QImage& img, const QString& sourcePath);
    void loadSavedReferenceIfAny();
    QRect labelPosToImage(const QPoint& pos) const;
    void saveManualRoi(const QRect& r);

    ScreenInspectUi* ui = nullptr;
    bool uiBound_ = false;
    QCamera* camera_ = nullptr;
    QCameraImageCapture* capture_ = nullptr;
    QCameraViewfinder* viewfinder_ = nullptr;
    QImage refImage_;
    QImage currImage_;
    QImage annotatedImage_;
    bool captureAfterReady_ = false;
    bool inspectAfterCapture_ = false;
    bool inspectRunning_ = false;
    bool busy_ = false;
    QRect manualRoi_;
    QPoint roiDragStart_;
    QPoint roiDragCur_;
    bool roiDragging_ = false;
};

#endif // SCREEN_INSPECT_WIDGET_H
