#include "qsetting.h"

#include "qsetting_bindings.h"
#include "qevent.h"
#include <algorithm>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QLineEdit>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QMimeData>
#include <QSet>
#include <QSignalBlocker>
#include <QString>
#include <QTabWidget>
#include <QVector>
#include "qpainter.h"
#include "ui_qsetting.h"
#include "test_flow/test_flow_editor.h"
#include "bydmes.h"
#include "log_upload_service.h"

#include <QFutureWatcher>
#include <QtConcurrent>

struct FreeWorkTestCatalogItem {
    int id;
    QString name;
    QString categoryKey;
};

QVector<FreeWorkTestCatalogItem> getFreeWorkTestCatalog();

namespace {
constexpr int kRowSpacing = 10;
constexpr int kColumnSpacing = 10;
constexpr int kMargin = 10;

/** 可选区 Tab 顺序与标题（key 与 testFunction.cpp 中 freeWorkTestCategoryForItem 一致） */
const QVector<QPair<QString, QString>> kFreeWorkOptionalCategoryTabs = {
    {QStringLiteral("product"), QStringLiteral("产品功能")},
    {QStringLiteral("fixture"), QStringLiteral("治具功能")},
    {QStringLiteral("dongle"), QStringLiteral("dongle功能")},
    {QStringLiteral("cloud"), QStringLiteral("云端交互")},
};

QString categoryDisplayNameForKey(const QString& categoryKey) {
    for (const auto& tab : kFreeWorkOptionalCategoryTabs) {
        if (tab.first == categoryKey) {
            return tab.second;
        }
    }
    return categoryKey;
}

/** 按 objectName 批量设置悬停说明（未列出的控件保持无提示） */
void applyNamedTooltips(QWidget* root, const QVector<QPair<QString, QString>>& tips) {
    if (!root) {
        return;
    }
    for (const auto& item : tips) {
        if (QWidget* w = root->findChild<QWidget*>(item.first)) {
            w->setToolTip(item.second);
        }
    }
}

const QString kSelectedStationKey = "TestOrderMeta/SelectedStation";
const QString kSelectedStationNameKey = "TestOrderMeta/SelectedStationName";

struct StationItem {
    QString name;
    QString key;
};

const QVector<StationItem> kPresetStations = {
    {"组装电流测试工站", "ASSEMBLY_CURRENT_TEST"},
    {"老化测试工站", "AGING_TEST"},
    {"按键测试工站", "BUTTON_TEST"},
    {"屏幕测试工站", "SCREEN_TEST"},
    {"蓝牙测试工站", "BLUETOOTH_TEST"},
    {"吸力测试工站", "SUCTION_TEST"},
};

struct TupleEnvPreset {
    QString key;
    QString baseUrl;
};

const QVector<TupleEnvPreset> kTupleEnvPresets = {
    {QStringLiteral("dev"), QStringLiteral("http://192.168.200.140:8080")},
    {QStringLiteral("prod"), QStringLiteral("https://lute-aiot-mac.luteos.com")},
    {QStringLiteral("build-dev-inner"), QStringLiteral("http://192.168.200.140:8080")},
    {QStringLiteral("build-dev"), QStringLiteral("https://983ug2va5885.vicp.fun")},
    {QStringLiteral("build-prod"), QStringLiteral("https://lute-aiot-mac.luteos.com")},
};

const QString kTupleEnvCustomKey = QStringLiteral("custom");

QString normalizeTupleBaseUrl(const QString& u) {
    QString s = u.trimmed();
    while (s.endsWith('/')) {
        s.chop(1);
    }
    return s;
}

int tupleEnvIndexForKey(QComboBox* combo, const QString& key) {
    if (!combo) {
        return -1;
    }
    for (int i = 0; i < combo->count(); ++i) {
        if (combo->itemData(i).toString() == key) {
            return i;
        }
    }
    return -1;
}

int tupleEnvIndexForUrl(QComboBox* combo, const QString& url) {
    const QString n = normalizeTupleBaseUrl(url);
    for (const auto& p : kTupleEnvPresets) {
        if (normalizeTupleBaseUrl(p.baseUrl) == n) {
            return tupleEnvIndexForKey(combo, p.key);
        }
    }
    return -1;
}

QString tupleBaseUrlForKey(const QString& key) {
    for (const auto& p : kTupleEnvPresets) {
        if (p.key == key) {
            return p.baseUrl;
        }
    }
    return {};
}

QString orderGroupName(const QString& stationKey) {
    const QString key = stationKey.trimmed();
    return key.isEmpty() ? "TestOrder_default" : QString("TestOrder_%1").arg(key);
}

QString stationKeyFromDisplayName(const QString& name) {
    const QString trimmedName = name.trimmed();
    for (const auto& item : kPresetStations) {
        if (item.name == trimmedName) {
            return item.key;
        }
    }
    return trimmedName;
}

QString stationDisplayNameFromKey(const QString& key) {
    const QString trimmedKey = key.trimmed();
    for (const auto& item : kPresetStations) {
        if (item.key == trimmedKey) {
            return item.name;
        }
    }
    return trimmedKey;
}

}

qsetting::qsetting(QWidget* parent) : QWidget(parent), ui(new Ui::qsetting) {
    ui->setupUi(this);
    updateMainStyle("Ubuntu.qss");
    setAcceptDrops(true);
    // 假设你已经定义了 QButtonGroup* StationGroup;
    // 假设 StationGroup 已经被定义并初始化
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonDebug"), 0);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonStaticCurrent"), 1);  // 更新为静态电流
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonMotorCalibration"), 2);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonImuCalibration"), 3);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonScreenTest"), 4);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonCameraTest"), 5);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonSignalTest"), 6);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonAgingTest"), 7);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonBoardFactoryTest"), 8);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonPressTest"), 9);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonFreeWorkstation"), 10);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonKeyTest"), 11);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonSuctionTest"), 12);

    // 如果需要从某个数据源添加项，可以使用循环来添加
    QStringList productList = {"V3", "V3Pro"};
    ui->comboBox_productName->addItems(productList);

    QStringList pressFunctionSwitch = {"无效选项", "单校准", "单测试", "校准加测试"};
    ui->comboBox_pressFunctionSwitch->addItems(pressFunctionSwitch);

    QStringList individualMode = {"无效选项", "单独电机", "单独按键", "都校准"};
    ui->comboBox_individualMode->addItems(individualMode);

    QStringList displayImageType = {"无效选项", "关闭所有图像", "ADC图像",     "压力值图像",
                                    "电机图像", "按键图像",     "显示全部图像"};
    ui->comboBox_displayImageType->addItems(displayImageType);

    QStringList factoryList = {"lx", "xwd", "hq", "wks", "ydm", "byd", "hz", "无mes厂"};
    ui->comboBox_factory->addItems(factoryList);

    initTupleEnvironmentCombo();
    loadConfig();
    originalStation_ = SETTINGS.value("SYSTEM/station").toString();
    connect(StationGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, [this](int) {
        const QString oldStation = originalStation_;
        saveCurrentTestOrder();
        saveConfig();
        SETTINGS.sync();
        const QString newStation = SETTINGS.value("SYSTEM/station").toString();
        if (oldStation.isEmpty() || oldStation == newStation) {
            return;
        }
        originalStation_ = newStation;
        stationReloading_ = true;
        qApp->setProperty("StationRestartRequested", true);
        const auto widgets = QApplication::topLevelWidgets();
        for (QWidget* widget : widgets) {
            widget->close();
        }
        QCoreApplication::exit(0);
    });
    initSettingTooltips();
    initFreeWorkTestOrderUi();
    initTestFlowEditorUi();
    RestoreFacDefaultSetting();
    ui->tabWidget->setCurrentIndex(0);  // 设置当前页为第一页


    ui->groupBox_SubPIDSettings->hide();
}

