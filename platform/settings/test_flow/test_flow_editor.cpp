#include "test_flow_editor.h"

#include "test_case.h"
#include "test_case_edit_dialog.h"

#include "Abini.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QSet>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QSet>
#include <QSignalBlocker>
#include <QVBoxLayout>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
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

bool flowEntriesEqual(const QVector<TestFlowItemEntry>& a, const QVector<TestFlowItemEntry>& b) {
    if (a.size() != b.size())
        return false;
    for (int i = 0; i < a.size(); ++i) {
        if (a.at(i).caseName != b.at(i).caseName)
            return false;
    }
    return true;
}

} // namespace

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

// ---------- TestFlowEditor ----------

TestFlowEditor::TestFlowEditor(QObject* parent) : QObject(parent) {
}

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

    setupStationComboContextMenu();

    connect(stationCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int newIdx) {
        if (suppressStationChange_)
            return;
        if (!confirmDiscardOrSaveOnLeave()) {
            suppressStationChange_ = true;
            stationCombo_->setCurrentIndex(stationComboPrevIndex_);
            suppressStationChange_ = false;
            return;
        }
        stationComboPrevIndex_ = newIdx;
        persistSelectedStation(currentStationKey());
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

                const QString preview = names.count() > 5 ? names.mid(0, 5).join(QLatin1Char('、')) + QStringLiteral(" 等 %1 项").arg(names.count())
                                                          : names.join(QLatin1Char('、'));
                if (QMessageBox::question(dialogParent_, QStringLiteral("确认删除"),
                                          QStringLiteral("将永久删除 test_case 下以下功能块 ini：\n%1\n\n"
                                                         "若当前编排区已引用，会同步移除对应块（须保存流程后生效）。")
                                              .arg(preview)) != QMessageBox::Yes) {
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

    TestFlowMeta meta;
    TestCaseStore::loadFlowMeta(meta);
    QString initialKey = TestCaseStore::resolveFlowStationKey(
        SETTINGS.value(QStringLiteral("TestOrderMeta/SelectedStation")).toString().trimmed());
    if (initialKey.isEmpty())
        initialKey = TestCaseStore::resolveFlowStationKey(meta.selectedStation.trimmed());
    if (initialKey.isEmpty())
        initialKey = QStringLiteral("FREE_WORK");
    refreshStationCombo(initialKey);
    persistSelectedStation(initialKey);
    reloadCurrentStation();
    stationComboPrevIndex_ = stationCombo_ ? stationCombo_->currentIndex() : 0;
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

void TestFlowEditor::refreshStationCombo(const QString& selectKey) {
    if (!stationCombo_)
        return;
    QString keyToSelect = selectKey.trimmed();
    if (keyToSelect.isEmpty())
        keyToSelect = currentStationKey();
    if (keyToSelect.isEmpty()) {
        TestFlowMeta meta;
        TestCaseStore::loadFlowMeta(meta);
        keyToSelect = meta.selectedStation.trimmed();
    }
    if (keyToSelect.isEmpty())
        keyToSelect = QStringLiteral("FREE_WORK");

    const QVector<TestFlowStationEntry> stations = TestCaseStore::loadFlowStationCatalog();
    QSignalBlocker blocker(stationCombo_);
    stationCombo_->clear();
    for (const TestFlowStationEntry& entry : stations)
        stationCombo_->addItem(entry.displayName, entry.key);

    int idx = stationCombo_->findData(keyToSelect);
    if (idx < 0)
        idx = stationCombo_->findText(TestCaseStore::flowStationDisplayName(keyToSelect));
    if (idx < 0 && !stations.isEmpty())
        idx = 0;
    if (idx >= 0)
        stationCombo_->setCurrentIndex(idx);
}

QMenu* TestFlowEditor::createFlowStationMenu(QWidget* parent, int hitComboIndex) {
    auto* menu = new QMenu(parent);
    QAction* const actNew = menu->addAction(QStringLiteral("新建工站"));
    QAction* const actRename = menu->addAction(QStringLiteral("重命名工站"));
    QAction* const actCopy = menu->addAction(QStringLiteral("复制工站"));
    QAction* const actDel = menu->addAction(QStringLiteral("删除工站"));

    const bool hasStation = stationCombo_ && stationCombo_->count() > 0;
    actRename->setEnabled(hasStation);
    actCopy->setEnabled(hasStation);
    actDel->setEnabled(hasStation);

    connect(actNew, &QAction::triggered, this, [this]() { promptAddFlowStation(); });
    connect(actRename, &QAction::triggered, this, [this, hitComboIndex]() {
        if (hitComboIndex >= 0 && !activateStationComboIndex(hitComboIndex))
            return;
        promptRenameCurrentFlowStation();
    });
    connect(actCopy, &QAction::triggered, this, [this, hitComboIndex]() {
        if (hitComboIndex >= 0 && !activateStationComboIndex(hitComboIndex))
            return;
        promptCopyCurrentFlowStation();
    });
    connect(actDel, &QAction::triggered, this, [this, hitComboIndex]() {
        if (hitComboIndex >= 0 && !activateStationComboIndex(hitComboIndex))
            return;
        promptRemoveCurrentFlowStation();
    });
    return menu;
}

void TestFlowEditor::setupStationComboContextMenu() {
    if (!stationCombo_)
        return;

    stationCombo_->setToolTip(QStringLiteral("选择要编辑的测试工站；数据来自 test_case/总的测试流程.ini"));

    if (auto* manageBtn =
            stationCombo_->parentWidget()->findChild<QToolButton*>(QStringLiteral("toolButton_testFlowStationMenu"))) {
        manageBtn->setMenu(createFlowStationMenu(manageBtn, stationCombo_->currentIndex()));
        manageBtn->setPopupMode(QToolButton::InstantPopup);
        connect(stationCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), manageBtn,
                [this, manageBtn](int) {
                    manageBtn->setMenu(createFlowStationMenu(manageBtn, stationCombo_->currentIndex()));
                });
    }

    stationCombo_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(stationCombo_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu* const menu = createFlowStationMenu(stationCombo_, stationCombo_->currentIndex());
        menu->exec(stationCombo_->mapToGlobal(pos));
        menu->deleteLater();
    });

    auto* stationView = new QListView(stationCombo_);
    stationView->setContextMenuPolicy(Qt::CustomContextMenu);
    stationCombo_->setView(stationView);
    connect(stationView, &QWidget::customContextMenuRequested, this, [this, stationView](const QPoint& pos) {
        const QModelIndex modelIndex = stationView->indexAt(pos);
        const int row = modelIndex.isValid() ? modelIndex.row() : stationCombo_->currentIndex();
        QMenu* const menu = createFlowStationMenu(stationView, row);
        menu->exec(stationView->viewport()->mapToGlobal(pos));
        menu->deleteLater();
    });
}

