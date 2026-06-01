#include "test_flow_editor.h"

#include "test_case.h"
#include "widgets/test_case_edit_dialog.h"

#include "Abini.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDrag>
#include <QFile>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QVBoxLayout>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace {

const char* kBlockStyleNormal = "border: 2px solid rgb(255, 165, 0); padding: 4px;";
const char* kBlockStyleSelected = "border: 2px solid rgb(30, 144, 255); padding: 4px;";

}  // namespace

// ---------- TestCaseBlock ----------

TestCaseBlock::TestCaseBlock(const QString& caseName, QWidget* parent)
    : QCheckBox(parent), caseName_(caseName) {
    setAcceptDrops(true);
    setCheckState(Qt::Checked);
    setCaseName(caseName);
    updateBlockStyle();
}

QString TestCaseBlock::caseName() const {
    return caseName_;
}

void TestCaseBlock::setCaseName(const QString& name) {
    caseName_ = name.trimmed();
    if (caseName_.isEmpty())
        setText(QStringLiteral("(空白块)"));
    else
        setText(caseName_);
}

void TestCaseBlock::setSelected(bool selected) {
    if (selected_ == selected)
        return;
    selected_ = selected;
    updateBlockStyle();
}

void TestCaseBlock::updateBlockStyle() {
    setStyleSheet(QString::fromUtf8(selected_ ? kBlockStyleSelected : kBlockStyleNormal));
}

void TestCaseBlock::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        startPos_ = event->pos();
        emit blockSelected(this);
    }
    QCheckBox::mousePressEvent(event);
}

void TestCaseBlock::mouseMoveEvent(QMouseEvent* event) {
    if ((event->buttons() & Qt::LeftButton) && (event->pos() - startPos_).manhattanLength() >= QApplication::startDragDistance())
        performDrag();
}

void TestCaseBlock::performDrag() {
    auto* mime = new QMimeData;
    mime->setText(caseName_.isEmpty() ? QStringLiteral("__blank__") : caseName_);
    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction);
}

void TestCaseBlock::contextMenuEvent(QContextMenuEvent* event) {
    emit blockSelected(this);
    QMenu menu(this);
    menu.addAction(QStringLiteral("打开设置"), this, [this]() { emit editRequested(this); });
    menu.addAction(QStringLiteral("从流程移除"), this, [this]() { emit removeFromFlowRequested(this); });
    menu.exec(event->globalPos());
}

// ---------- 工站列表（本文件内） ----------

namespace {

struct TestFlowStationItem {
    QString name;
    QString key;
};

const QVector<TestFlowStationItem> kPresetStations = {
    {QStringLiteral("default"), QStringLiteral("default")},
    {QStringLiteral("自由工站"), QStringLiteral("FREE_WORK")},
    {QStringLiteral("吸力测试工站"), QStringLiteral("SUCTION_TEST")},
};

QString stationKeyFromDisplayName(const QString& raw) {
    const QString t = raw.trimmed();
    for (const auto& p : kPresetStations) {
        if (p.name == t || p.key == t)
            return p.key;
    }
    return t;
}

QString stationDisplayNameFromKey(const QString& key) {
    for (const auto& p : kPresetStations) {
        if (p.key == key)
            return p.name;
    }
    return key;
}

QVector<TestFlowStationItem> collectTestFlowStations() {
    QSet<QString> unique;
    QVector<TestFlowStationItem> stations;
    auto append = [&](const QString& name, const QString& key) {
        const QString k = key.trimmed();
        if (k.isEmpty() || unique.contains(k))
            return;
        unique.insert(k);
        stations.append({name.trimmed().isEmpty() ? stationDisplayNameFromKey(k) : name.trimmed(), k});
    };

    for (const auto& p : kPresetStations)
        append(p.name, p.key);

    append(QString(), SETTINGS.value(QStringLiteral("TestOrderMeta/SelectedStationName")).toString());
    append(QString(), SETTINGS.value(QStringLiteral("TestOrderMeta/SelectedStation")).toString());

    for (const QString& k : TestCaseStore::listStationKeysFromFlow())
        append(stationDisplayNameFromKey(k), k);

    SETTINGS.beginGroup(QStringLiteral("TestOrder"));
    for (const QString& g : SETTINGS.childGroups())
        append(g, stationKeyFromDisplayName(g));
    SETTINGS.endGroup();

    const QStringList rootGroups = SETTINGS.childGroups();
    for (const QString& g : rootGroups) {
        if (g.startsWith(QStringLiteral("TestOrder_")))
            append(stationDisplayNameFromKey(g.mid(11)), g.mid(11));
    }

    if (stations.isEmpty())
        stations.append({QStringLiteral("default"), QStringLiteral("default")});
    return stations;
}

}  // namespace