void qsetting::initSettingTooltips() {
    setToolTip(QStringLiteral(
        "上位机全局参数；修改工站类型后需重启程序生效。\n"
        "串口、窗口大小、当前工站、WIFI/Name* →「上位机设置.local.ini」（不入库）；\n"
        "其余参数 →「上位机设置.ini」。"));
    const int tabFixture = ui->tabWidget->indexOf(ui->tab_fixture_setting);
    const int tabKey = ui->tabWidget->indexOf(ui->tab_key_setting);
    const int tabDetail = ui->tabWidget->indexOf(ui->tab_2);
    const int tabFlow = ui->tabWidget->indexOf(ui->tab_3);
    const int tabTestFlow = ui->tabWidget->indexOf(ui->tab_test_flow);
    if (tabFixture >= 0) {
        ui->tabWidget->setTabToolTip(tabFixture, QStringLiteral("治具/压感/外设/摄像头及治具测试要求等。"));
    }
    if (tabKey >= 0) {
        ui->tabWidget->setTabToolTip(tabKey, QStringLiteral("MES、工站、电流/IMU/信号、基础信息与产品差异化等。"));
    }
    if (tabDetail >= 0) {
        ui->tabWidget->setTabToolTip(tabDetail, QStringLiteral("老化、三元组、按键 KeyId、PLC、串口仪器、协议类型等工站专用项。"));
    }
    if (tabFlow >= 0) {
        ui->tabWidget->setTabToolTip(tabFlow, QStringLiteral("自由工站测试流程：左侧为执行顺序，右侧按分类勾选后拖入配置区。"));
    }
    if (tabTestFlow >= 0) {
        ui->tabWidget->setTabToolTip(tabTestFlow,
                                     QStringLiteral("编排 test_case/*.ini 与 总的测试流程.ini；不修改旧 TestOrder。"));
    }

    applyQSettingTableBindingTooltips(this);
}

void qsetting::initFreeWorkTestOrderUi() {
    freeWorkConfigLayout_ = qobject_cast<QVBoxLayout*>(ui->config_areas);
    if (!freeWorkConfigLayout_) {
        return;
    }

    // 可选区 Tab + 各页 ScrollArea 见 qsetting.ui；此处仅绑定网格并统一间距
    freeWorkOptionalLayouts_.clear();
    const QHash<QString, QGridLayout*> optionalGrids = {
        {QStringLiteral("product"), ui->optional_areas_product},
        {QStringLiteral("fixture"), ui->optional_areas_fixture},
        {QStringLiteral("dongle"), ui->optional_areas_dongle},
        {QStringLiteral("cloud"), ui->optional_areas_cloud},
    };
    for (const auto& tab : kFreeWorkOptionalCategoryTabs) {
        QGridLayout* grid = optionalGrids.value(tab.first);
        if (!grid) {
            continue;
        }
        grid->setVerticalSpacing(kRowSpacing);
        grid->setHorizontalSpacing(kColumnSpacing);
        grid->setContentsMargins(kMargin, kMargin, kMargin, kMargin);
        freeWorkOptionalLayouts_.insert(tab.first, grid);
    }

    const auto catalog = getFreeWorkTestCatalog();
    freeWorkRows_ = (catalog.size() + freeWorkCols_ - 1) / freeWorkCols_;
    freeWorkCheckBoxes_.clear();
    freeWorkCheckBoxes_.reserve(catalog.size());

    QHash<QString, int> optionalPosByCategory;
    for (const auto& tab : kFreeWorkOptionalCategoryTabs) {
        optionalPosByCategory.insert(tab.first, 0);
    }

    for (const auto& item : catalog) {
        auto* checkBox = new DraggableCheckBox(item.name, item.id, this);
        connect(checkBox, &QCheckBox::stateChanged, this, [this](int) {
            testOrderDirty_ = true;
            saveCurrentTestOrder();
        });
        checkBox->setToolTip(QStringLiteral("【%1】%2（id=%3）\n勾选后可拖入左侧「配置区域」，按自上而下顺序执行。")
                                 .arg(categoryDisplayNameForKey(item.categoryKey), item.name)
                                 .arg(item.id));
        freeWorkCheckBoxes_.append(checkBox);
        QString cat = item.categoryKey;
        if (!freeWorkOptionalLayouts_.contains(cat)) {
            cat = QStringLiteral("product");
        }
        QGridLayout* const grid = freeWorkOptionalLayouts_.value(cat);
        const int pos = optionalPosByCategory.value(cat);
        grid->addWidget(checkBox, pos / freeWorkCols_, pos % freeWorkCols_);
        optionalPosByCategory[cat] = pos + 1;
    }

    initTestOrderStationSelector();
    reorderFreeWorkCheckBoxes();
}

QString qsetting::categoryKeyForCheckBox(const DraggableCheckBox* checkBox) const {
    if (!checkBox) {
        return QStringLiteral("product");
    }
    const auto catalog = getFreeWorkTestCatalog();
    for (const auto& item : catalog) {
        if (item.id == checkBox->getIndex()) {
            return item.categoryKey;
        }
    }
    return QStringLiteral("product");
}

QGridLayout* qsetting::optionalLayoutForCategory(const QString& categoryKey) const {
    return freeWorkOptionalLayouts_.value(categoryKey, nullptr);
}

QGridLayout* qsetting::optionalLayoutAtGlobalPos(const QPoint& globalPos, QRect* areaOut) const {
    if (!ui->freeWorkOptionalTabs) {
        return nullptr;
    }
    for (int t = 0; t < ui->freeWorkOptionalTabs->count(); ++t) {
        QWidget* const pageHost = ui->freeWorkOptionalTabs->widget(t);
        if (!pageHost) {
            continue;
        }
        const QRect globalRect(pageHost->mapToGlobal(QPoint(0, 0)), pageHost->size());
        if (!globalRect.contains(globalPos)) {
            continue;
        }
        const QString key = kFreeWorkOptionalCategoryTabs.at(t).first;
        if (areaOut) {
            *areaOut = globalRect;
        }
        return freeWorkOptionalLayouts_.value(key, nullptr);
    }
    return nullptr;
}

void qsetting::removeCheckBoxFromAllOptionalLayouts(DraggableCheckBox* checkBox) {
    if (!checkBox) {
        return;
    }
    for (QGridLayout* layout : freeWorkOptionalLayouts_) {
        if (layout && layout->indexOf(checkBox) >= 0) {
            layout->removeWidget(checkBox);
        }
    }
    if (freeWorkConfigLayout_ && freeWorkConfigLayout_->indexOf(checkBox) >= 0) {
        freeWorkConfigLayout_->removeWidget(checkBox);
    }
}

int qsetting::optionalWidgetCount(const QGridLayout* layout) const {
    if (!layout) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < layout->count(); ++i) {
        if (layout->itemAt(i) && layout->itemAt(i)->widget()) {
            ++count;
        }
    }
    return count;
}

int qsetting::optionalRowCount(const QGridLayout* layout) const {
    const int count = optionalWidgetCount(layout);
    return qMax(1, (count + freeWorkCols_ - 1) / freeWorkCols_);
}

void qsetting::initTupleEnvironmentCombo() {
    QComboBox* c = ui->comboBox_tupleEnvironment;
    if (!c) {
        return;
    }
    c->clear();
    for (const auto& p : kTupleEnvPresets) {
        c->addItem(p.key, p.key);
    }
    c->addItem(QStringLiteral("自定义"), kTupleEnvCustomKey);
    connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &qsetting::on_comboBox_tupleEnvironment_currentIndexChanged);
}

void qsetting::on_comboBox_tupleEnvironment_currentIndexChanged(int index) {
    Q_UNUSED(index);
    QComboBox* c = ui->comboBox_tupleEnvironment;
    if (!c) {
        return;
    }
    const QString key = c->currentData().toString();
    if (key == kTupleEnvCustomKey || key.isEmpty()) {
        return;
    }
    const QString url = tupleBaseUrlForKey(key);
    if (!url.isEmpty()) {
        ui->lineEdit_tupleBaseUrl->setText(url);
    }
}

