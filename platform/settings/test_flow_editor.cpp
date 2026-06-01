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
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QSet>
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
// 专用 MIME，避免与 qsetting 自由工站 setText(索引) 拖放冲突
const char kTestCaseFlowMime[] = "application/x-testcase-flow-block";

QVBoxLayout* flowLayoutOfBlock(TestCaseBlock* block) {
    if (!block || !block->parentWidget())
        return nullptr;
    return qobject_cast<QVBoxLayout*>(block->parentWidget()->layout());
}

/** 将 src 插入到 layout 的 insertIndex 位置（先 remove 再 insert）。 */
void moveBlockInFlowLayout(QVBoxLayout* layout, TestCaseBlock* src, int insertIndex) {
    if (!layout || !src)
        return;
    const int from = layout->indexOf(src);
    if (from < 0)
        return;
    insertIndex = qBound(0, insertIndex, layout->count());
    if (insertIndex == from)
        return;
    layout->removeWidget(src);
    if (from < insertIndex)
        --insertIndex;
    layout->insertWidget(insertIndex, src);
}

bool acceptsTestCaseFlowMime(const QMimeData* mime) {
    return mime && mime->hasFormat(QLatin1String(kTestCaseFlowMime));
}

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
    mime->setData(QLatin1String(kTestCaseFlowMime), QByteArray("1"));
    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction);
}

void TestCaseBlock::dragEnterEvent(QDragEnterEvent* event) {
    if (acceptsTestCaseFlowMime(event->mimeData()))
        event->acceptProposedAction();
    else
        QCheckBox::dragEnterEvent(event);
}

void TestCaseBlock::dragMoveEvent(QDragMoveEvent* event) {
    if (acceptsTestCaseFlowMime(event->mimeData()))
        event->acceptProposedAction();
    else
        QCheckBox::dragMoveEvent(event);
}

