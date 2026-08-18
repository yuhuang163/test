#include "qfreeworkbox.h"

#include <QAction>
#include <QLabel>

#include "Abini.h"
#include "asd9026a_device.h"
#include "qfreework.h"
#include "shared_instrument.h"
#include "ui_qfreeworkbox.h"

QFreeWorkBox::QFreeWorkBox(QWidget* parent) : box_base(parent), ui(new Ui::QFreeWorkBox) {
    ui->setupUi(this);
    asd9026aDevice_ = new Asd9026aDevice(this);
    CreatWindow<QFreeWork>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);
    setWindowTitle("自由工站");
    ui->statusbar->addPermanentWidget(new QLabel(FREE_VER + QString(__DATE__) + " " + QString(__TIME__)));

    QAction* Fixture_connectl_act = ui->menubar->addAction("连接治具串口");
    connect(Fixture_connectl_act, &QAction::triggered, [=]() {
        if (Fixture_uart_ui == nullptr) {
            Fixture_uart_ui = new Fixture_uart;
            connect(Fixture_uart_ui, SIGNAL(send_data_to_mechine_start()), this, SLOT(startTest()));
            // Fixture_uart_ui->fixBaudRate = 115200;

            QString masterFixturecomName = SETTINGS.value(QString("mechine/0/masterFixturecomName")).toString();
            Fixture_uart_ui->ui->FixturecomNameCombo->setCurrentText(masterFixturecomName);
        }
        Fixture_uart_ui->raise();
        Fixture_uart_ui->show();
        Fixture_uart_ui->activateWindow();
    });

    QAction* startTest_act = ui->menubar->addAction("开始测试");
    connect(startTest_act, &QAction::triggered, this, &QFreeWorkBox::startTest);
}

QFreeWorkBox::~QFreeWorkBox() {
    if (Fixture_uart_ui != nullptr)
        SETTINGS.setValue(QString("mechine/0/masterFixturecomName"), Fixture_uart_ui->ui->FixturecomNameCombo->currentText());
    delete Fixture_uart_ui;
    qDeleteAll(sharedTempLoggerMutexes_);
    sharedTempLoggerMutexes_.clear();
    delete ui;
}

void QFreeWorkBox::startTest() {
    for (int i = 0; i < testList.size(); i++)
        testList[i]->startTest();
}

QString QFreeWorkBox::resolvedFixtureComName(int stationIndex) {
    const auto readPort = [](const QString& key) -> QString {
        return SETTINGS.value(key).toString().trimmed();
    };
    QString port = readPort(QStringLiteral("mechine/%1/masterFixturecomName").arg(stationIndex));
    if (!port.isEmpty())
        return port;
    port = readPort(QStringLiteral("mechine/0/masterFixturecomName"));
    if (!port.isEmpty())
        return port;
    return readPort(QStringLiteral("mechine/masterFixturecomName"));
}

QString QFreeWorkBox::selectedFixtureComName(int stationIndex) const {
    if (Fixture_uart_ui && Fixture_uart_ui->ui) {
        const QString selectedPort = Fixture_uart_ui->ui->FixturecomNameCombo->currentText().trimmed();
        if (!selectedPort.isEmpty())
            return selectedPort;
    }
    return resolvedFixtureComName(stationIndex);
}

void QFreeWorkBox::releaseSharedAsd9026aIfIdle() {
    if (!asd9026aDevice_ || !asd9026aDevice_->isOpen())
        return;
    for (test_base* station : testList) {
        if (station && station->isTestContinue)
            return;
    }
    const QString port = asd9026aDevice_->portName();
    asd9026aDevice_->close();
    emit sendBoxLog(QStringLiteral("ASD9026A 共享串口已释放：%1").arg(port));
}