void qsetting::initTestOrderStationSelector() {
    if (!ui->comboBox_testOrderStation) {
        return;
    }

    QVector<StationItem> stationCandidates = kPresetStations;
    auto appendCandidate = [&stationCandidates](const QString& rawValue) {
        const QString key = stationKeyFromDisplayName(rawValue);
        if (key.isEmpty()) {
            return;
        }
        stationCandidates.append({stationDisplayNameFromKey(key), key});
    };
    appendCandidate(SETTINGS.value(kSelectedStationKey).toString());
    appendCandidate(SETTINGS.value(kSelectedStationNameKey).toString());



    SETTINGS.beginGroup("TestOrder");
    const QStringList legacyGroupKeys = SETTINGS.childGroups();
    SETTINGS.endGroup();
    for (const QString& groupKey : legacyGroupKeys) {
        appendCandidate(groupKey);
    }

    const QStringList rootGroups = SETTINGS.childGroups();
    for (const QString& group : rootGroups) {
        if (group.startsWith("TestOrder_")) {
            appendCandidate(group.mid(QString("TestOrder_").size()));
        }
    }

    QSet<QString> uniqueStations;
    QVector<StationItem> stations;
    for (const auto& station : stationCandidates) {
        const QString key = station.key.trimmed();

        if (!key.isEmpty() && !uniqueStations.contains(key)) {
            uniqueStations.insert(key);
            stations.append({station.name.trimmed(), key});
        }
    }
    if (stations.isEmpty()) {
        stations.append({"default", "default"});
    }

    switchingTestOrderStation_ = true;
    ui->comboBox_testOrderStation->clear();
    for (const auto& station : stations) {
        ui->comboBox_testOrderStation->addItem(station.name, station.key);
    }

    activeTestOrderStation_ = stationKeyFromDisplayName(
        SETTINGS.value(kSelectedStationKey, SETTINGS.value("MES/test_station", "default")).toString());
    if (activeTestOrderStation_.isEmpty()) {
        activeTestOrderStation_ = "default";
    }
    int idx = ui->comboBox_testOrderStation->findData(activeTestOrderStation_);
    if (idx < 0) {
        idx = ui->comboBox_testOrderStation->findText(stationDisplayNameFromKey(activeTestOrderStation_));
    }
    if (idx >= 0) {
        ui->comboBox_testOrderStation->setCurrentIndex(idx);
    } else {
        ui->comboBox_testOrderStation->setCurrentIndex(0);
        activeTestOrderStation_ = ui->comboBox_testOrderStation->currentData().toString().trimmed();
    }
    switchingTestOrderStation_ = false;
    SETTINGS.setValue(kSelectedStationKey, activeTestOrderStation_);
    SETTINGS.setValue(kSelectedStationNameKey, stationDisplayNameFromKey(activeTestOrderStation_));
    applyStationUiState(activeTestOrderStation_);

}

void qsetting::applyStationUiState(const QString& stationKey) {
    const QString key = stationKey.trimmed();
    if (ui->config) {
        ui->config->setTitle(QString("配置区域（%1）").arg(stationDisplayNameFromKey(key)));
    }
}

QString qsetting::currentTestOrderStation() const {
    if (!activeTestOrderStation_.isEmpty()) {
        return activeTestOrderStation_;
    }
    if (ui->comboBox_testOrderStation) {
        const QString currentStation = ui->comboBox_testOrderStation->currentData().toString().trimmed();
        if (!currentStation.isEmpty()) {
            return currentStation;
        }
    }
    return SETTINGS.value("MES/test_station", "default").toString().trimmed();
}

QVector<int> qsetting::loadTestOrderIndexes(const QString& station) const {
    QVector<int> indexes;
    auto loadIndexesFromGroup = [&indexes](const QString& groupPath) {
        SETTINGS.beginGroup(groupPath);
        QStringList keys = SETTINGS.childKeys();
        std::sort(keys.begin(), keys.end(), [](const QString& left, const QString& right) { return left.toInt() < right.toInt(); });
        for (const QString& key : keys) {
            indexes.append(SETTINGS.value(key).toInt());
        }
        SETTINGS.endGroup();
    };

    const QString trimmedStation = station.trimmed();
    if (!trimmedStation.isEmpty()) {
        loadIndexesFromGroup(orderGroupName(trimmedStation));
    }
    // 兼容旧版写法: [TestOrder] 下的 station\index
    if (indexes.isEmpty() && !trimmedStation.isEmpty()) {
        loadIndexesFromGroup(QString("TestOrder/%1").arg(trimmedStation));
    }
    // if (indexes.isEmpty()) {
    //     loadIndexesFromGroup("TestOrder");
    // }
    // if (indexes.isEmpty()) {
    //     loadIndexesFromGroup(orderGroupName("default"));
    // }
    // if (indexes.isEmpty()) {
    //     loadIndexesFromGroup("TestOrder/default");
    // }
    return indexes;
}

void qsetting::saveTestOrderIndexes(const QString& station, const QVector<int>& indexes) const {
    QString groupPath = "TestOrder";
    const QString trimmedStation = station.trimmed();
    if (!trimmedStation.isEmpty()) {
        groupPath = orderGroupName(trimmedStation);
    }
    SETTINGS.beginGroup(groupPath);
    SETTINGS.remove("");
    for (int i = 0; i < indexes.size(); ++i) {
        SETTINGS.setValue(QString::number(i), indexes.at(i));
    }
    SETTINGS.endGroup();
}

DraggableCheckBox* qsetting::getConfiguredCheckBoxByIndex(int index) const {
    if (!freeWorkConfigLayout_) {
        return nullptr;
    }
    for (int i = 0; i < freeWorkConfigLayout_->count(); ++i) {
        auto* checkBox = qobject_cast<DraggableCheckBox*>(freeWorkConfigLayout_->itemAt(i)->widget());
        if (checkBox && checkBox->getIndex() == index) {
            return checkBox;
        }
    }
    return nullptr;
}

DraggableCheckBox* qsetting::getOptionalCheckBoxByIndex(int index) const {
    for (QGridLayout* layout : freeWorkOptionalLayouts_) {
        if (!layout) {
            continue;
        }
        for (int i = 0; i < layout->count(); ++i) {
            auto* checkBox = qobject_cast<DraggableCheckBox*>(layout->itemAt(i)->widget());
            if (checkBox && checkBox->getIndex() == index) {
                return checkBox;
            }
        }
    }
    return nullptr;
}

void qsetting::reorderFreeWorkCheckBoxes() {
    if (!freeWorkConfigLayout_ || freeWorkOptionalLayouts_.isEmpty()) {
        qDebug() << "[TestOrder] reorder skipped, layout null";
        return;
    }

    const QString stationKey = currentTestOrderStation();
    const QVector<int> indexes = loadTestOrderIndexes(stationKey);
    qDebug() << "[TestOrder] reorder begin, station =" << stationKey << ", indexes =" << indexes;

    while (QLayoutItem* item = freeWorkConfigLayout_->takeAt(0)) {
        delete item;
    }
    for (QGridLayout* layout : freeWorkOptionalLayouts_) {
        if (!layout) {
            continue;
        }
        while (QLayoutItem* item = layout->takeAt(0)) {
            delete item;
        }
    }

    QVector<QPair<DraggableCheckBox*, bool>> blockedStates;
    blockedStates.reserve(freeWorkCheckBoxes_.size());
    for (DraggableCheckBox* checkBox : freeWorkCheckBoxes_) {
        if (checkBox) {
            blockedStates.append(qMakePair(checkBox, checkBox->blockSignals(true)));
            removeCheckBoxFromAllOptionalLayouts(checkBox);
        }
    }

    QSet<int> selectedIds;
    for (int index : indexes) {
        if (selectedIds.contains(index)) {
            qDebug() << "[TestOrder] duplicate index ignored:" << index;
            continue;
        }
        DraggableCheckBox* checkBox = nullptr;
        for (DraggableCheckBox* item : freeWorkCheckBoxes_) {
            if (item && item->getIndex() == index) {
                checkBox = item;
                break;
            }
        }
        if (!checkBox) {
            qDebug() << "[TestOrder] index has no checkbox, ignored:" << index;
            continue;
        }
        selectedIds.insert(index);

        freeWorkConfigLayout_->addWidget(checkBox);
        checkBox->show();
    }

    QHash<QString, int> optionalPosByCategory;
    for (const auto& tab : kFreeWorkOptionalCategoryTabs) {
        optionalPosByCategory.insert(tab.first, 0);
    }
    for (DraggableCheckBox* checkBox : freeWorkCheckBoxes_) {
        if (!checkBox || selectedIds.contains(checkBox->getIndex())) {
            continue;
        }
        QString cat = categoryKeyForCheckBox(checkBox);
        QGridLayout* grid = freeWorkOptionalLayouts_.value(cat);
        if (!grid) {
            cat = QStringLiteral("product");
            grid = freeWorkOptionalLayouts_.value(cat);
        }
        if (!grid) {
            continue;
        }
        const int pos = optionalPosByCategory.value(cat);
        grid->addWidget(checkBox, pos / freeWorkCols_, pos % freeWorkCols_);
        checkBox->show();
        optionalPosByCategory[cat] = pos + 1;
    }

    for (const auto& state : blockedStates) {
        if (state.first) {
            state.first->blockSignals(state.second);
        }
    }

    freeWorkConfigLayout_->invalidate();
    for (QGridLayout* layout : freeWorkOptionalLayouts_) {
        if (layout) {
            layout->invalidate();
        }
    }
    if (ui->config) {
        ui->config->updateGeometry();
        ui->config->update();
    }
    if (ui->can_use) {
        ui->can_use->updateGeometry();
        ui->can_use->update();
    }
    updateGeometry();
    update();
    int optionalTotal = 0;
    for (QGridLayout* layout : freeWorkOptionalLayouts_) {
        optionalTotal += optionalWidgetCount(layout);
    }
    qDebug() << "[TestOrder] after rebuild, config count =" << freeWorkConfigLayout_->count()
             << ", optional count =" << optionalTotal << ", selected ids =" << selectedIds.values();

    savedTestOrderSnapshot_ = indexes;
    testOrderDirty_ = false;
}