bool TestFlowEditor::activateStationComboIndex(int index) {
    if (!stationCombo_ || index < 0 || index >= stationCombo_->count())
        return false;
    if (index == stationCombo_->currentIndex())
        return true;
    if (!confirmDiscardOrSaveOnLeave())
        return false;
    suppressStationChange_ = true;
    stationCombo_->setCurrentIndex(index);
    stationComboPrevIndex_ = index;
    suppressStationChange_ = false;
    persistSelectedStation(currentStationKey());
    reloadCurrentStation();
    return true;
}

void TestFlowEditor::promptAddFlowStation() {
    if (!dialogParent_)
        return;

    QDialog dlg(dialogParent_);
    dlg.setWindowTitle(QStringLiteral("新建工站"));
    auto* form = new QFormLayout(&dlg);
    auto* editName = new QLineEdit(&dlg);
    editName->setPlaceholderText(QStringLiteral("例如：自由工站、吸力测试工站"));
    form->addRow(QStringLiteral("工站名称"), editName);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString displayName = editName->text().trimmed();
    QString err;
    if (!TestCaseStore::addFlowStation(displayName, &err)) {
        QMessageBox::warning(dialogParent_, QStringLiteral("无法新建"), err);
        return;
    }
    refreshStationCombo(TestCaseStore::resolveFlowStationKey(displayName));
    persistSelectedStation(currentStationKey());
    reloadCurrentStation();
}

void TestFlowEditor::promptRenameCurrentFlowStation() {
    if (!dialogParent_ || !stationCombo_)
        return;
    const QString key = currentStationKey();
    if (key.isEmpty())
        return;

    const QString oldName = TestCaseStore::flowStationDisplayName(key);

    QDialog dlg(dialogParent_);
    dlg.setWindowTitle(QStringLiteral("重命名工站"));
    auto* form = new QFormLayout(&dlg);
    auto* editName = new QLineEdit(oldName, &dlg);
    editName->setPlaceholderText(QStringLiteral("仅改显示名称，工站内部键不变"));
    editName->selectAll();
    form->addRow(QStringLiteral("工站名称"), editName);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString newName = editName->text().trimmed();
    if (newName == oldName)
        return;

    QString err;
    if (!TestCaseStore::renameFlowStation(key, newName, &err)) {
        QMessageBox::warning(dialogParent_, QStringLiteral("无法重命名"), err);
        return;
    }
    refreshStationCombo(key);
    persistSelectedStation(key);
}

