#include "test_flow_editor.h"

#include "test_case.h"
#include "test_case_edit_dialog.h"
#include "test_case_sync_service.h"
#include "box_base.h"

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
#include <QFontMetrics>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QSet>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleOptionButton>
#include <QToolButton>
#include <QSet>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QtConcurrent>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

/** 流程编排 QSS（功能块 + 右键菜单），见 stytle/qss/test_flow_editor.qss */
const QString& testFlowEditorStyleSheet() {
    static const QString css = []() -> QString {
        QFile f(QStringLiteral(":/stytle/qss/test_flow_editor.qss"));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning("test_flow_editor: 无法加载 :/stytle/qss/test_flow_editor.qss");
            return QString();
        }
        return QString::fromUtf8(f.readAll());
    }();
    return css;
}

void applyFlowContextMenuStyle(QMenu* menu) {
    if (menu)
        menu->setStyleSheet(testFlowEditorStyleSheet());
}

/** 弹出列表按最长工站名加宽，避免「Wellness Warm…」截断；闭合框仍用原宽度，悬停看 ToolTip */
void adjustComboPopupToContents(QComboBox* combo) {
    if (!combo || !combo->view())
        return;
    QFontMetrics fm(combo->font());
    int maxText = 0;
    for (int i = 0; i < combo->count(); ++i)
        maxText = qMax(maxText, fm.horizontalAdvance(combo->itemText(i)));
    int popupW = maxText + 28; // 边距
    if (combo->count() > combo->maxVisibleItems())
        popupW += combo->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    popupW = qMax(popupW, combo->width());
    combo->view()->setMinimumWidth(popupW);
    if (auto* view = qobject_cast<QListView*>(combo->view()))
        view->setTextElideMode(Qt::ElideNone);
}

// 专用 MIME，避免与 qsetting 自由工站 setText(索引) 拖放冲突
const char kTestCaseFlowMime[] = "application/x-testcase-flow-block";

QVBoxLayout* flowLayoutOfBlock(TestCaseBlock* block) {
    if (!block || !block->parentWidget())
        return nullptr;
    return qobject_cast<QVBoxLayout*>(block->parentWidget()->layout());
}

/** 将 src 插入到 layout 的 insertIndex 位置（支持跨主流程/失败区拖拽）。 */
void moveBlockInFlowLayout(QVBoxLayout* layout, TestCaseBlock* src, int insertIndex) {
    if (!layout || !src)
        return;
    QVBoxLayout* srcLayout = flowLayoutOfBlock(src);
    if (srcLayout)
        srcLayout->removeWidget(src);
    QWidget* destParent = layout->parentWidget();
    if (destParent && src->parentWidget() != destParent)
        src->setParent(destParent);
    insertIndex = qBound(0, insertIndex, layout->count());
    layout->insertWidget(insertIndex, src);
}

bool acceptsTestCaseFlowMime(const QMimeData* mime) {
    return mime && mime->hasFormat(QLatin1String(kTestCaseFlowMime));
}

bool flowEntriesEqual(const QVector<TestFlowItemEntry>& a, const QVector<TestFlowItemEntry>& b) {
    if (a.size() != b.size())
        return false;
    for (int i = 0; i < a.size(); ++i) {
        if (a.at(i).caseName != b.at(i).caseName || a.at(i).enabled != b.at(i).enabled)
            return false;
    }
    return true;
}

} // namespace

// ---------- TestCaseBlock ----------

TestCaseBlock::TestCaseBlock(const QString& caseName, QWidget* parent)
    : QCheckBox(parent), caseName_(caseName) {
    setAcceptDrops(true);
    setCursor(Qt::PointingHandCursor);
    setCheckState(Qt::Checked);
    setStyleSheet(testFlowEditorStyleSheet());
    setCaseName(caseName);
    connect(this, &QCheckBox::stateChanged, this, [this](int) { updateBlockStyle(); });
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
    updateBlockStyle();
}

void TestCaseBlock::setSelected(bool selected) {
    if (selected_ == selected)
        return;
    selected_ = selected;
    updateBlockStyle();
}

void TestCaseBlock::setRunMenuVisible(bool visible) {
    runMenuVisible_ = visible;
}

void TestCaseBlock::updateBlockStyle() {
    // 状态走 QSS 动态属性 flowState，样式表见 test_flow_editor.qss
    QString state;
    if (isBlank())
        state = QStringLiteral("blank");
    else if (!isChecked())
        state = selected_ ? QStringLiteral("uncheckedSelected") : QStringLiteral("unchecked");
    else
        state = selected_ ? QStringLiteral("selected") : QStringLiteral("normal");
    if (property("flowState").toString() == state)
        return;
    setProperty("flowState", state);
    if (QStyle* s = style()) {
        s->unpolish(this);
        s->polish(this);
    }
    update();
}

void TestCaseBlock::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    // 样式表负责底色/勾选框；文字单独居中绘制（QCheckBox 默认无法 text-align:center）
    QStyleOptionButton opt;
    initStyleOption(&opt);
    const QString label = opt.text;
    opt.text.clear();

    QPainter painter(this);
    style()->drawControl(QStyle::CE_CheckBox, &opt, &painter, this);

    QColor textColor = isChecked() ? QColor(0x14, 0x53, 0x2d) : QColor(0x4b, 0x55, 0x63);
    if (isBlank())
        textColor = QColor(0x6b, 0x72, 0x80);
    QFont f = font();
    f.setPointSize(13);
    f.setBold(selected_);
    f.setItalic(isBlank());
    painter.setFont(f);
    painter.setPen(textColor);
    painter.drawText(rect(), Qt::AlignCenter, label);
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
    // 勿以功能块为父：块上 QCheckBox { color: 灰/绿 } 会污染菜单文字
    QMenu menu(window());
    applyFlowContextMenuStyle(&menu);
    menu.addAction(QStringLiteral("打开设置"), this, [this]() { emit editRequested(this); });
    if (runMenuVisible_) {
        QAction* runAct = menu.addAction(QStringLiteral("运行"), this, [this]() { emit runRequested(this); });
        runAct->setEnabled(!isBlank());
    }
    menu.addSeparator();
    menu.addAction(QStringLiteral("添加空白块"), this, [this]() { emit addBlankAfterRequested(this); });
    menu.addAction(QStringLiteral("添加已有块"), this, [this]() { emit addExistingAfterRequested(this); });
    menu.addSeparator();
    QAction* copyAct = menu.addAction(QStringLiteral("复制"), this, [this]() { emit copyRequested(this); });
    copyAct->setEnabled(!isBlank());
    menu.addAction(QStringLiteral("从流程移除"), this, [this]() { emit removeFromFlowRequested(this); });
    menu.exec(event->globalPos());
}