void qsetting::saveCurrentTestOrder() {
    if (!freeWorkConfigLayout_) {
        return;
    }
    QVector<int> indexes;
    for (int i = 0; i < freeWorkConfigLayout_->count(); ++i) {
        auto* checkBox = qobject_cast<DraggableCheckBox*>(freeWorkConfigLayout_->itemAt(i)->widget());
        if (checkBox && checkBox->isChecked()) {
            indexes.append(checkBox->getIndex());
        }
    }
    saveTestOrderIndexes(currentTestOrderStation(), indexes);
}

void qsetting::moveToLayout(QLayout* fromLayout, QLayout* toLayout, QWidget* widget) {
    if (!widget || !toLayout) {
        return;
    }
    if (fromLayout) {
        fromLayout->removeWidget(widget);
    }
    toLayout->addWidget(widget);
}

void qsetting::moveToGrid(QGridLayout* layout, QWidget* widget, int row, int col) {
    if (!layout || !widget) {
        return;
    }
    if (layout->indexOf(widget) != -1) {
        layout->removeWidget(widget);
    }
    layout->addWidget(widget, row, col);
}

void qsetting::moveToOptionalByPosition(DraggableCheckBox* checkBox, QGridLayout* optionalLayout, int row, int col) {
    if (!optionalLayout || !checkBox) {
        return;
    }

    const int targetIndex = qMax(0, row * freeWorkCols_ + col);
    QVector<DraggableCheckBox*> optionalWidgets;
    optionalWidgets.reserve(optionalLayout->count() + 1);

    for (int i = 0; i < optionalLayout->count(); ++i) {
        auto* widget = qobject_cast<DraggableCheckBox*>(optionalLayout->itemAt(i)->widget());
        if (widget && widget != checkBox) {
            optionalWidgets.append(widget);
        }
    }

    const int insertIndex = qBound(0, targetIndex, optionalWidgets.size());
    optionalWidgets.insert(insertIndex, checkBox);

    removeCheckBoxFromAllOptionalLayouts(checkBox);

    for (DraggableCheckBox* widget : optionalWidgets) {
        optionalLayout->removeWidget(widget);
    }
    for (int i = 0; i < optionalWidgets.size(); ++i) {
        optionalLayout->addWidget(optionalWidgets.at(i), i / freeWorkCols_, i % freeWorkCols_);
    }
}

void qsetting::calculateGridPosition(const QPoint& globalPos, const QRect& area, int& row, int& col,
                                     const QGridLayout* optionalLayout) const {
    if (!optionalLayout) {
        row = 0;
        col = 0;
        return;
    }
    const int rows = optionalRowCount(optionalLayout);
    const int singleHeight = qMax(1, (area.height() - 2 * kMargin + kRowSpacing) / rows);
    const int singleWidth = qMax(1, (area.width() - 2 * kMargin + kColumnSpacing) / qMax(1, freeWorkCols_));
    row = qBound(0, (globalPos.y() - area.y() - kMargin) / singleHeight, qMax(0, rows - 1));
    col = qBound(0, (globalPos.x() - area.x() - kMargin) / singleWidth, qMax(0, freeWorkCols_ - 1));
}

int qsetting::getIndexAt(const QPoint& globalPos) const {
    if (!freeWorkConfigLayout_) {
        return -1;
    }
    for (int i = 0; i < freeWorkConfigLayout_->count(); ++i) {
        QWidget* widget = freeWorkConfigLayout_->itemAt(i)->widget();
        if (!widget) {
            continue;
        }
        const QRect globalRect(widget->mapToGlobal(QPoint(0, 0)), widget->size());
        if (globalRect.contains(globalPos)) {
            return i;
        }
    }
    return -1;
}

void qsetting::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
    }
}

void qsetting::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasText()) {
        dragPos_ = event->pos();
        update();
        event->acceptProposedAction();
    }
}

void qsetting::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    if (!mimeData->hasText()) {
        return;
    }

    const int sourceIndex = mimeData->text().toInt();
    DraggableCheckBox* sourceCheckBox = getConfiguredCheckBoxByIndex(sourceIndex);
    if (!sourceCheckBox) {
        sourceCheckBox = getOptionalCheckBoxByIndex(sourceIndex);
    }
    if (!sourceCheckBox || !freeWorkConfigLayout_ || freeWorkOptionalLayouts_.isEmpty()) {
        return;
    }

    const QPoint mouseGlobalPos = mapToGlobal(event->pos());
    const QRect configGlobalArea(ui->config->mapToGlobal(QPoint(0, 0)), ui->config->size());
    QRect optionalGlobalArea;
    QGridLayout* targetOptionalLayout = optionalLayoutAtGlobalPos(mouseGlobalPos, &optionalGlobalArea);

    if (configGlobalArea.contains(mouseGlobalPos)) {
        removeCheckBoxFromAllOptionalLayouts(sourceCheckBox);
        freeWorkConfigLayout_->addWidget(sourceCheckBox);
        const int destIndex = getIndexAt(mouseGlobalPos);
        if (destIndex >= 0 && destIndex != freeWorkConfigLayout_->indexOf(sourceCheckBox)) {
            freeWorkConfigLayout_->removeWidget(sourceCheckBox);
            freeWorkConfigLayout_->insertWidget(destIndex, sourceCheckBox);
        }
        event->acceptProposedAction();
    } else if (targetOptionalLayout && !optionalGlobalArea.isNull()) {
        int row = 0;
        int col = 0;
        calculateGridPosition(mouseGlobalPos, optionalGlobalArea, row, col, targetOptionalLayout);
        moveToOptionalByPosition(sourceCheckBox, targetOptionalLayout, row, col);
        event->acceptProposedAction();
    }

    dragPos_ = QPoint();
    testOrderDirty_ = true;
    saveCurrentTestOrder();
}

void qsetting::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    if (dragPos_.isNull() || !ui->freeWorkOptionalTabs) {
        return;
    }

    const QPoint globalDrag = mapToGlobal(dragPos_);
    QRect optionalArea;
    QGridLayout* layout = optionalLayoutAtGlobalPos(globalDrag, &optionalArea);
    if (!layout || optionalArea.isNull()) {
        return;
    }

    const int rows = optionalRowCount(layout);
    const int singleHeight = qMax(1, (optionalArea.height() - (2 * kMargin) + kRowSpacing) / rows);
    const int singleWidth = qMax(1, (optionalArea.width() - (2 * kMargin) + kColumnSpacing) / qMax(1, freeWorkCols_));

    QPainter painter(this);
    painter.fillRect(QRect(dragPos_, QSize(singleWidth - kColumnSpacing, singleHeight - kRowSpacing)), Qt::green);
}