void TestFlowEditor::promptCopyCurrentFlowStation() {
    if (!dialogParent_ || !stationCombo_)
        return;
    const QString sourceKey = currentStationKey();
    if (sourceKey.isEmpty())
        return;

    const QString sourceLabel = stationCombo_->currentText();
    const QVector<TestFlowStationEntry> catalog = TestCaseStore::loadFlowStationCatalog();
    auto displayNameTaken = [&catalog](const QString& name) {
        for (const TestFlowStationEntry& entry : catalog) {
            if (entry.displayName == name)
                return true;
        }
        return false;
    };
    QString defaultName = sourceLabel + QStringLiteral(" 副本");
    if (displayNameTaken(defaultName)) {
        for (int n = 2; n < 100; ++n) {
            const QString candidate = sourceLabel + QStringLiteral(" 副本%1").arg(n);
            if (!displayNameTaken(candidate)) {
                defaultName = candidate;
                break;
            }
        }
    }

    QDialog dlg(dialogParent_);
    dlg.setWindowTitle(QStringLiteral("复制工站"));
    auto* form = new QFormLayout(&dlg);
    auto* editName = new QLineEdit(defaultName, &dlg);
    editName->setPlaceholderText(QStringLiteral("新工站显示名称"));
    editName->selectAll();
    form->addRow(QStringLiteral("源工站"), new QLabel(sourceLabel, &dlg));
    form->addRow(QStringLiteral("新工站名称"), editName);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString newName = editName->text().trimmed();
    const QVector<TestFlowItemEntry> entries = currentFlowEntries();
    const bool stopFlow =
        stopFlowOnTestFailCheck_ ? stopFlowOnTestFailCheck_->isChecked() : true;

    QString newKey;
    QString err;
    if (!TestCaseStore::copyFlowStation(sourceKey, newName, entries, stopFlow, &newKey, &err)) {
        QMessageBox::warning(dialogParent_, QStringLiteral("无法复制"), err);
        return;
    }

    lastLoadedStationKey_.clear();
    refreshStationCombo(newKey);
    persistSelectedStation(newKey);
    reloadCurrentStation();
    QMessageBox::information(dialogParent_, QStringLiteral("已复制"),
                             QStringLiteral("已创建工站「%1」，流程已写入 总的测试流程.ini")
                                 .arg(TestCaseStore::flowStationDisplayName(newKey)));
}