// ---------- TestFlowEditor ----------

TestFlowEditor::TestFlowEditor(QObject* parent) : QObject(parent) {
}

void TestFlowEditor::bindUi(QWidget* dialogParent, QComboBox* stationCombo, QScrollArea* scroll, QVBoxLayout* flowLayout,
                            QCheckBox* stopFlowOnTestFailCheck, QPushButton* btnDownloadCase,
                            QPushButton* btnUploadCase, QPushButton* btnSave, QPushButton* btnClear,
                            QScrollArea* failScroll, QVBoxLayout* failLayout, QPushButton* btnFailClear) {
    if (uiBound_)
        return;
    uiBound_ = true;

    dialogParent_ = dialogParent;
    stationCombo_ = stationCombo;
    scroll_ = scroll;
    flowLayout_ = flowLayout;
    failScroll_ = failScroll;
    failLayout_ = failLayout;
    stopFlowOnTestFailCheck_ = stopFlowOnTestFailCheck;

    if (!stationCombo_ || !scroll_ || !flowLayout_)
        return;

    flowContainer_ = scroll_->widget();
    if (flowContainer_) {
        flowContainer_->setAcceptDrops(true);
        flowContainer_->installEventFilter(this);
        flowContainer_->setStyleSheet(QStringLiteral("background: #f8fafc;"));
    }
    if (scroll_) {
        scroll_->setStyleSheet(QStringLiteral(
            "QScrollArea {"
            "  border: 1px solid #e2e8f0;"
            "  border-radius: 8px;"
            "  background: #f8fafc;"
            "}"
            "QScrollArea > QWidget > QWidget { background: #f8fafc; }"));
        if (scroll_->viewport())
            scroll_->viewport()->installEventFilter(this);
    }
    if (flowLayout_) {
        flowLayout_->setSpacing(6);
        flowLayout_->setContentsMargins(10, 10, 10, 10);
    }

    setupFailFlowRegionUi(btnFailClear);

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
    if (btnDownloadCase) {
        connect(btnDownloadCase, &QPushButton::clicked, this, &TestFlowEditor::startDownloadStationCase);
    }
    if (btnUploadCase) {
        connect(btnUploadCase, &QPushButton::clicked, this, &TestFlowEditor::startUploadCurrentStationCase);
    }
    if (dialogParent_) {
        if (auto* btnDownloadSteps =
                dialogParent_->findChild<QPushButton*>(QStringLiteral("pushButton_testFlowDownloadSteps"))) {
            connect(btnDownloadSteps, &QPushButton::clicked, this, &TestFlowEditor::startDownloadStepsLibrary);
        }
        if (auto* btnUploadSteps =
                dialogParent_->findChild<QPushButton*>(QStringLiteral("pushButton_testFlowUploadSteps"))) {
            connect(btnUploadSteps, &QPushButton::clicked, this, &TestFlowEditor::startUploadStepsLibrary);
        }
    }
    connect(btnSave, &QPushButton::clicked, this, [this]() { saveCurrentFlow(); });
    connect(btnClear, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(dialogParent_, QStringLiteral("确认"),
                                  QStringLiteral("清空当前工站主流程编排区？不会删除磁盘 ini")) == QMessageBox::Yes) {
            clearBlocks();
        }
    });

    if (stopFlowOnTestFailCheck_) {
        stopFlowOnTestFailCheck_->setToolTip(
            QStringLiteral("勾选后，本工站主流程任一步失败即停止后续主流程步骤，并按序执行下方「测试失败执行区域」；"
                           "失败区为空则直接结束整单"));
    }
    if (scroll_) {
        scroll_->setToolTip(QStringLiteral("主测试流程。拖拽功能块到目标位置：落在某块上半部=插到其前，下半部=插到其后；"
                                           "也可选中块后点「上移」「下移」，或右键添加功能块。可拖到下方失败区。调整完须点「保存流程」。"));
    }

    QString initialKey = TestCaseStore::resolveFlowStationKey(
        TestCaseStore::loadSelectedFlowStationKey());
    if (initialKey.isEmpty())
        initialKey = QStringLiteral("FREE_WORK");
    refreshStationCombo(initialKey);
    persistSelectedStation(initialKey);
    reloadCurrentStation();
    stationComboPrevIndex_ = stationCombo_ ? stationCombo_->currentIndex() : 0;
}

void TestFlowEditor::setupFailFlowRegionUi(QPushButton* btnFailClear) {
    if (!failScroll_ || !failLayout_)
        return;

    failContainer_ = failScroll_->widget();
    if (failContainer_) {
        failContainer_->setAcceptDrops(true);
        failContainer_->installEventFilter(this);
        failContainer_->setStyleSheet(QStringLiteral("background: #fff7ed;"));
    }
    if (failScroll_) {
        failScroll_->setStyleSheet(QStringLiteral(
            "QScrollArea {"
            "  border: 1px solid #fed7aa;"
            "  border-radius: 8px;"
            "  background: #fff7ed;"
            "}"));
        if (failScroll_->viewport())
            failScroll_->viewport()->installEventFilter(this);
    }
    failLayout_->setSpacing(6);
    failLayout_->setContentsMargins(10, 10, 10, 10);

    if (auto* pageLayout = qobject_cast<QVBoxLayout*>(scroll_->parentWidget() ? scroll_->parentWidget()->layout() : nullptr)) {
        const int mainIdx = pageLayout->indexOf(scroll_);
        const int failIdx = pageLayout->indexOf(failScroll_);
        if (mainIdx >= 0)
            pageLayout->setStretch(mainIdx, 3);
        if (failIdx >= 0)
            pageLayout->setStretch(failIdx, 1);
    }

    if (btnFailClear) {
        connect(btnFailClear, &QPushButton::clicked, this, [this]() {
            if (QMessageBox::question(dialogParent_, QStringLiteral("确认"),
                                      QStringLiteral("清空测试失败执行区域？")) == QMessageBox::Yes) {
                clearFailBlocks();
            }
        });
    }
}