void qsetting::readSubPIDAndFilter() {
    SETTINGS.beginGroup("SUBPID");
    // 读取 [SUBPID] 部分的所有键值对
    QStringList subPIDKeys = SETTINGS.childKeys();  // 获取所有键
    SETTINGS.endGroup();
    // 用于存储不同值的变量
    QString subPID_00;
    QString subPID_01;
    QString subPID_02;
    QString subPID_03;
    // qDebug() << "subPIDKeys###########: " << subPIDKeys;
    // 遍历所有键，并读取对应的值
    for (const QString& key : subPIDKeys) {
        QString value = SETTINGS.value("SUBPID/" + key).toString();

        // 根据值分类，将键名加到相应的变量中
        if (value == "00") {
            if (!subPID_00.isEmpty()) {
                subPID_00 += "=";  // 如果不是第一个键，添加 "=" 连接符
            }
            subPID_00 += key;
        } else if (value == "01") {
            if (!subPID_01.isEmpty()) {
                subPID_01 += "=";
            }
            subPID_01 += key;
        } else if (value == "02") {
            if (!subPID_02.isEmpty()) {
                subPID_02 += "=";
            }
            subPID_02 += key;
        } else if (value == "03") {
            if (!subPID_03.isEmpty()) {
                subPID_03 += "=";
            }
            subPID_03 += key;
        }
    }

    // 输出结果
    // qDebug() << "subPID_00: " << subPID_00;
    // qDebug() << "subPID_01: " << subPID_01;
    // qDebug() << "subPID_02: " << subPID_02;
    // qDebug() << "subPID_03: " << subPID_03;

    // 将数据填充到 QLineEdit 控件
    ui->lineEdit_SubPID_00->setText(subPID_00);
    ui->lineEdit_SubPID_01->setText(subPID_01);
    ui->lineEdit_SubPID_02->setText(subPID_02);
    ui->lineEdit_SubPID_03->setText(subPID_03);
}
void qsetting::loadConfig() {
    // 假设 SETTINGS 是一个 QSettings 实例
    QString station = SETTINGS.value("SYSTEM/station").toString();  // 读取 station 的值
    const QSize availableSize = QApplication::desktop()->availableGeometry(this).size();
    QVariant windowSize(availableSize / 4 * 3);
    this->resize(SETTINGS.value("Window/SettingSize", windowSize).toSize());

    // 使用映射将 station 和 QRadioButton 关联
    QMap<QString, QRadioButton*> stationMap = {{"IMU_CALI", ui->radioButtonImuCalibration},
                                               {"MOTOR_TEST", ui->radioButtonMotorCalibration},
                                               {"QUIESCENT_CURRENT", ui->radioButtonStaticCurrent},
                                               {"SCREEN_TEST", ui->radioButtonScreenTest},
                                               {"CAMERA_TEST", ui->radioButtonCameraTest},
                                               {"WIFIBLE_TEST", ui->radioButtonSignalTest},
                                               {"AGE_TEST", ui->radioButtonAgingTest},
                                               {"PCBA_TEST", ui->radioButtonBoardFactoryTest},
                                               {"FREE_WORK", ui->radioButtonFreeWorkstation},
                                               {"KEY_TEST", ui->radioButtonKeyTest},
                                               {"SUCTION_TEST", ui->radioButtonSuctionTest},
                                               {"MAIN_TEST", ui->radioButtonDebug},
                                               {"PRESS_TEST", ui->radioButtonPressTest}};

    // 清除所有 QRadioButton 的选中状态
    for (auto button : stationMap) {
        button->setChecked(false);
    }

    // 根据 station 的值设置对应的 QRadioButton 为选中状态
    if (stationMap.contains(station)) {
        stationMap[station]->setChecked(true);
    }

    loadQSettingTableBindings(this);
    readSubPIDAndFilter();

    if (ui->comboBox_systemProtocolType->count() == 0) {
        ui->comboBox_systemProtocolType->addItem(QStringLiteral("qpb（产测 PB）"), QStringLiteral("qpb"));
        ui->comboBox_systemProtocolType->addItem(QStringLiteral("qfctp（FCTP）"), QStringLiteral("qfctp"));
        ui->comboBox_systemProtocolType->addItem(QStringLiteral("qaiot（AIOT TLV）"), QStringLiteral("qaiot"));
    }
    {
        const QString proto = SETTINGS.value(QStringLiteral("SYSTEM/ProtocolType"), QStringLiteral("qpb"))
                                  .toString()
                                  .trimmed()
                                  .toLower();
        int idx = ui->comboBox_systemProtocolType->findData(proto);
        if (idx < 0) {
            idx = 0;
        }
        ui->comboBox_systemProtocolType->setCurrentIndex(idx);
    }
    ui->lineEdit_plcIpAddressStation2->setText(SETTINGS.value(QStringLiteral("PLC/IpAddress_Station2"), QString()).toString());
    ui->lineEdit_plcPortStation2->setText(SETTINGS.value(QStringLiteral("PLC/Port_Station2"), QString()).toString());
    ui->lineEdit_plcUnitIdStation2->setText(SETTINGS.value(QStringLiteral("PLC/UnitId_Station2"), QString()).toString());
    {
        const int mb2 = SETTINGS.value(QStringLiteral("PLC/MBase_Station2"), -1).toInt();
        ui->lineEdit_plcMBaseStation2->setText(mb2 < 0 ? QString() : QString::number(mb2));
    }
    ui->lineEdit_plcMCoilOffsetStation2->setText(
        SETTINGS.value(QStringLiteral("PLC/MCoilAddressOffset_Station2"), QString()).toString());

    const QString defaultTupleUrl = kTupleEnvPresets.first().baseUrl;
    const QString savedTupleUrl = SETTINGS.value("Tuple/BaseUrl", defaultTupleUrl).toString();
    ui->lineEdit_tupleBaseUrl->setText(savedTupleUrl);

    {
        QSignalBlocker blocker(ui->comboBox_tupleEnvironment);
        QString envKey = SETTINGS.value("Tuple/Environment").toString().trimmed();
        int idx = envKey.isEmpty() ? -1 : tupleEnvIndexForKey(ui->comboBox_tupleEnvironment, envKey);
        if (idx < 0) {
            idx = tupleEnvIndexForUrl(ui->comboBox_tupleEnvironment, savedTupleUrl);
        }
        if (idx < 0) {
            idx = ui->comboBox_tupleEnvironment->count() - 1;
        }
        ui->comboBox_tupleEnvironment->setCurrentIndex(idx);
    }

    {
        const QString capEndian = SETTINGS.value(QStringLiteral("KeyCap/ValueEndian"), QStringLiteral("big")).toString();
        ui->comboBox_KeyCapValueEndian->setCurrentIndex(
            capEndian.compare(QStringLiteral("little"), Qt::CaseInsensitive) == 0 ? 1 : 0);
    }

    if (ui->lineEdit_remoteLogUploadUrl->text().trimmed().isEmpty()) {
        ui->lineEdit_remoteLogUploadUrl->setText(LogUploadService::defaultUploadUrl());
    }
    if (ui->lineEdit_remoteLogDeviceId->text().trimmed().isEmpty()) {
        ui->lineEdit_remoteLogDeviceId->setText(LogUploadService::defaultDeviceId());
    }
    if (ui->lineEdit_remoteLogStation->text().trimmed().isEmpty()) {
        ui->lineEdit_remoteLogStation->setText(QStringLiteral("DEFAULT"));
    }
    ui->label_remoteLogStatus->setText(QString());
}
void qsetting::updateMainStyle(QString style) {
    applyWidgetStyleSheet(this, style);
}
qsetting::~qsetting() {
    qDebug() << "已经删除页面";
    delete ui;
}

void qsetting::saveSubPIDAndFilter() {
    // 获取 QLineEdit 控件中的文本内容
    QString subPID_00 = ui->lineEdit_SubPID_00->text();
    QString subPID_01 = ui->lineEdit_SubPID_01->text();
    QString subPID_02 = ui->lineEdit_SubPID_02->text();
    QString subPID_03 = ui->lineEdit_SubPID_03->text();

    // 输出当前内容
    // qDebug() << "subPID_00 (from UI): " << subPID_00;
    // qDebug() << "subPID_01 (from UI): " << subPID_01;
    // qDebug() << "subPID_02 (from UI): " << subPID_02;
    // qDebug() << "subPID_03 (from UI): " << subPID_03;

    // 进入 SUBPID 组
    SETTINGS.beginGroup("SUBPID");

    // 清空原有的配置
    for (const QString& key : SETTINGS.childKeys()) {
        SETTINGS.remove(key);
    }

    // 解析并保存每个 subPID 组的键值对
    // 将 subPID_00 中的键值对存入文件
    QStringList keys_00 = subPID_00.split("=");
    for (const QString& key : keys_00) {
        SETTINGS.setValue(key, "00");
    }

    // 将 subPID_01 中的键值对存入文件
    QStringList keys_01 = subPID_01.split("=");
    for (const QString& key : keys_01) {
        SETTINGS.setValue(key, "01");
    }

    // 将 subPID_02 中的键值对存入文件
    QStringList keys_02 = subPID_02.split("=");
    for (const QString& key : keys_02) {
        SETTINGS.setValue(key, "02");
    }

    // 将 subPID_03 中的键值对存入文件
    QStringList keys_03 = subPID_03.split("=");
    for (const QString& key : keys_03) {
        SETTINGS.setValue(key, "03");
    }

    // 结束 SUBPID 组
    SETTINGS.endGroup();

    // 输出保存后的结果
    qDebug() << "Saved SUBPID data to file!";
}