// ---------- TestFlowEditor ----------

TestFlowEditor::TestFlowEditor(QObject* parent) : QObject(parent) {}

void TestFlowEditor::bindUi(QWidget* dialogParent, QComboBox* stationCombo, QScrollArea* scroll, QVBoxLayout* flowLayout,
                            QCheckBox* stopFlowOnTestFailCheck, QPushButton* btnSave, QPushButton* btnClear,
                            QPushButton* btnImport, QPushButton* btnAdd) {
    if (uiBound_)
        return;
    uiBound_ = true;

    dialogParent_ = dialogParent;
    stationCombo_ = stationCombo;
    scroll_ = scroll;
    flowLayout_ = flowLayout;
    stopFlowOnTestFailCheck_ = stopFlowOnTestFailCheck;

    if (!stationCombo_ || !scroll_ || !flowLayout_)
        return;

    {
        QSignalBlocker blocker(stationCombo_);
        stationCombo_->clear();
        const auto stations = collectTestFlowStations();
        for (const auto& s : stations)
            stationCombo_->addItem(s.name, s.key);
    }

    connect(stationCombo_, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
        reloadCurrentStation();
    });
    connect(btnSave, &QPushButton::clicked, this, [this]() { saveCurrentFlow(); });
    connect(btnClear, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(dialogParent_, QStringLiteral("确认"),
                                  QStringLiteral("清空当前工站编排区？不会删除磁盘 ini")) == QMessageBox::Yes) {
            clearBlocks();
        }
    });
    connect(btnAdd, &QPushButton::clicked, this, [this]() { appendBlock(QString()); });
    connect(btnImport, &QPushButton::clicked, this, [this]() {
        QDialog pickDlg(dialogParent_);
        pickDlg.setWindowTitle(QStringLiteral("选择已有功能块"));
        auto* list = new QListWidget(&pickDlg);
        list->setSelectionMode(QAbstractItemView::ExtendedSelection);
        for (const QString& name : TestCaseStore::listCaseIniNames())
            list->addItem(name);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &pickDlg);
        auto* lay = new QVBoxLayout(&pickDlg);
        lay->addWidget(list);
        lay->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &pickDlg, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &pickDlg, &QDialog::reject);
        list->clearSelection();
        if (pickDlg.exec() != QDialog::Accepted)
            return;
        for (QListWidgetItem* item : list->selectedItems())
            appendBlock(item->text());
    });

    if (stopFlowOnTestFailCheck_) {
        stopFlowOnTestFailCheck_->setToolTip(
            QStringLiteral("勾选后，本工站任一步测试失败（含卡控、超时、钩子等）即结束整单，不再执行后续步骤"));
    }

    reloadCurrentStation();
}

void TestFlowEditor::setSelectedBlock(TestCaseBlock* block) {
    if (selectedBlock_ == block)
        return;
    if (selectedBlock_)
        selectedBlock_->setSelected(false);
    selectedBlock_ = block;
    if (selectedBlock_)
        selectedBlock_->setSelected(true);
}