void TestFlowEditor::showFlowRegionContextMenu(QVBoxLayout* layout, const QPoint& globalPos,
                                               TestCaseBlock* insertAfter) {
    if (!layout || !dialogParent_)
        return;
    const bool toFailRegion = layout == failLayout_;
    QMenu menu(dialogParent_);
    applyFlowContextMenuStyle(&menu);
    menu.addAction(QStringLiteral("添加空白块"), this, [this, layout, insertAfter]() {
        if (insertAfter)
            insertBlockAfter(insertAfter, QString());
        else if (layout == failLayout_)
            appendFailBlock(QString());
        else
            appendBlock(QString());
    });
    menu.addAction(QStringLiteral("添加已有块"), this, [this, toFailRegion, insertAfter]() {
        promptImportBlocks(toFailRegion, insertAfter);
    });
    menu.exec(globalPos);
}

void TestFlowEditor::promptImportBlocks(bool toFailRegion, TestCaseBlock* insertAfter) {
        QDialog pickDlg(dialogParent_);
        pickDlg.setWindowTitle(toFailRegion ? QStringLiteral("选择失败区功能块") : QStringLiteral("选择已有功能块"));
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
            applyFlowContextMenuStyle(&menu);
            menu.addAction(QStringLiteral("删除功能块"), [this, list, &allNames, searchEdit, targets]() {
                QStringList names;
                for (QListWidgetItem* item : targets) {
                    const QString name = item->text().trimmed();
                    if (!name.isEmpty())
                        names.append(name);
                }
                if (names.isEmpty())
                    return;

                const QString preview = names.count() > 5 ? names.mid(0, 5).join(QStringLiteral("、")) + QStringLiteral(" 等 %1 项").arg(names.count())
                                                          : names.join(QStringLiteral("、"));
                if (QMessageBox::question(dialogParent_, QStringLiteral("确认删除"),
                                          QStringLiteral("将永久删除 test_case 下以下功能块 ini：\r\n%1\r\n\r\n"
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

                auto removeDeletedFrom = [&](QVBoxLayout* layout) {
                    if (!layout)
                        return;
                    for (int i = layout->count() - 1; i >= 0; --i) {
                        auto* block = qobject_cast<TestCaseBlock*>(layout->itemAt(i)->widget());
                        if (!block || block->isBlank())
                            continue;
                        if (deleted.contains(block->caseName())) {
                            if (selectedBlock_ == block)
                                setSelectedBlock(nullptr);
                            layout->removeWidget(block);
                            block->deleteLater();
                        }
                    }
                };
                removeDeletedFrom(flowLayout_);
                removeDeletedFrom(failLayout_);
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
        // 双击某项：仅选中该项并确认添加
        connect(list, &QListWidget::itemDoubleClicked, &pickDlg, [&pickDlg, list](QListWidgetItem* item) {
            if (!item)
                return;
            list->clearSelection();
            item->setSelected(true);
            list->setCurrentItem(item);
            pickDlg.accept();
        });
        searchEdit->setFocus();
        list->clearSelection();
        if (pickDlg.exec() != QDialog::Accepted)
            return;
        TestCaseBlock* anchor = insertAfter;
        for (QListWidgetItem* item : list->selectedItems()) {
            const QString name = item->text();
            if (anchor) {
                insertBlockAfter(anchor, name);
                anchor = selectedBlock_;
            } else if (toFailRegion) {
                appendFailBlock(name);
            } else {
                appendBlock(name);
            }
        }
}

void TestFlowEditor::moveSelectedBlock(int delta) {
    QVBoxLayout* layout = layoutOfSelectedBlock();
    if (!layout || !selectedBlock_ || delta == 0)
        return;
    const int from = layout->indexOf(selectedBlock_);
    const int to = from + delta;
    if (from < 0 || to < 0 || to >= layout->count())
        return;
    layout->removeWidget(selectedBlock_);
    layout->insertWidget(to, selectedBlock_);
}

QVBoxLayout* TestFlowEditor::layoutOfSelectedBlock() const {
    return flowLayoutOfBlock(selectedBlock_);
}

bool TestFlowEditor::eventFilter(QObject* watched, QEvent* event) {
    QVBoxLayout* targetLayout = nullptr;
    QWidget* regionWidget = nullptr;
    if (watched == flowContainer_ || (scroll_ && watched == scroll_->viewport())) {
        targetLayout = flowLayout_;
        regionWidget = flowContainer_;
    } else if (watched == failContainer_ || (failScroll_ && watched == failScroll_->viewport())) {
        targetLayout = failLayout_;
        regionWidget = failContainer_;
    }
    if (targetLayout) {
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
                moveBlockInFlowLayout(targetLayout, src, targetLayout->count());
                e->acceptProposedAction();
            }
            return true;
        }
        case QEvent::ContextMenu: {
            auto* e = static_cast<QContextMenuEvent*>(event);
            if (e->reason() == QContextMenuEvent::Mouse && regionWidget) {
                const QPoint localPos = regionWidget->mapFromGlobal(e->globalPos());
                if (regionWidget->childAt(localPos)) {
                    // 点在功能块上时由 TestCaseBlock::contextMenuEvent 处理
                    break;
                }
            }
            showFlowRegionContextMenu(targetLayout, e->globalPos(), nullptr);
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
    if (keyToSelect.isEmpty())
        keyToSelect = TestCaseStore::loadSelectedFlowStationKey();
    if (keyToSelect.isEmpty())
        keyToSelect = QStringLiteral("FREE_WORK");

    // 按当前产品只展示对应工站（自由工站/默认工站始终保留）
    QString productName = SETTINGS.value(QStringLiteral("Mes/Product_Name")).toString().trimmed();
    if (productName.isEmpty())
        productName = SETTINGS.value(QStringLiteral("MES/Product_Name")).toString().trimmed();
    const QVector<TestFlowStationEntry> stations =
        TestCaseStore::loadFlowStationCatalogForProduct(productName);
    QSignalBlocker blocker(stationCombo_);
    stationCombo_->clear();
    for (const TestFlowStationEntry& entry : stations) {
        stationCombo_->addItem(entry.displayName, entry.key);
        // 闭合态截断时悬停仍可读全名
        stationCombo_->setItemData(stationCombo_->count() - 1, entry.displayName, Qt::ToolTipRole);
    }

    int idx = stationCombo_->findData(keyToSelect);
    if (idx < 0)
        idx = stationCombo_->findText(TestCaseStore::flowStationDisplayName(keyToSelect));
    // 当前选中工站不属于本产品时，落到本产品下第一项
    if (idx < 0 && !stations.isEmpty())
        idx = 0;
    if (idx >= 0)
        stationCombo_->setCurrentIndex(idx);
    adjustComboPopupToContents(stationCombo_);
}

void TestFlowEditor::onProductNameChanged() {
    if (!uiBound_ || !stationCombo_)
        return;
    refreshStationCombo(QString());
    persistSelectedStation(currentStationKey());
    reloadCurrentStation();
    stationComboPrevIndex_ = stationCombo_->currentIndex();
}

QMenu* TestFlowEditor::createFlowStationMenu(QWidget* parent, int hitComboIndex) {
    auto* menu = new QMenu(parent);
    applyFlowContextMenuStyle(menu);
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
    }, Qt::QueuedConnection);
    connect(actCopy, &QAction::triggered, this, [this, hitComboIndex]() {
        if (hitComboIndex >= 0 && !activateStationComboIndex(hitComboIndex))
            return;
        promptCopyCurrentFlowStation();
    }, Qt::QueuedConnection);
    connect(actDel, &QAction::triggered, this, [this, hitComboIndex]() {
        if (hitComboIndex >= 0 && !activateStationComboIndex(hitComboIndex))
            return;
        promptRemoveCurrentFlowStation();
    }, Qt::QueuedConnection);
    return menu;
}

void TestFlowEditor::setupStationComboContextMenu() {
    if (!stationCombo_)
        return;

    stationCombo_->setToolTip(
        QStringLiteral("选择要编辑的测试工站（随 Mes/Product_Name 产品型号过滤）；"
                       "数据来自 test_case/总的测试流程.ini"));

    if (auto* manageBtn =
            stationCombo_->parentWidget()->findChild<QToolButton*>(QStringLiteral("toolButton_testFlowStationMenu"))) {
        // 固定菜单 + aboutToShow 刷新可用态；勿在 currentIndexChanged 里反复 setMenu，否则 InstantPopup 下 QAction 可能不触发
        auto* menu = new QMenu(manageBtn);
        applyFlowContextMenuStyle(menu);
        QAction* const actNew = menu->addAction(QStringLiteral("新建工站"));
        QAction* const actRename = menu->addAction(QStringLiteral("重命名工站"));
        QAction* const actCopy = menu->addAction(QStringLiteral("复制工站"));
        QAction* const actDel = menu->addAction(QStringLiteral("删除工站"));
        connect(actNew, &QAction::triggered, this, [this]() { promptAddFlowStation(); }, Qt::QueuedConnection);
        connect(actRename, &QAction::triggered, this, [this]() { promptRenameCurrentFlowStation(); },
                Qt::QueuedConnection);
        connect(actCopy, &QAction::triggered, this, [this]() { promptCopyCurrentFlowStation(); }, Qt::QueuedConnection);
        connect(actDel, &QAction::triggered, this, [this]() { promptRemoveCurrentFlowStation(); }, Qt::QueuedConnection);
        connect(menu, &QMenu::aboutToShow, this, [this, actRename, actCopy, actDel]() {
            const bool hasStation = stationCombo_ && stationCombo_->count() > 0;
            actRename->setEnabled(hasStation);
            actCopy->setEnabled(hasStation);
            actDel->setEnabled(hasStation);
        });
        manageBtn->setMenu(menu);
        manageBtn->setPopupMode(QToolButton::InstantPopup);
    }

    stationCombo_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(stationCombo_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu* const menu = createFlowStationMenu(stationCombo_, stationCombo_->currentIndex());
        menu->exec(stationCombo_->mapToGlobal(pos));
        menu->deleteLater();
    });

    auto* stationView = new QListView(stationCombo_);
    stationView->setContextMenuPolicy(Qt::CustomContextMenu);
    stationView->setTextElideMode(Qt::ElideNone);
    stationCombo_->setView(stationView);
    adjustComboPopupToContents(stationCombo_);
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
    const QVector<TestFlowItemEntry> failEntries = currentFailFlowEntries();
    const bool stopFlow =
        stopFlowOnTestFailCheck_ ? stopFlowOnTestFailCheck_->isChecked() : true;

    QString newKey;
    QString err;
    if (!TestCaseStore::copyFlowStation(sourceKey, newName, entries, stopFlow, &newKey, &err)) {
        QMessageBox::warning(dialogParent_, QStringLiteral("无法复制"), err);
        return;
    }
    // 复制目录后覆盖主流程；显式再写失败区，避免仅拷贝未含未保存编辑
    TestCaseStore::saveStationFlowItems(newKey, entries, failEntries, stopFlow);

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
                              QStringLiteral("删除工站「%1」及其在 总的测试流程.ini 中的流程配置？\r\n"
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

void TestFlowEditor::clearBlocksInLayout(QVBoxLayout* layout) {
    if (!layout)
        return;
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (item->widget()) {
            if (selectedBlock_ == item->widget())
                selectedBlock_ = nullptr;
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void TestFlowEditor::clearBlocks() {
    clearBlocksInLayout(flowLayout_);
}

void TestFlowEditor::clearFailBlocks() {
    clearBlocksInLayout(failLayout_);
}

TestCaseBlock* TestFlowEditor::createBlock(QVBoxLayout* layout, QWidget* container, const QString& caseName,
                                           bool enabled) {
    if (!layout || !container)
        return nullptr;
    auto* block = new TestCaseBlock(caseName, container);
    block->setChecked(enabled);
    block->setRunMenuVisible(singleStepRunEnabled_);
    connect(block, &TestCaseBlock::editRequested, this, &TestFlowEditor::openEditDialog);
    connect(block, &TestCaseBlock::addBlankAfterRequested, this, [this](TestCaseBlock* b) {
        insertBlockAfter(b, QString());
    });
    connect(block, &TestCaseBlock::addExistingAfterRequested, this, [this](TestCaseBlock* b) {
        promptImportBlocks(flowLayoutOfBlock(b) == failLayout_, b);
    });
    connect(block, &TestCaseBlock::copyRequested, this, &TestFlowEditor::copyBlock);
    connect(block, &TestCaseBlock::runRequested, this, &TestFlowEditor::runBlock);
    connect(block, &TestCaseBlock::removeFromFlowRequested, this, [this](TestCaseBlock* b) {
        if (selectedBlock_ == b)
            setSelectedBlock(nullptr);
        if (QVBoxLayout* lay = flowLayoutOfBlock(b))
            lay->removeWidget(b);
        b->deleteLater();
    });
    connect(block, &TestCaseBlock::blockSelected, this, [this](TestCaseBlock* b) { setSelectedBlock(b); });
    return block;
}

void TestFlowEditor::appendBlock(const QString& caseName, bool enabled) {
    TestCaseBlock* block = createBlock(flowLayout_, flowContainer_, caseName, enabled);
    if (!block || !flowLayout_)
        return;
    flowLayout_->addWidget(block);
    setSelectedBlock(block);
}

void TestFlowEditor::appendFailBlock(const QString& caseName, bool enabled) {
    TestCaseBlock* block = createBlock(failLayout_, failContainer_, caseName, enabled);
    if (!block || !failLayout_)
        return;
    failLayout_->addWidget(block);
    setSelectedBlock(block);
}

void TestFlowEditor::insertBlockAfter(TestCaseBlock* after, const QString& caseName, bool enabled) {
    QVBoxLayout* layout = after ? flowLayoutOfBlock(after) : flowLayout_;
    QWidget* container = layout == failLayout_ ? failContainer_ : flowContainer_;
    TestCaseBlock* block = createBlock(layout, container, caseName, enabled);
    if (!block || !layout)
        return;
    const int idx = after ? layout->indexOf(after) : -1;
    if (idx < 0)
        layout->addWidget(block);
    else
        layout->insertWidget(idx + 1, block);
    setSelectedBlock(block);
}

void TestFlowEditor::copyBlock(TestCaseBlock* src) {
    if (!src || !dialogParent_)
        return;
    if (src->isBlank()) {
        QMessageBox::warning(dialogParent_, QStringLiteral("无法复制"),
                             QStringLiteral("空白块无法复制，请先打开设置并保存步骤。"));
        return;
    }
    const QString stationKey = currentStationKey();
    if (stationKey.isEmpty()) {
        QMessageBox::warning(dialogParent_, QStringLiteral("无法复制"), QStringLiteral("请先选择工站"));
        return;
    }

    TestCaseDefinition def;
    if (!TestCaseStore::loadCaseForStation(stationKey, src->caseName(), def)) {
        QMessageBox::warning(dialogParent_, QStringLiteral("无法复制"),
                             QStringLiteral("无法读取步骤「%1」配置").arg(src->caseName()));
        return;
    }

    const QString baseName = def.meta.name.trimmed().isEmpty() ? src->caseName() : def.meta.name.trimmed();
    QSet<QString> usedNames;
    for (const TestFlowItemEntry& entry : currentFlowEntries())
        usedNames.insert(entry.caseName.trimmed());
    for (const TestFlowItemEntry& entry : currentFailFlowEntries())
        usedNames.insert(entry.caseName.trimmed());
    for (const QString& name : TestCaseStore::listCaseIniNames())
        usedNames.insert(name.trimmed());

    QString newName = baseName + QStringLiteral(" 副本");
    for (int n = 2; usedNames.contains(newName) || TestCasePaths::stepIniExistsForStation(stationKey, newName);
         ++n) {
        if (n > 999) {
            QMessageBox::warning(dialogParent_, QStringLiteral("无法复制"),
                                 QStringLiteral("无法生成可用的副本名称"));
            return;
        }
        newName = baseName + QStringLiteral(" 副本%1").arg(n);
    }

    QSet<QString> usedMesTags;
    for (const TestFlowItemEntry& entry : currentFlowEntries()) {
        TestCaseDefinition other;
        if (!TestCaseStore::loadCaseForStation(stationKey, entry.caseName, other))
            continue;
        const QString tag = other.meta.mesTag.trimmed();
        if (!tag.isEmpty())
            usedMesTags.insert(tag);
    }
    QString baseTag = def.meta.mesTag.trimmed();
    if (baseTag.isEmpty())
        baseTag = QStringLiteral("STEP");
    QString newTag = baseTag + QStringLiteral("_COPY");
    for (int n = 2; usedMesTags.contains(newTag); ++n) {
        if (n > 999) {
            QMessageBox::warning(dialogParent_, QStringLiteral("无法复制"),
                                 QStringLiteral("无法生成可用的上报MES字段"));
            return;
        }
        newTag = baseTag + QStringLiteral("_COPY%1").arg(n);
    }

    def.meta.name = newName;
    def.meta.displayName = newName;
    def.meta.mesTag = newTag;

    QStringList errors;
    if (!TestCaseValidator::validateCase(def, errors)) {
        QMessageBox::warning(dialogParent_, QStringLiteral("无法复制"), errors.join(QStringLiteral("\r\n")));
        return;
    }
    if (!TestCaseStore::saveCaseForStation(stationKey, def)) {
        QMessageBox::warning(dialogParent_, QStringLiteral("无法复制"), QStringLiteral("写入步骤配置失败"));
        return;
    }

    insertBlockAfter(src, newName, src->isChecked());
}

void TestFlowEditor::setSingleStepRunEnabled(bool enabled) {
    singleStepRunEnabled_ = enabled;
    auto apply = [enabled](QVBoxLayout* layout) {
        if (!layout)
            return;
        for (int i = 0; i < layout->count(); ++i) {
            if (auto* block = qobject_cast<TestCaseBlock*>(layout->itemAt(i)->widget()))
                block->setRunMenuVisible(enabled);
        }
    };
    apply(flowLayout_);
    apply(failLayout_);
}

void TestFlowEditor::runBlock(TestCaseBlock* src) {
    if (!src || !dialogParent_ || !singleStepRunEnabled_)
        return;
    if (src->isBlank()) {
        QMessageBox::warning(dialogParent_, QStringLiteral("无法运行"),
                             QStringLiteral("空白块无法运行，请先打开设置并保存步骤。"));
        return;
    }
    const QString stationKey = currentStationKey();
    if (stationKey.isEmpty()) {
        QMessageBox::warning(dialogParent_, QStringLiteral("无法运行"), QStringLiteral("请先选择工站"));
        return;
    }
    if (!TestCasePaths::stepIniExistsForStation(stationKey, src->caseName())) {
        QMessageBox::warning(dialogParent_, QStringLiteral("无法运行"),
                             QStringLiteral("步骤「%1」配置不存在，请先保存。").arg(src->caseName()));
        return;
    }
    emit runStepRequested(stationKey, src->caseName());
}

QString TestFlowEditor::currentStationKey() const {
    return stationCombo_ ? stationCombo_->currentData().toString().trimmed() : QString();
}

QVector<TestFlowItemEntry> TestFlowEditor::entriesFromLayout(QVBoxLayout* layout) const {
    QVector<TestFlowItemEntry> entries;
    if (!layout)
        return entries;
    for (int i = 0; i < layout->count(); ++i) {
        auto* w = layout->itemAt(i)->widget();
        auto* block = qobject_cast<TestCaseBlock*>(w);
        if (!block || block->isBlank())
            continue;
        TestFlowItemEntry entry;
        entry.caseName = block->caseName();
        entry.enabled = block->isChecked();
        entries.append(entry);
    }
    return entries;
}

QVector<TestFlowItemEntry> TestFlowEditor::currentFlowEntries() const {
    return entriesFromLayout(flowLayout_);
}

QVector<TestFlowItemEntry> TestFlowEditor::currentFailFlowEntries() const {
    return entriesFromLayout(failLayout_);
}

void TestFlowEditor::persistSelectedStation(const QString& key) {
    const QString resolved = TestCaseStore::resolveFlowStationKey(key.trimmed());
    if (resolved.isEmpty())
        return;

    const QString displayName = TestCaseStore::flowStationDisplayName(resolved);
    TestCaseStore::saveSelectedFlowStation(resolved, displayName);
    // 已打开的自由工站立即刷新 Tab / 串口显隐，避免要等点「开始测试」才生效
    box_base::refreshFlowUiOnAllFreeWork();
}

bool TestFlowEditor::saveStationFlow(const QString& stationKey) {
    const QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    if (key.isEmpty()) {
        QMessageBox::warning(dialogParent_, QStringLiteral("保存失败"), QStringLiteral("工站无效"));
        return false;
    }
    const QVector<TestFlowItemEntry> entries = currentFlowEntries();
    const QVector<TestFlowItemEntry> failEntries = currentFailFlowEntries();
    for (const TestFlowItemEntry& entry : entries) {
        if (!TestCasePaths::stepIniExistsForStation(key, entry.caseName)) {
            QMessageBox::warning(dialogParent_, QStringLiteral("保存失败"),
                                 QStringLiteral("case 不存在: %1，请先保存配置").arg(entry.caseName));
            return false;
        }
    }
    for (const TestFlowItemEntry& entry : failEntries) {
        if (!TestCasePaths::stepIniExistsForStation(key, entry.caseName)) {
            QMessageBox::warning(dialogParent_, QStringLiteral("保存失败"),
                                 QStringLiteral("失败区 case 不存在: %1，请先保存配置").arg(entry.caseName));
            return false;
        }
    }
    QStringList mesErrors;
    if (!TestCaseValidator::validateFlowMesTags(key, entries, mesErrors)) {
        QMessageBox::warning(dialogParent_, QStringLiteral("保存失败"), mesErrors.join(QStringLiteral("\r\n")));
        return false;
    }
    const bool stopFlowOnTestFail =
        stopFlowOnTestFailCheck_ ? stopFlowOnTestFailCheck_->isChecked() : true;
    TestCaseStore::saveStationFlowItems(key, entries, failEntries, stopFlowOnTestFail);
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

void TestFlowEditor::startUploadCurrentStationCase() {
    const QString stationKey = currentStationKey();
    const QString stationName = TestCaseStore::flowStationDisplayName(stationKey);
    const QString label = stationName.isEmpty() ? stationKey : stationName;
    if (label.isEmpty()) {
        QMessageBox::warning(dialogParent_, QStringLiteral("上传本工站用例"),
                             QStringLiteral("请先选择当前测试工站。"));
        return;
    }

    if (hasUnsavedChanges()) {
        const auto saveAnswer = QMessageBox::question(
            dialogParent_, QStringLiteral("上传本工站用例"),
            QStringLiteral("当前编排有未保存改动，是否先保存再上传？"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
        if (saveAnswer == QMessageBox::Cancel) {
            return;
        }
        if (saveAnswer == QMessageBox::Yes && !saveStationFlow(stationKey)) {
            return;
        }
    }

    bool remarkOk = false;
    const QString remark = QInputDialog::getMultiLineText(
                               dialogParent_, QStringLiteral("上传本工站用例"),
                               QStringLiteral("请填写上传说明（必填，将显示在网页草稿列表）：\n工站「%1」")
                                   .arg(label),
                               QString(), &remarkOk)
                               .trimmed();
    if (!remarkOk) {
        return;
    }
    if (remark.isEmpty()) {
        QMessageBox::warning(dialogParent_, QStringLiteral("上传本工站用例"),
                             QStringLiteral("上传说明不能为空。"));
        return;
    }
    if (remark.size() > 500) {
        QMessageBox::warning(dialogParent_, QStringLiteral("上传本工站用例"),
                             QStringLiteral("上传说明最多 500 字。"));
        return;
    }

    const auto answer = QMessageBox::question(
        dialogParent_, QStringLiteral("上传本工站用例"),
        QStringLiteral("上传当前工站「%1」到云端草稿（不影响其他工站）。\n"
                       "说明：%2\n\n网页「合入 + 发布」后，其他电脑才能下载到。\n\n确认上传？")
            .arg(label, remark));
    if (answer != QMessageBox::Yes) {
        return;
    }

    persistSelectedStation(stationKey);
    auto* watcher = new QFutureWatcher<TestCaseSyncService::SyncResult>(this);
    connect(watcher, &QFutureWatcher<TestCaseSyncService::SyncResult>::finished, this, [this, watcher]() {
        const TestCaseSyncService::SyncResult result = watcher->result();
        if (result.ok) {
            QMessageBox::information(dialogParent_, QStringLiteral("上传本工站用例"), result.message);
        } else {
            QMessageBox::warning(dialogParent_, QStringLiteral("上传本工站用例"), result.message);
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([remark]() { return TestCaseSyncService::uploadToCloud(remark); }));
}

void TestFlowEditor::startUploadStepsLibrary() {
    bool remarkOk = false;
    const QString remark =
        QInputDialog::getMultiLineText(dialogParent_, QStringLiteral("上传用例库"),
                                       QStringLiteral("请填写上传说明（必填，将显示在网页草稿列表）：\n"
                                                      "将上传本机 test_case/steps 共享用例库。"),
                                       QString(), &remarkOk)
            .trimmed();
    if (!remarkOk) {
        return;
    }
    if (remark.isEmpty()) {
        QMessageBox::warning(dialogParent_, QStringLiteral("上传用例库"), QStringLiteral("上传说明不能为空。"));
        return;
    }
    if (remark.size() > 500) {
        QMessageBox::warning(dialogParent_, QStringLiteral("上传用例库"), QStringLiteral("上传说明最多 500 字。"));
        return;
    }

    const auto answer = QMessageBox::question(
        dialogParent_, QStringLiteral("上传用例库"),
        QStringLiteral("上传本机共享用例库（test_case/steps）到云端草稿。\n"
                       "说明：%1\n\n不会上传各工站 profiles；网页「合入 + 发布」后，"
                       "其他电脑才能「下载用例库」。\n\n确认上传？")
            .arg(remark));
    if (answer != QMessageBox::Yes) {
        return;
    }

    auto* watcher = new QFutureWatcher<TestCaseSyncService::SyncResult>(this);
    connect(watcher, &QFutureWatcher<TestCaseSyncService::SyncResult>::finished, this, [this, watcher]() {
        const TestCaseSyncService::SyncResult result = watcher->result();
        watcher->deleteLater();
        if (result.ok) {
            QMessageBox::information(dialogParent_, QStringLiteral("上传用例库"), result.message);
        } else {
            QMessageBox::warning(dialogParent_, QStringLiteral("上传用例库"), result.message);
        }
    });
    watcher->setFuture(QtConcurrent::run([remark]() { return TestCaseSyncService::uploadStepsLibrary(remark); }));
}

void TestFlowEditor::startDownloadStepsLibrary() {
    const auto answer = QMessageBox::question(
        dialogParent_, QStringLiteral("下载用例库"),
        QStringLiteral("从云端下载已发布的共享用例库，仅覆盖本机 test_case/steps。\n"
                       "不会动各工站 profiles；未发布的草稿也拉不到。\n\n确认下载？"));
    if (answer != QMessageBox::Yes) {
        return;
    }

    auto* watcher = new QFutureWatcher<TestCaseSyncService::SyncResult>(this);
    connect(watcher, &QFutureWatcher<TestCaseSyncService::SyncResult>::finished, this, [this, watcher]() {
        const TestCaseSyncService::SyncResult result = watcher->result();
        watcher->deleteLater();
        if (!result.ok) {
            QMessageBox::warning(dialogParent_, QStringLiteral("下载用例库"), result.message);
            return;
        }
        QMessageBox::information(dialogParent_, QStringLiteral("下载用例库"), result.message);
        // 用例库变更可能影响「添加已有块」列表与步骤展示名
        TestCaseStore::invalidateCloudItemNameCache();
        reloadCurrentStation();
    });
    watcher->setFuture(QtConcurrent::run([]() { return TestCaseSyncService::syncStepsLibraryFromCloud(); }));
}

void TestFlowEditor::startDownloadStationCase() {
    auto* listWatcher = new QFutureWatcher<TestCaseSyncService::ProfileListResult>(this);
    connect(listWatcher, &QFutureWatcher<TestCaseSyncService::ProfileListResult>::finished, this,
            [this, listWatcher]() {
                const TestCaseSyncService::ProfileListResult listResult = listWatcher->result();
                listWatcher->deleteLater();
                if (!listResult.ok) {
                    QMessageBox::warning(dialogParent_, QStringLiteral("下载工站用例"), listResult.message);
                    return;
                }
                if (listResult.items.isEmpty()) {
                    QMessageBox::information(dialogParent_, QStringLiteral("下载工站用例"),
                                             listResult.message.isEmpty()
                                                 ? QStringLiteral("云端尚无已发布的工站用例")
                                                 : listResult.message);
                    return;
                }

                const QString currentKey = currentStationKey().trimmed();
                QStringList labels;
                int currentIndex = 0;
                for (int i = 0; i < listResult.items.size(); ++i) {
                    const TestCaseSyncService::PublishedProfile& item = listResult.items.at(i);
                    QString label = item.displayName;
                    if (label.isEmpty()) {
                        label = item.stationKey;
                    }
                    if (!item.profileVersion.isEmpty()) {
                        label += QStringLiteral("（v%1）").arg(item.profileVersion);
                    }
                    if (!currentKey.isEmpty() &&
                        item.stationKey.compare(currentKey, Qt::CaseInsensitive) == 0) {
                        label += QStringLiteral(" · 当前工站");
                        currentIndex = i;
                    }
                    labels.append(label);
                }

                bool ok = false;
                const QString selected = QInputDialog::getItem(
                    dialogParent_, QStringLiteral("下载工站用例"),
                    QStringLiteral("请选择要下载的工站（可下载本工站或其他工站）："), labels,
                    currentIndex, false, &ok);
                if (!ok || selected.isEmpty()) {
                    return;
                }

                const int idx = labels.indexOf(selected);
                if (idx < 0 || idx >= listResult.items.size()) {
                    return;
                }

                const TestCaseSyncService::PublishedProfile chosen = listResult.items.at(idx);
                const QString label =
                    chosen.displayName.isEmpty() ? chosen.stationKey : chosen.displayName;
                const bool downloadingCurrent =
                    !currentKey.isEmpty() &&
                    chosen.stationKey.compare(currentKey, Qt::CaseInsensitive) == 0;
                if (downloadingCurrent && hasUnsavedChanges() && !confirmDiscardOrSaveOnLeave()) {
                    return;
                }

                const auto answer = QMessageBox::question(
                    dialogParent_, QStringLiteral("下载工站用例"),
                    QStringLiteral("从云端下载已发布的正式用例，仅覆盖本机工站「%1」。\n"
                                   "不会动其他工站；未发布的草稿也拉不到。\n\n确认下载？")
                        .arg(label));
                if (answer != QMessageBox::Yes) {
                    return;
                }

                const QString stationKey = chosen.stationKey;
                const QString displayName = chosen.displayName;
                auto* syncWatcher = new QFutureWatcher<TestCaseSyncService::SyncResult>(this);
                connect(syncWatcher, &QFutureWatcher<TestCaseSyncService::SyncResult>::finished, this,
                        [this, syncWatcher]() {
                            const TestCaseSyncService::SyncResult result = syncWatcher->result();
                            syncWatcher->deleteLater();
                            if (!result.ok) {
                                QMessageBox::warning(dialogParent_, QStringLiteral("下载工站用例"),
                                                     result.message);
                                return;
                            }
                            QMessageBox::information(dialogParent_, QStringLiteral("下载工站用例"),
                                                     result.message);
                            // 下载可能新增工站目录或覆盖 flow/steps；须重扫目录并刷新编排区与自由工站 Tab
                            TestCaseStore::reregisterFlowStationsFromProfiles();
                            const QString downloadedKey =
                                TestCaseStore::resolveFlowStationKey(result.stationKey.trimmed());
                            const QString selectKey =
                                downloadedKey.isEmpty() ? currentStationKey() : downloadedKey;
                            refreshStationCombo(selectKey);
                            stationComboPrevIndex_ = stationCombo_ ? stationCombo_->currentIndex() : 0;
                            persistSelectedStation(currentStationKey());
                            reloadCurrentStation();
                        });
                syncWatcher->setFuture(QtConcurrent::run([stationKey, displayName]() {
                    return TestCaseSyncService::syncStationFromCloud(stationKey, displayName);
                }));
            });
    listWatcher->setFuture(QtConcurrent::run([]() { return TestCaseSyncService::listPublishedProfiles(); }));
}

void TestFlowEditor::updateSavedSnapshot() {
    savedEntriesSnapshot_ = currentFlowEntries();
    savedFailEntriesSnapshot_ = currentFailFlowEntries();
    savedStopFlowOnTestFail_ =
        stopFlowOnTestFailCheck_ ? stopFlowOnTestFailCheck_->isChecked() : true;
}

bool TestFlowEditor::hasUnsavedChanges() const {
    if (!uiBound_ || !flowLayout_)
        return false;
    if (!flowEntriesEqual(currentFlowEntries(), savedEntriesSnapshot_))
        return true;
    if (!flowEntriesEqual(currentFailFlowEntries(), savedFailEntriesSnapshot_))
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
    auto* btnCancel = box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(btnSave);
    box.exec();

    if (box.clickedButton() == btnSave) {
        const QString key = lastLoadedStationKey_.isEmpty() ? currentStationKey() : lastLoadedStationKey_;
        if (!saveStationFlow(key))
            return false;
        return true;
    }

    // 「取消」= 不保存：丢弃界面改动，并允许继续关闭/切页/切工站
    reloadCurrentStation();
    return true;
}

void TestFlowEditor::reloadCurrentStation() {
    const QString key = currentStationKey();
    lastLoadedStationKey_ = key;
    clearBlocks();
    clearFailBlocks();
    if (stopFlowOnTestFailCheck_) {
        QSignalBlocker blocker(stopFlowOnTestFailCheck_);
        stopFlowOnTestFailCheck_->setChecked(TestCaseStore::loadStationStopFlowOnTestFail(key, true));
    }
    const QVector<TestFlowItemEntry> entries = TestCaseStore::loadStationFlowItems(key);
    for (const TestFlowItemEntry& entry : entries)
        appendBlock(entry.caseName, entry.enabled);
    const QVector<TestFlowItemEntry> failEntries = TestCaseStore::loadStationFailFlowItems(key);
    for (const TestFlowItemEntry& entry : failEntries)
        appendFailBlock(entry.caseName, entry.enabled);
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
    const QString stationKey = currentStationKey();
    auto* dlg = new TestCaseEditDialog(dialogParent_);
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->setWindowModality(Qt::NonModal);
    // 独立窗口并带最大化按钮（纯 Dialog 默认往往没有）
    dlg->setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint
                        | Qt::WindowCloseButtonHint);
    dlg->setStationContext(stationKey);
    dlg->setFlowContext(currentFlowEntries());

    if (!block->isBlank()) {
        TestCaseDefinition def;
        TestCaseStore::loadCaseForStation(stationKey, block->caseName(), def);
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
            TestCaseStore::saveStationFlowItems(currentStationKey(), currentFlowEntries(),
                                                currentFailFlowEntries(),
                                                stopFlowOnTestFailCheck_ ? stopFlowOnTestFailCheck_->isChecked() : true);
    });

    dlg->show();
}