void qsetting::saveConfig() {
    SETTINGS.setValue("Window/SettingSize", this->size());
    // 假设 SETTINGS 是一个 QSettings 实例
    SETTINGS.setValue("SYSTEM/station", ui->radioButtonImuCalibration->isChecked()   ? "IMU_CALI" :
                                        ui->radioButtonMotorCalibration->isChecked() ? "MOTOR_TEST" :
                                        ui->radioButtonStaticCurrent->isChecked()    ? "QUIESCENT_CURRENT" :
                                        ui->radioButtonScreenTest->isChecked()       ? "SCREEN_TEST" :
                                        ui->radioButtonCameraTest->isChecked()       ? "CAMERA_TEST" :
                                        ui->radioButtonSignalTest->isChecked()       ? "WIFIBLE_TEST" :
                                        ui->radioButtonAgingTest->isChecked()        ? "AGE_TEST" :
                                        ui->radioButtonPressTest->isChecked()        ? "PRESS_TEST" :
                                        ui->radioButtonBoardFactoryTest->isChecked() ? "PCBA_TEST" :
                                        ui->radioButtonKeyTest->isChecked()          ? "KEY_TEST" :
                                        ui->radioButtonSuctionTest->isChecked()      ? "SUCTION_TEST" :
                                        ui->radioButtonFreeWorkstation->isChecked()  ? "FREE_WORK" :
                                                                                       "MAIN_TEST");

    saveQSettingTableBindings(this);
    saveSubPIDAndFilter();

    SETTINGS.setValue(QStringLiteral("SYSTEM/ProtocolType"),
                      ui->comboBox_systemProtocolType->currentData().toString().trimmed());

    auto savePlcOptional = [](const QString& key, const QString& raw) {
        const QString t = raw.trimmed();
        if (t.isEmpty()) {
            SETTINGS.remove(key);
        } else {
            SETTINGS.setValue(key, t);
        }
    };
    savePlcOptional(QStringLiteral("PLC/IpAddress_Station2"), ui->lineEdit_plcIpAddressStation2->text());
    savePlcOptional(QStringLiteral("PLC/Port_Station2"), ui->lineEdit_plcPortStation2->text());
    savePlcOptional(QStringLiteral("PLC/UnitId_Station2"), ui->lineEdit_plcUnitIdStation2->text());
    {
        const QString t = ui->lineEdit_plcMBaseStation2->text().trimmed();
        if (t.isEmpty()) {
            SETTINGS.remove(QStringLiteral("PLC/MBase_Station2"));
        } else {
            SETTINGS.setValue(QStringLiteral("PLC/MBase_Station2"), t.toInt());
        }
    }
    savePlcOptional(QStringLiteral("PLC/MCoilAddressOffset_Station2"), ui->lineEdit_plcMCoilOffsetStation2->text());

    SETTINGS.setValue("Tuple/Environment", ui->comboBox_tupleEnvironment->currentData().toString());
    SETTINGS.setValue("Tuple/BaseUrl", ui->lineEdit_tupleBaseUrl->text());

    SETTINGS.setValue(QStringLiteral("KeyCap/ValueEndian"),
                      ui->comboBox_KeyCapValueEndian->currentIndex() == 1 ? QStringLiteral("little")
                                                                           : QStringLiteral("big"));
    if (ui->comboBox_factory->currentText() == QStringLiteral("byd")) {
        bydmes::loadExternalMesConfig(nullptr);
    }
}

void qsetting::closeEvent(QCloseEvent* event) {
    if (stationReloading_) {
        qDebug() << "切换工站中，跳过设置页关闭保存";
        event->accept();
        return;
    }
    const int tabTestFlow = ui->tabWidget->indexOf(ui->tab_test_flow);
    if (testFlowEditor_ && tabTestFlow >= 0 && ui->tabWidget->currentIndex() == tabTestFlow) {
        if (!testFlowEditor_->confirmDiscardOrSaveOnLeave()) {
            event->ignore();
            return;
        }
    }
    saveCurrentTestOrder();
    saveConfig();
    qDebug() << "已经保存配置信息";
    event->accept();
}

void qsetting::RestoreProductDefaultSetting() {
    ui->checkBox_PressIndependent->setChecked(true);  //默认所有产品都直接开始
    ui->checkBox_NeedWriteSkuid->setChecked(false);
    ui->checkBox_NeedWriteSubpid->setChecked(false);
    ui->checkBox_BluetoothImageTransfer->setChecked(false);
    ui->checkBox_IMUCalibrationWakeup->setChecked(false);
    ui->checkBox_DisableSerialPortRx->setChecked(false);
    ui->checkBox_ShipModeResponse->setChecked(false);
    ui->checkBox_SerialPortMAC->setChecked(false);
    ui->checkBox_MagneticReuseMotorStatus->setChecked(false);
    ui->checkBox_TestAudioCurrent->setChecked(false);
    ui->checkBox_TestShippingCurrent->setChecked(false);

    ui->checkBox_SendMotorCalibration->setChecked(false);
    ui->checkBox_LightTest->setChecked(false);
    ui->checkBox_ServoMotorStart->setChecked(false);
    ui->checkBox_uperMotor->setChecked(false);
    ui->checkBox_PressWindow->setChecked(false);

    ui->checkBox_TestWifiSignal->setChecked(false);
    ui->checkBox_IMULastEnterStartTest->setChecked(false);

    //立讯的p20p要回车开始，木星是按键开始
    //立讯：imu需要晃动唤醒（加快蓝牙连接），全扫码再测试
    if (ui->comboBox_productName->currentText() == "U7") {
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        ui->checkBox_DisableSerialPortRx->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_TestShippingCurrent->setChecked(true);
        ui->checkBox_SendMotorCalibration->setChecked(true);
        ui->checkBox_ServoMotorStart->setChecked(true);
        ui->checkBox_TestAudioCurrent->setChecked(true);

        ui->lineEdit_ButtonThreshold->setText("40");
        ui->lineEdit_ButtonThresholdCount->setText("200");
    }
    //立讯：imu需要晃动唤醒（加快蓝牙连接），全扫码再测试
    if (ui->comboBox_productName->currentText() == "U7P") {
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        ui->checkBox_BluetoothImageTransfer->setChecked(true);
        ui->checkBox_DisableSerialPortRx->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_TestShippingCurrent->setChecked(true);
        ui->checkBox_SendMotorCalibration->setChecked(true);
        ui->checkBox_ServoMotorStart->setChecked(true);
        ui->checkBox_TestAudioCurrent->setChecked(true);

        ui->lineEdit_ButtonThreshold->setText("40");
        ui->lineEdit_ButtonThresholdCount->setText("200");
    }
    //立讯：imu需要晃动唤醒，全扫码再测试
    if (ui->comboBox_productName->currentText() == "F20") {
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
        ui->lineEdit_ButtonThreshold->setText("60");
        ui->lineEdit_ButtonThresholdCount->setText("300");
        ui->lineEdit_BrushThreshold->setText("10");
        ui->lineEdit_BrushThresholdCount->setText("200");
    }
    //立讯：imu需要晃动唤醒，全扫码再测试
    if (ui->comboBox_productName->currentText() == "Q20P") {
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
        ui->checkBox_TestAudioCurrent->setChecked(true);
        ui->checkBox_uperMotor->setChecked(true);
    }
    //欣旺达：依次扫码连接，不需要唤醒
    if (ui->comboBox_productName->currentText() == "Q20") {
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_TestAudioCurrent->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
    }
    //华勤：欣旺达：依次扫码连接，不需要唤醒
    if (ui->comboBox_productName->currentText() == "Y20") {
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->checkBox_PressWindow->setChecked(true);
    }
    //华勤：欣旺达：依次扫码连接，不需要唤醒
    if (ui->comboBox_productName->currentText() == "Y20P") {
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->checkBox_PressWindow->setChecked(true);
        ui->lineEdit_ButtonThreshold->setText("30");
        ui->lineEdit_ButtonThresholdCount->setText("400");
        ui->lineEdit_BrushThreshold->setText("350");
        ui->lineEdit_BrushThresholdCount->setText("300");
    }
    if (ui->comboBox_productName->currentText() == "T10") {
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
    }

    //华勤：欣旺达：依次扫码连接，不需要唤醒
    if (ui->comboBox_productName->currentText() == "Y20PS") {
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->checkBox_PressWindow->setChecked(true);
        ui->lineEdit_ButtonThreshold->setText("40");
        ui->lineEdit_ButtonThresholdCount->setText("400");
    }

    //欣旺达：依次扫标签码连接，需要唤醒
    if (ui->comboBox_productName->currentText() == "Y21") {
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->checkBox_PressWindow->setChecked(true);
        ui->lineEdit_ButtonThreshold->setText("70");
        ui->lineEdit_ButtonThresholdCount->setText("300");
    }
    //立讯：imu不需要晃动唤醒，全扫码再测试
    //华勤：依次扫码连接，不需要唤醒
    if (ui->comboBox_productName->currentText() == "P20P") {
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_MagneticReuseMotorStatus->setChecked(true);
        ui->checkBox_SendMotorCalibration->setChecked(true);
        ui->checkBox_LightTest->setChecked(true);
        ui->checkBox_ServoMotorStart->setChecked(true);

        if (ui->comboBox_factory->currentText() == "lx")
            ui->checkBox_IMULastEnterStartTest->setChecked(true);
    }
    //立讯：imu不需要晃动唤醒，全扫码再测试//原p30ps
    if (ui->comboBox_productName->currentText() == "Y25SE") {
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_LightTest->setChecked(true);
        ui->checkBox_uperMotor->setChecked(true);
        ui->lineEdit_ButtonThreshold->setText("40");
        ui->lineEdit_ButtonThresholdCount->setText("200");
    }
    //立讯：imu不需要晃动唤醒，全扫码再测试
    if (ui->comboBox_productName->currentText() == "Y30S") {
        ui->checkBox_TestAudioCurrent->setChecked(true);
        ui->checkBox_TestShippingCurrent->setChecked(true);
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        // ui->checkBox_NeedWriteSkuid->setChecked(true);
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_uperMotor->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->lineEdit_ButtonThreshold->setText("40");
        ui->lineEdit_ButtonThresholdCount->setText("200");
    }
    //立讯：imu不需要晃动唤醒，全扫码再测试
    if (ui->comboBox_productName->currentText() == "Y30") {
        ui->checkBox_TestAudioCurrent->setChecked(true);
        ui->checkBox_TestShippingCurrent->setChecked(true);
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        // ui->checkBox_NeedWriteSkuid->setChecked(true);
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_uperMotor->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->lineEdit_ButtonThreshold->setText("40");
        ui->lineEdit_ButtonThresholdCount->setText("200");
    }

    //立讯：imu不需要晃动唤醒，全扫码再测试
    if (ui->comboBox_productName->currentText() == "Y30P") {
        ui->checkBox_TestAudioCurrent->setChecked(true);
        ui->checkBox_TestShippingCurrent->setChecked(true);
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        // ui->checkBox_NeedWriteSkuid->setChecked(true);
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_uperMotor->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->lineEdit_BrushThreshold->setText("20");
        ui->lineEdit_BrushThresholdCount->setText("200");
    }

    if (ui->comboBox_productName->currentText() == "P20PS") {
        ui->checkBox_TestShippingCurrent->setChecked(true);
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        // ui->checkBox_NeedWriteSkuid->setChecked(true);
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_LightTest->setChecked(true);
        ui->checkBox_uperMotor->setChecked(true);
    }
}