void TestFlowEditor::promptRemoveCurrentFlowStation() {
    if (!dialogParent_)
        return;
    const QString key = currentStationKey();
    if (key.isEmpty())
        return;
    const QString displayName = TestCaseStore::flowStationDisplayName(key);
    if (displayName == QStringLiteral("默认工站") || displayName == QStringLiteral("自由工站")) {
        QMessageBox::warning(dialogParent_, QStringLiteral("无法删除"),
                             QStringLiteral("内置工站「默认工站」「自由工站」不可删除"));
        return;
    }
    const QString label = stationCombo_->currentText();
    if (QMessageBox::question(dialogParent_, QStringLiteral("确认删除"),
                              QStringLiteral("删除工站「%1」及其在 总的测试流程.ini 中的流程配置？\n"
                                             "不会删除 test_case 下的功能块 ini。")
                                  .arg(label)) != QMessageBox::Yes) {
        return;
    }
    QString err;
    if (!TestCaseStore::removeFlowStation(key, &err)) {
        QMessageBox::warning(dialogParent_, QStringLiteral("删除失败"), err);
        return;
    }
    refreshStationCombo(QStringLiteral("FREE_WORK"));
    persistSelectedStation(currentStationKey());
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

void TestFlowEditor::persistSelectedStation(const QString& key) {
    const QString resolved = TestCaseStore::resolveFlowStationKey(key.trimmed());
    if (resolved.isEmpty())
        return;

    const QString displayName = TestCaseStore::flowStationDisplayName(resolved);
    TestFlowMeta meta;
    TestCaseStore::loadFlowMeta(meta);
    meta.selectedStation = resolved;
    meta.selectedStationName = displayName;
    TestCaseStore::saveFlowMeta(meta);
    SETTINGS.setValue(QStringLiteral("TestOrderMeta/SelectedStation"), resolved);
    SETTINGS.setValue(QStringLiteral("TestOrderMeta/SelectedStationName"), displayName);
}

bool TestFlowEditor::saveStationFlow(const QString& stationKey) {
    const QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    if (key.isEmpty()) {
        QMessageBox::warning(dialogParent_, QStringLiteral("保存失败"), QStringLiteral("工站无效"));
        return false;
    }
    const QVector<TestFlowItemEntry> entries = currentFlowEntries();
    for (const TestFlowItemEntry& entry : entries) {
        if (!QFile::exists(TestCasePaths::caseIniPath(entry.caseName))) {
            QMessageBox::warning(dialogParent_, QStringLiteral("保存失败"),
                                 QStringLiteral("case 不存在: %1，请先保存配置").arg(entry.caseName));
            return false;
        }
    }
    const bool stopFlowOnTestFail =
        stopFlowOnTestFailCheck_ ? stopFlowOnTestFailCheck_->isChecked() : true;
    TestCaseStore::saveStationFlowItems(key, entries, stopFlowOnTestFail);
    persistSelectedStation(key);
    lastLoadedStationKey_ = key;
    updateSavedSnapshot();
    return true;
}

void TestFlowEditor::saveCurrentFlow() {
    if (!saveStationFlow(currentStationKey()))
        return;
    QMessageBox::information(dialogParent_, QStringLiteral("已保存"),
                             QStringLiteral("流程已写入 test_case/总的测试流程.ini"));
}

void TestFlowEditor::updateSavedSnapshot() {
    savedEntriesSnapshot_ = currentFlowEntries();
    savedStopFlowOnTestFail_ =
        stopFlowOnTestFailCheck_ ? stopFlowOnTestFailCheck_->isChecked() : true;
}

bool TestFlowEditor::hasUnsavedChanges() const {
    if (!uiBound_ || !flowLayout_)
        return false;
    if (!flowEntriesEqual(currentFlowEntries(), savedEntriesSnapshot_))
        return true;
    if (stopFlowOnTestFailCheck_ && stopFlowOnTestFailCheck_->isChecked() != savedStopFlowOnTestFail_) {
        return true;
    }
    return false;
}

bool TestFlowEditor::confirmDiscardOrSaveOnLeave() {
    if (!hasUnsavedChanges())
        return true;

    QMessageBox box(dialogParent_);
    box.setWindowTitle(QStringLiteral("未保存的更改"));
    box.setText(QStringLiteral("测试流程编排已修改，是否保存？"));
    box.setIcon(QMessageBox::Question);
    auto* btnSave = box.addButton(QStringLiteral("保存"), QMessageBox::AcceptRole);
    auto* btnDiscard = box.addButton(QStringLiteral("不保存"), QMessageBox::DestructiveRole);
    auto* btnCancel = box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(btnSave);
    box.exec();

    if (box.clickedButton() == btnCancel)
        return false;
    if (box.clickedButton() == btnSave) {
        const QString key = lastLoadedStationKey_.isEmpty() ? currentStationKey() : lastLoadedStationKey_;
        if (!saveStationFlow(key))
            return false;
        return true;
    }
    reloadCurrentStation();
    return true;
}

void TestFlowEditor::reloadCurrentStation() {
    const QString key = currentStationKey();
    lastLoadedStationKey_ = key;
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
    updateSavedSnapshot();
}

void TestFlowEditor::openEditDialog(TestCaseBlock* block) {
    if (!block || !dialogParent_)
        return;
    setSelectedBlock(block);

    if (QPointer<TestCaseEditDialog> existing = openEditDialogs_.value(block)) {
        existing->raise();
        existing->activateWindow();
        return;
    }

    const QString oldFlowKey = block->caseName();
    auto* dlg = new TestCaseEditDialog(dialogParent_);
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->setWindowModality(Qt::NonModal);
    dlg->setWindowFlag(Qt::Window, true);

    if (!block->isBlank()) {
        TestCaseDefinition def;
        TestCaseStore::loadCase(block->caseName(), def);
        dlg->setDefinition(def, block->caseName());
        dlg->setWindowTitle(QStringLiteral("测试项配置 - %1").arg(block->caseName()));
    } else {
        dlg->setWindowTitle(QStringLiteral("测试项配置 - 新步骤"));
    }

    openEditDialogs_.insert(block, dlg);
    connect(dlg, &QObject::destroyed, this, [this, block]() { openEditDialogs_.remove(block); });

    QPointer<TestCaseBlock> blockGuard(block);
    QPointer<TestCaseEditDialog> dlgGuard(dlg);
    connect(dlg, &QDialog::accepted, this, [this, blockGuard, dlgGuard, oldFlowKey]() {
        if (!blockGuard || !dlgGuard)
            return;
        const TestCaseDefinition saved = dlgGuard->definition();
        blockGuard->setCaseName(saved.meta.name);
        if (!oldFlowKey.isEmpty() && oldFlowKey != saved.meta.name)
            TestCaseStore::saveStationFlowItems(currentStationKey(), currentFlowEntries());
    });

    dlg->show();
}