void TestCaseBlock::dropEvent(QDropEvent* event) {
    if (!acceptsTestCaseFlowMime(event->mimeData())) {
        QCheckBox::dropEvent(event);
        return;
    }
    auto* src = qobject_cast<TestCaseBlock*>(event->source());
    if (!src || src == this) {
        event->ignore();
        return;
    }
    QVBoxLayout* layout = flowLayoutOfBlock(this);
    if (!layout) {
        event->ignore();
        return;
    }
    const int overIdx = layout->indexOf(this);
    if (overIdx < 0) {
        event->ignore();
        return;
    }
    const bool insertAfter = event->pos().y() > height() / 2;
    moveBlockInFlowLayout(layout, src, insertAfter ? overIdx + 1 : overIdx);
    event->acceptProposedAction();
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

    flowContainer_ = scroll_->widget();
    if (flowContainer_) {
        flowContainer_->setAcceptDrops(true);
        flowContainer_->installEventFilter(this);
    }

    if (auto* toolbar = stationCombo_->parentWidget()->findChild<QHBoxLayout*>(QStringLiteral("horizontalLayout_testFlowToolbar"))) {
        auto* btnUp = new QPushButton(QStringLiteral("上移"), stationCombo_->parentWidget());
        auto* btnDown = new QPushButton(QStringLiteral("下移"), stationCombo_->parentWidget());
        btnUp->setToolTip(QStringLiteral("将当前选中的功能块上移一步"));
        btnDown->setToolTip(QStringLiteral("将当前选中的功能块下移一步"));
        const int saveIdx = toolbar->indexOf(btnSave);
        const int insertAt = saveIdx >= 0 ? saveIdx : toolbar->count();
        toolbar->insertWidget(insertAt, btnUp);
        toolbar->insertWidget(insertAt + 1, btnDown);
        connect(btnUp, &QPushButton::clicked, this, [this]() { moveSelectedBlock(-1); });
        connect(btnDown, &QPushButton::clicked, this, [this]() { moveSelectedBlock(1); });
    }

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
        pickDlg.setMinimumSize(420, 480);

        QStringList allNames = TestCaseStore::listCaseIniNames();

        auto* searchEdit = new QLineEdit(&pickDlg);
        searchEdit->setClearButtonEnabled(true);
        searchEdit->setPlaceholderText(QStringLiteral("搜索功能块名称（支持模糊匹配）"));

        auto* list = new QListWidget(&pickDlg);
        list->setSelectionMode(QAbstractItemView::ExtendedSelection);
        list->setContextMenuPolicy(Qt::CustomContextMenu);

        auto applyCaseFilter = [list, &allNames](const QString& keyword) {
            const QString key = keyword.trimmed();
            list->clear();
            for (const QString& name : allNames) {
                if (key.isEmpty() || name.contains(key, Qt::CaseInsensitive))
                    list->addItem(name);
            }
        };
        applyCaseFilter(QString());
        connect(searchEdit, &QLineEdit::textChanged, &pickDlg, applyCaseFilter);

        connect(list, &QWidget::customContextMenuRequested, &pickDlg, [this, list, &allNames, searchEdit](const QPoint& pos) {
            QList<QListWidgetItem*> targets = list->selectedItems();
            if (targets.isEmpty()) {
                if (QListWidgetItem* item = list->itemAt(pos))
                    targets.append(item);
            }
            if (targets.isEmpty())
                return;

            QMenu menu(list);
            menu.addAction(QStringLiteral("删除功能块"), [this, list, &allNames, searchEdit, targets]() {
                QStringList names;
                for (QListWidgetItem* item : targets) {
                    const QString name = item->text().trimmed();
                    if (!name.isEmpty())
                        names.append(name);
                }
                if (names.isEmpty())
                    return;

                const QString preview = names.count() > 5 ? names.mid(0, 5).join(QLatin1Char('、'))
                                                                + QStringLiteral(" 等 %1 项").arg(names.count())
                                                          : names.join(QLatin1Char('、'));
                if (QMessageBox::question(dialogParent_, QStringLiteral("确认删除"),
                                          QStringLiteral("将永久删除 test_case 下以下功能块 ini：\n%1\n\n"
                                                         "若当前编排区已引用，会同步移除对应块（须保存流程后生效）。")
                                              .arg(preview))
                    != QMessageBox::Yes) {
                    return;
                }

                QSet<QString> deleted;
                for (const QString& name : names) {
                    if (TestCasePaths::isReservedCaseName(name)) {
                        QMessageBox::warning(dialogParent_, QStringLiteral("无法删除"),
                                             QStringLiteral("保留名称不可删除：%1").arg(name));
                        continue;
                    }
                    const QString path = TestCasePaths::caseIniPath(name);
                    if (!QFile::exists(path)) {
                        allNames.removeAll(name);
                        deleted.insert(name);
                        continue;
                    }
                    if (!QFile::remove(path)) {
                        QMessageBox::warning(dialogParent_, QStringLiteral("删除失败"),
                                             QStringLiteral("无法删除文件：%1").arg(path));
                        continue;
                    }
                    allNames.removeAll(name);
                    deleted.insert(name);
                }
                if (deleted.isEmpty())
                    return;

                if (flowLayout_) {
                    for (int i = flowLayout_->count() - 1; i >= 0; --i) {
                        auto* block = qobject_cast<TestCaseBlock*>(flowLayout_->itemAt(i)->widget());
                        if (!block || block->isBlank())
                            continue;
                        if (deleted.contains(block->caseName())) {
                            if (selectedBlock_ == block)
                                setSelectedBlock(nullptr);
                            flowLayout_->removeWidget(block);
                            block->deleteLater();
                        }
                    }
                }
                const QString key = searchEdit->text().trimmed();
                list->clear();
                for (const QString& name : allNames) {
                    if (key.isEmpty() || name.contains(key, Qt::CaseInsensitive))
                        list->addItem(name);
                }
            });
            menu.exec(list->mapToGlobal(pos));
        });

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &pickDlg);
        auto* lay = new QVBoxLayout(&pickDlg);
        lay->addWidget(searchEdit);
        lay->addWidget(list, 1);
        lay->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &pickDlg, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &pickDlg, &QDialog::reject);
        searchEdit->setFocus();
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
    if (scroll_) {
        scroll_->setToolTip(QStringLiteral("拖拽功能块到目标位置：落在某块上半部=插到其前，下半部=插到其后；"
                                           "也可选中块后点「上移」「下移」。调整完须点「保存流程」。"));
    }

    reloadCurrentStation();
}

void TestFlowEditor::moveSelectedBlock(int delta) {
    if (!flowLayout_ || !selectedBlock_ || delta == 0)
        return;
    const int from = flowLayout_->indexOf(selectedBlock_);
    const int to = from + delta;
    if (from < 0 || to < 0 || to >= flowLayout_->count())
        return;
    flowLayout_->removeWidget(selectedBlock_);
    flowLayout_->insertWidget(to, selectedBlock_);
}

bool TestFlowEditor::eventFilter(QObject* watched, QEvent* event) {
    if (watched == flowContainer_ && flowLayout_) {
        switch (event->type()) {
        case QEvent::DragEnter: {
            auto* e = static_cast<QDragEnterEvent*>(event);
            if (acceptsTestCaseFlowMime(e->mimeData()))
                e->acceptProposedAction();
            return true;
        }
        case QEvent::DragMove: {
            auto* e = static_cast<QDragMoveEvent*>(event);
            if (acceptsTestCaseFlowMime(e->mimeData()))
                e->acceptProposedAction();
            return true;
        }
        case QEvent::Drop: {
            auto* e = static_cast<QDropEvent*>(event);
            if (!acceptsTestCaseFlowMime(e->mimeData()))
                return true;
            auto* src = qobject_cast<TestCaseBlock*>(e->source());
            if (src) {
                flowLayout_->removeWidget(src);
                flowLayout_->addWidget(src);
                e->acceptProposedAction();
            }
            return true;
        }
        default:
            break;
        }
    }
    return QObject::eventFilter(watched, event);
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