void qsetting::RestoreFacDefaultSetting() {
    // 隐藏 QLabel 和 QLineEdit 控件
    ui->label_weikesen_order->hide();
    ui->lineEdit_weikesen_order->hide();
    ui->label_78->hide();
    ui->lineEdit_mesUrl->hide();
    ui->label_74->hide();
    ui->lineEdit_macStation->hide();
    ui->label_79->hide();
    ui->lineEdit_processName->hide();
    ui->label_80->hide();
    ui->lineEdit_modelName->hide();
    ui->label_mac_field->hide();
    ui->lineEdit_mac_field->hide();
    ui->label_79->hide();
    ui->label_action_huaqin->hide();
    ui->lineEdit_action_huaqin->hide();
    ui->label_line_huaqin->hide();
    ui->lineEdit_line_huaqin->hide();
    ui->label_mes_login_account->hide();
    ui->lineEdit_mes_login_account->hide();
    ui->label_mes_login_password->hide();
    ui->lineEdit_mes_login_password->hide();
    ui->label_mes_operator->hide();
    ui->lineEdit_mes_operator->hide();
    ui->label_mes_login_station->hide();
    ui->lineEdit_mes_login_station->hide();
    ui->label_mes_retest->hide();
    ui->checkBox_mesRetest->hide();
    ui->label_mes_config_file_path->hide();
    ui->lineEdit_mes_config_file_path->hide();
    ui->pushButton_mesConfigFileBrowse->hide();

    if (ui->comboBox_factory->currentText() == "wks") {
        ui->label_78->show();
        ui->lineEdit_mesUrl->show();
        ui->label_79->show();
        ui->lineEdit_processName->show();
        ui->label_weikesen_order->show();
        ui->lineEdit_weikesen_order->show();
    }
    if (ui->comboBox_factory->currentText() == "ydm") {
        ui->label_78->show();
        ui->lineEdit_mesUrl->show();
        ui->label_80->show();
        ui->lineEdit_modelName->show();
        ui->label_weikesen_order->show();
        ui->lineEdit_weikesen_order->show();
    }
    if (ui->comboBox_factory->currentText() == "xwd") {
        ui->label_74->show();
        ui->lineEdit_macStation->show();
        ui->label_mes_login_account->show();
        ui->lineEdit_mes_login_account->show();
        ui->label_mes_login_password->show();
        ui->lineEdit_mes_login_password->show();
        ui->label_mes_operator->show();
        ui->lineEdit_mes_operator->show();
        ui->label_mes_login_station->show();
        ui->lineEdit_mes_login_station->show();
    }
    if (ui->comboBox_factory->currentText() == "hq") {
        ui->label_action_huaqin->show();
        ui->lineEdit_action_huaqin->show();
        ui->label_line_huaqin->show();
        ui->lineEdit_line_huaqin->show();
        ui->label_mes_login_account->show();
        ui->lineEdit_mes_login_account->show();
        ui->label_mes_login_password->show();
        ui->lineEdit_mes_login_password->show();
    }
    if (ui->comboBox_factory->currentText() == "byd") {
        ui->label_mes_config_file_path->show();
        ui->lineEdit_mes_config_file_path->show();
        ui->pushButton_mesConfigFileBrowse->show();
        ui->label_78->show();
        ui->lineEdit_mesUrl->show();
        ui->label_79->show();
        ui->lineEdit_processName->show();
        ui->label_weikesen_order->show();
        ui->lineEdit_weikesen_order->show();
        ui->label_line_huaqin->show();
        ui->lineEdit_line_huaqin->show();
        ui->label_mes_login_account->show();
        ui->lineEdit_mes_login_account->show();
        ui->label_mes_login_password->show();
        ui->lineEdit_mes_login_password->show();
        ui->label_mes_operator->show();
        ui->lineEdit_mes_operator->show();
        ui->label_mes_login_station->show();
        ui->lineEdit_mes_login_station->show();
    }
    if (ui->comboBox_factory->currentText() == "lx") {
        ui->label_78->show();
        ui->lineEdit_mesUrl->show();
        ui->label_79->show();
        ui->lineEdit_processName->show();
        ui->label_80->show();
        ui->lineEdit_modelName->show();
        ui->label_mac_field->show();
        ui->lineEdit_mac_field->show();
    }
    // 华庄：GET /mrs/checkRoute、/mrs/createRoute；工站号、工单(料号)、操作员、是否重测
    if (ui->comboBox_factory->currentText() == "hz") {
        ui->label_78->show();
        ui->lineEdit_mesUrl->show();
        ui->label_75->show();
        ui->lineEdit_station->show();
        ui->label_weikesen_order->show();
        ui->lineEdit_weikesen_order->show();
        ui->label_mes_operator->show();
        ui->lineEdit_mes_operator->show();
        ui->label_mes_retest->show();
        ui->checkBox_mesRetest->show();
        ui->lineEdit_mesUrl->setToolTip(
            QStringLiteral("华庄 MES 根地址，如 http://10.0.2.5/mrs；程序会自动请求 /checkRoute、/createRoute。"));
        ui->lineEdit_station->setToolTip(QStringLiteral("华庄 stationNo（Mes/machineNo），站前检查与过站共用。"));
        ui->lineEdit_weikesen_order->setToolTip(
            QStringLiteral("华庄 prodNo（Mes/Work_Order）；留空时接口使用 auto。"));
        ui->lineEdit_mes_operator->setToolTip(QStringLiteral("华庄过站 userNo（Mes/mUserno）。"));
    }
    if (ui->comboBox_factory->currentText() == "无mes厂") {
        ui->label_weikesen_order->show();
        ui->lineEdit_weikesen_order->show();
        ui->label_78->show();
        ui->lineEdit_mesUrl->show();
        ui->label_74->show();
        ui->lineEdit_macStation->show();
        ui->label_79->show();
        ui->lineEdit_processName->show();
        ui->label_80->show();
        ui->lineEdit_modelName->show();
        ui->label_mac_field->show();
        ui->lineEdit_mac_field->show();
        ui->label_79->show();
        ui->label_action_huaqin->show();
        ui->lineEdit_action_huaqin->show();
        ui->label_line_huaqin->show();
        ui->lineEdit_line_huaqin->show();
        ui->label_mes_login_account->show();
        ui->lineEdit_mes_login_account->show();
        ui->label_mes_login_password->show();
        ui->lineEdit_mes_login_password->show();
        ui->label_mes_operator->show();
        ui->lineEdit_mes_operator->show();
        ui->label_mes_login_station->show();
        ui->lineEdit_mes_login_station->show();
    }
}
void qsetting::on_comboBox_productName_textActivated(const QString& arg1) {
    qDebug() << "选择的产品" << arg1;
    RestoreProductDefaultSetting();
    if (arg1 == QStringLiteral("V3")) {
        ui->snLineEdit->setText(QStringLiteral("^[0-9a-zA-Z]{12}$"));
    } else if (arg1 == QStringLiteral("V3Pro")) {
        ui->snLineEdit->setText(QStringLiteral("^[0-9a-zA-Z]{15}$"));
    }
}