void TestFlowEditor::clearBlocks() {
    if (!flowLayout_)
        return;
    selectedBlock_ = nullptr;
    while (QLayoutItem* item = flowLayout_->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void TestFlowEditor::appendBlock(const QString& caseName) {
    if (!scroll_ || !flowLayout_)
        return;
    QWidget* container = scroll_->widget();
    if (!container)
        return;
    auto* block = new TestCaseBlock(caseName, container);
    connect(block, &TestCaseBlock::editRequested, this, &TestFlowEditor::openEditDialog);
    connect(block, &TestCaseBlock::removeFromFlowRequested, this, [this](TestCaseBlock* b) {
        if (selectedBlock_ == b)
            setSelectedBlock(nullptr);
        flowLayout_->removeWidget(b);
        b->deleteLater();
    });
    connect(block, &TestCaseBlock::blockSelected, this, [this](TestCaseBlock* b) { setSelectedBlock(b); });
    flowLayout_->addWidget(block);
    setSelectedBlock(block);
}

QString TestFlowEditor::currentStationKey() const {
    return stationCombo_ ? stationCombo_->currentData().toString().trimmed() : QString();
}

QVector<TestFlowItemEntry> TestFlowEditor::currentFlowEntries() const {
    QVector<TestFlowItemEntry> entries;
    if (!flowLayout_)
        return entries;
    for (int i = 0; i < flowLayout_->count(); ++i) {
        auto* w = flowLayout_->itemAt(i)->widget();
        auto* block = qobject_cast<TestCaseBlock*>(w);
        if (!block || block->isBlank())
            continue;
        TestFlowItemEntry entry;
        entry.caseName = block->caseName();
        entries.append(entry);
    }
    return entries;
}

void TestFlowEditor::saveCurrentFlow() {
    const QString key = currentStationKey();
    const QVector<TestFlowItemEntry> entries = currentFlowEntries();
    for (const TestFlowItemEntry& entry : entries) {
        if (!QFile::exists(TestCasePaths::caseIniPath(entry.caseName))) {
            QMessageBox::warning(dialogParent_, QStringLiteral("保存失败"),
                                 QStringLiteral("case 不存在: %1，请先保存配置").arg(entry.caseName));
            return;
        }
    }
    const bool stopFlowOnTestFail =
        stopFlowOnTestFailCheck_ ? stopFlowOnTestFailCheck_->isChecked() : true;
    TestCaseStore::saveStationFlowItems(key, entries, stopFlowOnTestFail);
    TestFlowMeta meta;
    TestCaseStore::loadFlowMeta(meta);
    meta.selectedStation = key;
    meta.selectedStationName = stationCombo_->currentText();
    TestCaseStore::saveFlowMeta(meta);
    SETTINGS.setValue(QStringLiteral("TestOrderMeta/SelectedStation"), key);
    SETTINGS.setValue(QStringLiteral("TestOrderMeta/SelectedStationName"), meta.selectedStationName);
    QMessageBox::information(dialogParent_, QStringLiteral("已保存"),
                             QStringLiteral("流程已写入 test_case/总的测试流程.ini"));
}

void TestFlowEditor::reloadCurrentStation() {
    const QString key = currentStationKey();
    clearBlocks();
    if (stopFlowOnTestFailCheck_) {
        QSignalBlocker blocker(stopFlowOnTestFailCheck_);
        stopFlowOnTestFailCheck_->setChecked(TestCaseStore::loadStationStopFlowOnTestFail(key, true));
    }
    const QVector<TestFlowItemEntry> entries = TestCaseStore::loadStationFlowItems(key);
    for (const TestFlowItemEntry& entry : entries)
        appendBlock(entry.caseName);
    if (flowLayout_ && flowLayout_->count() > 0) {
        if (auto* block = qobject_cast<TestCaseBlock*>(flowLayout_->itemAt(0)->widget()))
            setSelectedBlock(block);
    }
}

void TestFlowEditor::openEditDialog(TestCaseBlock* block) {
    if (!block || !dialogParent_)
        return;
    setSelectedBlock(block);
    TestCaseEditDialog dlg(dialogParent_);
    if (!block->isBlank()) {
        TestCaseDefinition def;
        TestCaseStore::loadCase(block->caseName(), def);
        dlg.setDefinition(def, block->caseName());
    }
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QString oldFlowKey = block->caseName();
    const TestCaseDefinition saved = dlg.definition();
    block->setCaseName(saved.meta.name);
    if (!oldFlowKey.isEmpty() && oldFlowKey != saved.meta.name)
        TestCaseStore::saveStationFlowItems(currentStationKey(), currentFlowEntries());
}