SerialChannel* QFreeWorkBox::ensureSharedTempLoggerChannel(int deviceIndex0Based, const QString& portName,
                                                           QString* errorOut, int baudRate,
                                                           SerialChannel::RtsDtrMode rtsMode) {
    const QString port = portName.trimmed();
    if (port.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("共享温度仪串口名为空");
        return nullptr;
    }
    SerialChannel::OpenParams params;
    params.portName = port;
    params.baudRate = baudRate > 0 ? baudRate : 115200;
    params.readBufferSize = 4096;
    params.readDebounceMs = 35;
    params.rtsDtrMode = rtsMode;

    SerialChannel*& channel = sharedTempLoggerChannels_[deviceIndex0Based];
    if (!channel)
        channel = new SerialChannel(this);

    const SerialChannel::OpenParams cached = sharedTempLoggerOpenParams_.value(deviceIndex0Based);
    const bool sameParams = channel->isOpen() && cached.portName.compare(port, Qt::CaseInsensitive) == 0
                            && cached.baudRate == params.baudRate && cached.readDebounceMs == params.readDebounceMs
                            && cached.rtsDtrMode == params.rtsDtrMode;
    if (sameParams)
        return channel;
    if (channel->isOpen())
        channel->close();

    if (!channel->open(params)) {
        if (errorOut)
            *errorOut = QStringLiteral("%1：%2").arg(port, channel->errorString());
        return nullptr;
    }
    sharedTempLoggerOpenParams_.insert(deviceIndex0Based, params);
    emit sendBoxLog(QStringLiteral("温度记录仪共享串口已打开：设备%1 %2 波特率%3 RTS=%4")
                        .arg(deviceIndex0Based)
                        .arg(port)
                        .arg(params.baudRate)
                        .arg(SharedInstrument::tempRtsModeLabel(params.rtsDtrMode)));
    return channel;
}

QMutex* QFreeWorkBox::sharedTempLoggerMutex(int deviceIndex0Based) {
    QMutex*& mutex = sharedTempLoggerMutexes_[deviceIndex0Based];
    if (!mutex)
        mutex = new QMutex();
    return mutex;
}

void QFreeWorkBox::releaseSharedTempLoggerIfIdle() {
    for (test_base* station : testList) {
        if (station && station->isTestContinue)
            return;
    }
    for (auto it = sharedTempLoggerChannels_.begin(); it != sharedTempLoggerChannels_.end(); ++it) {
        SerialChannel* ch = it.value();
        if (!ch || !ch->isOpen())
            continue;
        const QString port = ch->portName();
        ch->close();
        emit sendBoxLog(QStringLiteral("温度记录仪共享串口已释放：设备%1 %2").arg(it.key()).arg(port));
    }
}

Fixture_uart* QFreeWorkBox::ensureFixtureUartConnected(int stationIndex, QString* detailOut,
                                                       bool* autoConnectedOut) {
    if (autoConnectedOut)
        *autoConnectedOut = false;
    if (Fixture_uart_ui == nullptr) {
        Fixture_uart_ui = new Fixture_uart;
        connect(Fixture_uart_ui, SIGNAL(send_data_to_mechine_start()), this, SLOT(startTest()));
    }
    if (Fixture_uart_ui->isFixtureSerialOpen()) {
        if (detailOut)
            *detailOut = Fixture_uart_ui->ui->FixturecomNameCombo->currentText().trimmed();
        return Fixture_uart_ui;
    }
    const QString port = resolvedFixtureComName(stationIndex);
    if (port.isEmpty()) {
        if (detailOut)
            *detailOut = QStringLiteral("配置未设置治具串口（mechine/*/masterFixturecomName）");
        return nullptr;
    }
    if (!Fixture_uart_ui->tryOpenSerialPort(port, true)) {
        if (detailOut)
            *detailOut = QStringLiteral("自动连接治具串口失败：%1").arg(port);
        return nullptr;
    }
    if (detailOut)
        *detailOut = port;
    if (autoConnectedOut)
        *autoConnectedOut = true;
    return Fixture_uart_ui;
}