void qsetting::on_comboBox_factory_textActivated(const QString& arg1) {
    qDebug() << "选择的工厂" << arg1;
    RestoreFacDefaultSetting();
}

void qsetting::on_comboBox_testOrderStation_currentTextChanged(const QString& text) {
    Q_UNUSED(text);
    if (switchingTestOrderStation_ || !ui->comboBox_testOrderStation) {
        qDebug() << "[TestOrder] station changed ignored, switching =" << switchingTestOrderStation_;
        return;
    }
    const QString nextStation = ui->comboBox_testOrderStation->currentData().toString().trimmed();
    qDebug() << "[TestOrder] station changed, current =" << activeTestOrderStation_ << ", next =" << nextStation
             << ", display =" << ui->comboBox_testOrderStation->currentText();
    if (nextStation.isEmpty() || nextStation == activeTestOrderStation_) {
        qDebug() << "[TestOrder] station unchanged/empty, skip reorder";
        return;
    }
    if (testOrderDirty_) {
        const auto result = QMessageBox::question(this, "保存测试流程配置", "当前工站的测试流程已变化，是否保存？",
                                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (result == QMessageBox::Yes) {
            saveCurrentTestOrder();
        } else {
            saveTestOrderIndexes(activeTestOrderStation_, savedTestOrderSnapshot_);
        }
        testOrderDirty_ = false;
    }
    activeTestOrderStation_ = nextStation;
    SETTINGS.setValue(kSelectedStationKey, activeTestOrderStation_);
    SETTINGS.setValue(kSelectedStationNameKey, stationDisplayNameFromKey(activeTestOrderStation_));
    applyStationUiState(activeTestOrderStation_);
    reorderFreeWorkCheckBoxes();
}

void qsetting::on_pushButton_clearConfiguredTestOrder_clicked() {
    if (!freeWorkConfigLayout_ || freeWorkOptionalLayouts_.isEmpty()) {
        return;
    }
    const auto result = QMessageBox::question(this, "确认清空", "确定要清空当前工站的已配置测试流程吗？",
                                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (result != QMessageBox::Yes) {
        return;
    }

    while (QLayoutItem* item = freeWorkConfigLayout_->takeAt(0)) {
        delete item;
    }
    for (QGridLayout* layout : freeWorkOptionalLayouts_) {
        if (!layout) {
            continue;
        }
        while (QLayoutItem* item = layout->takeAt(0)) {
            delete item;
        }
    }

    QVector<QPair<DraggableCheckBox*, bool>> blockedStates;
    blockedStates.reserve(freeWorkCheckBoxes_.size());
    for (DraggableCheckBox* checkBox : freeWorkCheckBoxes_) {
        if (checkBox) {
            blockedStates.append(qMakePair(checkBox, checkBox->blockSignals(true)));
        }
    }

    QHash<QString, int> optionalPosByCategory;
    for (const auto& tab : kFreeWorkOptionalCategoryTabs) {
        optionalPosByCategory.insert(tab.first, 0);
    }
    for (DraggableCheckBox* checkBox : freeWorkCheckBoxes_) {
        if (!checkBox) {
            continue;
        }
        QString cat = categoryKeyForCheckBox(checkBox);
        QGridLayout* grid = freeWorkOptionalLayouts_.value(cat);
        if (!grid) {
            cat = QStringLiteral("product");
            grid = freeWorkOptionalLayouts_.value(cat);
        }
        if (!grid) {
            continue;
        }
        const int pos = optionalPosByCategory.value(cat);
        grid->addWidget(checkBox, pos / freeWorkCols_, pos % freeWorkCols_);
        checkBox->show();
        optionalPosByCategory[cat] = pos + 1;
    }

    for (const auto& state : blockedStates) {
        if (state.first) {
            state.first->blockSignals(state.second);
        }
    }

    freeWorkConfigLayout_->invalidate();
    for (QGridLayout* layout : freeWorkOptionalLayouts_) {
        if (layout) {
            layout->invalidate();
        }
    }
    testOrderDirty_ = true;
    saveCurrentTestOrder();
}

void qsetting::on_pushButton_mesConfigFileBrowse_clicked() {
    QString startDir = QFileInfo(ui->lineEdit_mes_config_file_path->text().trimmed()).absolutePath();
    if (startDir.isEmpty() || !QFileInfo(startDir).isDir()) {
        startDir = QCoreApplication::applicationDirPath();
    }
    const QString path = QFileDialog::getOpenFileName(this, "选择MES配置文件", startDir,
                                                    "配置文件 (*.ini *.json *.xml);;所有文件 (*.*)");
    if (!path.isEmpty()) {
        ui->lineEdit_mes_config_file_path->setText(path);
    }
}

void qsetting::on_pushButton_uploadLogs_clicked() {
    if (!ui->pushButton_uploadLogs->isEnabled()) {
        return;
    }

    const QString uploadUrl = ui->lineEdit_remoteLogUploadUrl->text().trimmed();
    QString deviceId = ui->lineEdit_remoteLogDeviceId->text().trimmed();
    const QString station = ui->lineEdit_remoteLogStation->text().trimmed();

    if (uploadUrl.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("日志上传"), QStringLiteral("请填写上传地址"));
        return;
    }
    if (deviceId.isEmpty()) {
        deviceId = LogUploadService::defaultDeviceId();
        ui->lineEdit_remoteLogDeviceId->setText(deviceId);
    }

    saveConfig();

    LogUploadService::UploadConfig cfg;
    cfg.uploadUrl = uploadUrl;
    cfg.deviceId = deviceId;
    cfg.station = station.isEmpty() ? QStringLiteral("DEFAULT") : station;

    ui->pushButton_uploadLogs->setEnabled(false);
    ui->label_remoteLogStatus->setText(QStringLiteral("正在后台打包并上传 所有log，请稍候…"));

    auto* watcher = new QFutureWatcher<QPair<bool, QString>>(this);
    connect(watcher, &QFutureWatcher<QPair<bool, QString>>::finished, this, [this, watcher]() {
        const QPair<bool, QString> result = watcher->result();
        ui->label_remoteLogStatus->setText(result.second);
        ui->pushButton_uploadLogs->setEnabled(true);
        if (result.first) {
            QMessageBox::information(this, QStringLiteral("日志上传"), result.second);
        } else {
            QMessageBox::warning(this, QStringLiteral("日志上传"), result.second);
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([cfg]() {
        QString message;
        const bool ok = LogUploadService::packAndUpload(cfg, &message);
        return qMakePair(ok, message);
    }));
}

void qsetting::initTestFlowEditorUi() {
    testFlowEditor_ = new TestFlowEditor(this);
    testFlowEditor_->bindUi(this, ui->comboBox_testFlowStation, ui->scrollArea_testFlow,
                            ui->verticalLayout_testFlowBlocks, ui->checkBox_testFlowStopOnTestFail,
                            ui->pushButton_testFlowSave, ui->pushButton_testFlowClear, ui->pushButton_testFlowImport,
                            ui->pushButton_testFlowAdd);
    const int tabIdx = ui->tabWidget->indexOf(ui->tab_test_flow);
    if (tabIdx >= 0) {
        ui->tabWidget->setTabToolTip(tabIdx,
                                     QStringLiteral("编排 test_case/*.ini 与 总的测试流程.ini；不修改旧 TestOrder。"));
    }
    lastSettingsTabIndex_ = ui->tabWidget->currentIndex();
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        const int tabTestFlow = ui->tabWidget->indexOf(ui->tab_test_flow);
        if (tabTestFlow >= 0 && lastSettingsTabIndex_ == tabTestFlow && index != tabTestFlow && testFlowEditor_) {
            if (!testFlowEditor_->confirmDiscardOrSaveOnLeave()) {
                QSignalBlocker blocker(ui->tabWidget);
                ui->tabWidget->setCurrentIndex(tabTestFlow);
                return;
            }
        }
        lastSettingsTabIndex_ = index;
    });
}
