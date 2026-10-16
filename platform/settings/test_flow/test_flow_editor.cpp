#include "test_flow_editor.h"

#include "test_case.h"
#include "test_case_edit_dialog.h"
#include "test_case_sync_service.h"
#include "test_case_policy_service.h"
#include "box_base.h"

#include "Abini.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QGroupBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFontMetrics>
#include <QFrame>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QScreen>
#include <QSet>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
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

/** 确认框不挂带壳层 QSS 的父窗，避免继承后图标/文案/按钮栏比例错乱 */
static QMessageBox::StandardButton askQuestion(QWidget* anchor, const QString& title, const QString& text,
                                               QMessageBox::StandardButtons buttons = QMessageBox::Yes | QMessageBox::No,
                                               QMessageBox::StandardButton defaultButton = QMessageBox::Yes)
{
    QMessageBox box(QMessageBox::Question, title, text, buttons, nullptr);
    box.setWindowModality(Qt::ApplicationModal);
    if (QAbstractButton* yesBtn = box.button(QMessageBox::Yes))
        yesBtn->setText(QStringLiteral("是"));
    if (QAbstractButton* noBtn = box.button(QMessageBox::No))
        noBtn->setText(QStringLiteral("否"));
    if (QAbstractButton* cancelBtn = box.button(QMessageBox::Cancel))
        cancelBtn->setText(QStringLiteral("取消"));
    box.setDefaultButton(defaultButton);
    box.adjustSize();
    if (anchor) {
        const QRect ag = anchor->window() ? anchor->window()->frameGeometry() : anchor->frameGeometry();
        box.move(ag.center() - QPoint(box.width() / 2, box.height() / 2));
    }
    return static_cast<QMessageBox::StandardButton>(box.exec());
}

static bool askYesNo(QWidget* anchor, const QString& title, const QString& text,
                     QMessageBox::StandardButton defaultButton = QMessageBox::Yes)
{
    return askQuestion(anchor, title, text, QMessageBox::Yes | QMessageBox::No, defaultButton) == QMessageBox::Yes;
}

/** 新建/复制工站：优先向云端申请 StationKey；失败时可选择本机发号或取消。返回 false 表示用户取消。 */
bool promptFlowStationKeyFromCloud(QWidget* parent, const QString& displayName, QString* preferredKeyOut,
                                   bool* usedCloudOut = nullptr) {
    if (preferredKeyOut)
        preferredKeyOut->clear();
    if (usedCloudOut)
        *usedCloudOut = false;
    const TestCaseSyncService::SyncResult alloc = TestCaseSyncService::allocateStationKeyFromCloud(displayName);
    if (alloc.ok && !alloc.stationKey.isEmpty()) {
        if (preferredKeyOut)
            *preferredKeyOut = alloc.stationKey;
        if (usedCloudOut)
            *usedCloudOut = true;
        return true;
    }
    const QString hint =
        alloc.message.isEmpty() ? QStringLiteral("无法从云端获取工站编号") : alloc.message;
    if (!parent)
        return false;
    return askYesNo(parent, QStringLiteral("云端发号失败"),
                    hint + QStringLiteral("\n\n是否改用本机自动发号继续？"), QMessageBox::No);
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

QComboBox* makeSerialBaudCombo(QWidget* parent, int baudRate) {
    auto* combo = new QComboBox(parent);
    combo->addItems({QStringLiteral("9600"), QStringLiteral("19200"), QStringLiteral("115200"),
                     QStringLiteral("38400"), QStringLiteral("57600"), QStringLiteral("460800"),
                     QStringLiteral("921600"), QStringLiteral("1000000")});
    const int baud = baudRate > 0 ? baudRate : 115200;
    const int index = combo->findText(QString::number(baud));
    combo->setCurrentIndex(index >= 0 ? index : combo->findText(QStringLiteral("115200")));
    return combo;
}

int serialBaudFromCombo(QComboBox* combo, int fallback) {
    if (!combo)
        return fallback;
    bool ok = false;
    const int baud = combo->currentText().toInt(&ok);
    return ok && baud > 0 ? baud : fallback;
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
    // 拖拽落点后立刻重排序号；源区若不同也要刷新
    auto renumber = [](QVBoxLayout* lay) {
        if (!lay)
            return;
        int index = 0;
        for (int i = 0; i < lay->count(); ++i) {
            if (auto* block = qobject_cast<TestCaseBlock*>(lay->itemAt(i)->widget()))
                block->setFlowIndex(++index);
        }
    };
    renumber(layout);
    if (srcLayout && srcLayout != layout)
        renumber(srcLayout);
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

/** 流程块行内小图标（避免 emoji 撑高行高） */
QIcon flowLineIcon(const QString& kind) {
    const int s = 16;
    QPixmap pm(s, s);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor stroke(0x4b, 0x55, 0x63);
    QPen pen(stroke, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    if (kind == QLatin1String("copy")) {
        p.drawRoundedRect(3, 5, 8, 8, 1.5, 1.5);
        p.drawRoundedRect(6, 3, 8, 8, 1.5, 1.5);
    } else if (kind == QLatin1String("edit")) {
        p.drawEllipse(QPointF(8, 8), 5.2, 5.2);
        static const qreal kCos[] = {1, 0.5, -0.5, -1, -0.5, 0.5};
        static const qreal kSin[] = {0, 0.8660254, 0.8660254, 0, -0.8660254, -0.8660254};
        for (int i = 0; i < 6; ++i) {
            p.drawLine(QPointF(8 + 5.2 * kCos[i], 8 + 5.2 * kSin[i]),
                       QPointF(8 + 7.2 * kCos[i], 8 + 7.2 * kSin[i]));
        }
        p.setBrush(stroke);
        p.drawEllipse(QPointF(8, 8), 1.6, 1.6);
    } else if (kind == QLatin1String("remove")) {
        p.drawLine(5, 4, 11, 4);
        p.drawLine(6.5, 4, 7, 3);
        p.drawLine(9.5, 4, 9, 3);
        p.drawRect(5, 5, 6, 8);
        p.drawLine(7, 7, 7, 11);
        p.drawLine(9, 7, 9, 11);
    }
    return QIcon(pm);
}

QIcon flowEnableIcon(bool checked) {
    const int s = 16;
    QPixmap pm(s, s);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (checked) {
        p.setPen(QPen(QColor(0x15, 0x80, 0x3d), 1));
        p.setBrush(QColor(0x22, 0xc5, 0x5e));
        p.drawRoundedRect(0.5, 0.5, 15, 15, 3.5, 3.5);
        p.setPen(QPen(Qt::white, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawLine(4, 8, 7, 11);
        p.drawLine(7, 11, 12, 5);
    } else {
        p.setPen(QPen(QColor(0x9c, 0xa3, 0xaf), 1));
        p.setBrush(Qt::white);
        p.drawRoundedRect(0.5, 0.5, 15, 15, 3.5, 3.5);
    }
    return QIcon(pm);
}

} // namespace

// ---------- TestCaseBlock ----------

QToolButton* TestCaseBlock::makeActionButton(const QString& objectName, const QString& tip, const QIcon& icon) {
    auto* btn = new QToolButton(this);
    btn->setObjectName(objectName);
    btn->setToolTip(tip);
    btn->setIcon(icon);
    btn->setIconSize(QSize(14, 14));
    btn->setFixedSize(26, 26);
    btn->setCursor(Qt::ArrowCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setAutoRaise(false);
    btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    return btn;
}

TestCaseBlock::TestCaseBlock(const QString& caseName, QWidget* parent)
    : QFrame(parent), caseName_(caseName) {
    setAcceptDrops(true);
    setCursor(Qt::PointingHandCursor);
    setObjectName(QStringLiteral("TestCaseBlock"));
    // 固定紧凑行高，避免 ScrollArea / emoji 把块撑得过高
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(36);
    setStyleSheet(testFlowEditorStyleSheet());

    enableCheck_ = new QToolButton(this);
    enableCheck_->setObjectName(QStringLiteral("flowBlockEnable"));
    enableCheck_->setCheckable(true);
    enableCheck_->setChecked(true);
    enableCheck_->setFixedSize(18, 18);
    enableCheck_->setIconSize(QSize(16, 16));
    enableCheck_->setIcon(flowEnableIcon(true));
    enableCheck_->setCursor(Qt::ArrowCursor);
    enableCheck_->setFocusPolicy(Qt::NoFocus);
    enableCheck_->setAutoRaise(true);
    enableCheck_->setToolButtonStyle(Qt::ToolButtonIconOnly);

    indexLabel_ = new QLabel(QStringLiteral("01"), this);
    indexLabel_->setObjectName(QStringLiteral("flowBlockIndex"));
    indexLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    indexLabel_->setAutoFillBackground(false);

    titleLabel_ = new QLabel(this);
    titleLabel_->setObjectName(QStringLiteral("flowBlockTitle"));
    titleLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    titleLabel_->setAutoFillBackground(false);
    titleLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    btnCopy_ =
        makeActionButton(QStringLiteral("flowBlockBtnCopy"), QStringLiteral("复制"), flowLineIcon(QStringLiteral("copy")));
    auto* btnEdit =
        makeActionButton(QStringLiteral("flowBlockBtnEdit"), QStringLiteral("打开设置"), flowLineIcon(QStringLiteral("edit")));
    auto* btnRemove = makeActionButton(QStringLiteral("flowBlockBtnRemove"), QStringLiteral("从流程移除"),
                                       flowLineIcon(QStringLiteral("remove")));

    auto* actions = new QWidget(this);
    actions->setObjectName(QStringLiteral("flowBlockActions"));
    auto* actionsLay = new QHBoxLayout(actions);
    actionsLay->setContentsMargins(0, 0, 0, 0);
    actionsLay->setSpacing(4);
    actionsLay->addWidget(btnCopy_);
    actionsLay->addWidget(btnEdit);
    actionsLay->addWidget(btnRemove);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(10, 0, 8, 0);
    root->setSpacing(8);
    root->addWidget(enableCheck_, 0, Qt::AlignVCenter);
    root->addWidget(indexLabel_, 0, Qt::AlignVCenter);
    root->addWidget(titleLabel_, 1, Qt::AlignVCenter);
    root->addWidget(actions, 0, Qt::AlignVCenter);

    connect(enableCheck_, &QToolButton::toggled, this, [this](bool on) {
        enableCheck_->setIcon(flowEnableIcon(on));
        emit blockSelected(this);
        updateBlockStyle();
        updateActionEnabled();
    });
    connect(btnCopy_, &QToolButton::clicked, this, [this]() {
        emit blockSelected(this);
        emit copyRequested(this);
    });
    connect(btnEdit, &QToolButton::clicked, this, [this]() {
        emit blockSelected(this);
        emit editRequested(this);
    });
    connect(btnRemove, &QToolButton::clicked, this, [this]() {
        emit blockSelected(this);
        emit removeFromFlowRequested(this);
    });

    setCaseName(caseName);
    updateBlockStyle();
    updateActionEnabled();
}

QString TestCaseBlock::caseName() const {
    return caseName_;
}

void TestCaseBlock::setCaseName(const QString& name) {
    caseName_ = name.trimmed();
    if (titleLabel_) {
        titleLabel_->setText(caseName_.isEmpty() ? QStringLiteral("(空白块)") : caseName_);
        QFont f = titleLabel_->font();
        f.setItalic(caseName_.isEmpty());
        titleLabel_->setFont(f);
    }
    updateBlockStyle();
    updateActionEnabled();
}

bool TestCaseBlock::isChecked() const {
    return enableCheck_ && enableCheck_->isChecked();
}

void TestCaseBlock::setChecked(bool checked) {
    if (!enableCheck_)
        return;
    QSignalBlocker blocker(enableCheck_);
    enableCheck_->setChecked(checked);
    enableCheck_->setIcon(flowEnableIcon(checked));
    updateBlockStyle();
    updateActionEnabled();
}

void TestCaseBlock::setFlowIndex(int index1Based) {
    if (!indexLabel_)
        return;
    indexLabel_->setText(QStringLiteral("%1").arg(index1Based, 2, 10, QLatin1Char('0')));
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

void TestCaseBlock::updateActionEnabled() {
    if (btnCopy_)
        btnCopy_->setEnabled(!isBlank());
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

void TestCaseBlock::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        startPos_ = event->pos();
        emit blockSelected(this);
    }
    QFrame::mousePressEvent(event);
}

void TestCaseBlock::mouseDoubleClickEvent(QMouseEvent* event) {
    // 自由工站：双击功能块 = 单步运行（与右键「运行」相同）
    if (event->button() == Qt::LeftButton && runMenuVisible_ && !isBlank()) {
        emit blockSelected(this);
        emit runRequested(this);
        event->accept();
        return;
    }
    QFrame::mouseDoubleClickEvent(event);
}

void TestCaseBlock::mouseMoveEvent(QMouseEvent* event) {
    if ((event->buttons() & Qt::LeftButton)
        && (event->pos() - startPos_).manhattanLength() >= QApplication::startDragDistance())
        performDrag();
    QFrame::mouseMoveEvent(event);
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
        QFrame::dragEnterEvent(event);
}

void TestCaseBlock::dragMoveEvent(QDragMoveEvent* event) {
    if (acceptsTestCaseFlowMime(event->mimeData()))
        event->acceptProposedAction();
    else
        QFrame::dragMoveEvent(event);
}

void TestCaseBlock::dropEvent(QDropEvent* event) {
    if (!acceptsTestCaseFlowMime(event->mimeData())) {
        QFrame::dropEvent(event);
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
    // 勿以功能块为父：块上 color 会污染菜单文字
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
    menu.addSeparator();
    menu.addAction(QStringLiteral("一键清空"), this, [this]() { emit clearRegionRequested(this); });
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
        flowContainer_->setAutoFillBackground(true);
        flowContainer_->setStyleSheet(QStringLiteral("background: #f8fafc;"));
        flowContainer_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    }
    if (scroll_) {
        scroll_->setStyleSheet(QStringLiteral(
            "QScrollArea {"
            "  border: 1px solid #e2e8f0;"
            "  border-radius: 8px;"
            "  background: #f8fafc;"
            "}"
            "QScrollArea > QWidget > QWidget { background: #f8fafc; }"));
        if (QWidget* vp = scroll_->viewport()) {
            vp->setAutoFillBackground(true);
            vp->setStyleSheet(QStringLiteral("background: #f8fafc;"));
            vp->installEventFilter(this);
        }
    }
    if (flowLayout_) {
        flowLayout_->setSpacing(4);
        flowLayout_->setContentsMargins(8, 8, 8, 8);
        flowLayout_->setSizeConstraint(QLayout::SetMinimumSize);
    }

    setupFailFlowRegionUi(btnFailClear);

    if (auto* toolbar =
            stationCombo_->parentWidget()->findChild<QHBoxLayout*>(QStringLiteral("horizontalLayout_testFlowToolbar"))) {
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
            // 下载/上传用例库改到工站右键菜单，工具栏按钮隐藏保留兼容
            btnDownloadSteps->hide();
        }
        if (auto* btnUploadSteps =
                dialogParent_->findChild<QPushButton*>(QStringLiteral("pushButton_testFlowUploadSteps"))) {
            btnUploadSteps->hide();
        }
        if (auto* btnSerialUi =
                dialogParent_->findChild<QPushButton*>(QStringLiteral("pushButton_testFlowSerialUi"))) {
            connect(btnSerialUi, &QPushButton::clicked, this, &TestFlowEditor::promptSerialUiConfig);
        }
        if (auto* btnBindings =
                dialogParent_->findChild<QPushButton*>(QStringLiteral("pushButton_testFlowStationBindings"))) {
            connect(btnBindings, &QPushButton::clicked, this, &TestFlowEditor::promptStationBindingsConfig);
        }
    }
    connect(btnSave, &QPushButton::clicked, this, [this]() { saveCurrentFlow(); });
    // 「一键清空」改到编排区/功能块右键菜单，工具栏按钮隐藏保留兼容
    if (btnClear)
        btnClear->hide();

    if (stopFlowOnTestFailCheck_) {
        stopFlowOnTestFailCheck_->setToolTip(
            QStringLiteral("勾选后，本工站主流程任一步失败即停止后续主流程步骤，并按序执行下方「测试失败执行区域」；"
                           "失败区为空则直接结束整单"));
    }
    if (scroll_) {
        scroll_->setToolTip(QStringLiteral("主测试流程。拖拽功能块到目标位置：落在某块上半部=插到其前，下半部=插到其后；"
                                           "也可选中块后点工具栏「上移」「下移」，或右键添加/一键清空。可拖到下方失败区。调整完须点「保存流程」。"));
    }

    QString initialKey = TestCaseStore::resolveFlowStationKey(
        TestCaseStore::loadSelectedFlowStationKey());
    if (initialKey.isEmpty())
        initialKey = QStringLiteral("FREE_WORK");
    refreshStationCombo(initialKey);
    persistSelectedStation(currentStationKey());
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
        failContainer_->setAutoFillBackground(true);
        failContainer_->setStyleSheet(QStringLiteral("background: #f8fafc;"));
        failContainer_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    }
    if (failScroll_) {
        failScroll_->setMinimumHeight(56);
        failScroll_->setStyleSheet(QStringLiteral(
            "QScrollArea {"
            "  border: 1px solid #e2e8f0;"
            "  border-radius: 8px;"
            "  background: #f8fafc;"
            "}"
            "QScrollArea > QWidget > QWidget { background: #f8fafc; }"));
        if (QWidget* vp = failScroll_->viewport()) {
            vp->setAutoFillBackground(true);
            vp->setStyleSheet(QStringLiteral("background: #f8fafc;"));
            vp->installEventFilter(this);
        }
    }
    failLayout_->setSpacing(4);
    failLayout_->setContentsMargins(8, 8, 8, 8);
    failLayout_->setSizeConstraint(QLayout::SetMinimumSize);

    installFlowHeightSplitter();

    if (dialogParent_) {
        if (auto* failLabel = dialogParent_->findChild<QLabel*>(QStringLiteral("label_testFlowFailRegion"))) {
            failLabel->setStyleSheet(QStringLiteral("font-weight: 600; color: #b45309;"));
        }
    }

    if (btnFailClear) {
        connect(btnFailClear, &QPushButton::clicked, this, [this]() {
            if (askYesNo(dialogParent_, QStringLiteral("确认"), QStringLiteral("清空测试失败执行区域？"))) {
                clearFailBlocks();
                refreshFlowScrollAreas();
            }
        });
    }
}

void TestFlowEditor::installFlowHeightSplitter() {
    if (!scroll_ || !failScroll_)
        return;
    QWidget* page = scroll_->parentWidget();
    auto* pageLayout = qobject_cast<QVBoxLayout*>(page ? page->layout() : nullptr);
    if (!page || !pageLayout)
        return;
    if (page->findChild<QSplitter*>(QStringLiteral("testFlowHeightSplitter")))
        return;

    const int mainIdx = pageLayout->indexOf(scroll_);
    if (mainIdx < 0)
        return;

    auto* failPanel = new QWidget(page);
    failPanel->setObjectName(QStringLiteral("testFlowFailPanel"));
    auto* failPanelLay = new QVBoxLayout(failPanel);
    failPanelLay->setContentsMargins(0, 0, 0, 0);
    failPanelLay->setSpacing(4);

    if (auto* failToolbarLay =
            page->findChild<QHBoxLayout*>(QStringLiteral("horizontalLayout_testFlowFailToolbar"))) {
        auto* failHeader = new QWidget(failPanel);
        auto* newTb = new QHBoxLayout(failHeader);
        newTb->setContentsMargins(0, 0, 0, 0);
        while (QLayoutItem* it = failToolbarLay->takeAt(0)) {
            if (QWidget* w = it->widget()) {
                newTb->addWidget(w);
            } else if (QSpacerItem* sp = it->spacerItem()) {
                newTb->addItem(new QSpacerItem(sp->sizeHint().width(), sp->sizeHint().height(),
                                               QSizePolicy::Expanding, QSizePolicy::Minimum));
            }
            delete it;
        }
        pageLayout->removeItem(failToolbarLay);
        delete failToolbarLay;
        failPanelLay->addWidget(failHeader);
    }

    pageLayout->removeWidget(scroll_);
    pageLayout->removeWidget(failScroll_);
    failPanelLay->addWidget(failScroll_, 1);

    auto* splitter = new QSplitter(Qt::Vertical, page);
    splitter->setObjectName(QStringLiteral("testFlowHeightSplitter"));
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(5);
    splitter->setStyleSheet(QStringLiteral(
        "QSplitter::handle:vertical {"
        "  background: #e2e8f0;"
        "  margin: 2px 24px;"
        "  border-radius: 2px;"
        "  height: 3px;"
        "}"));
    splitter->addWidget(scroll_);
    splitter->addWidget(failPanel);
    // 主流程占大头；失败区默认只留一条矮带，空着时不抢高度
    failPanel->setMinimumHeight(72);
    splitter->setStretchFactor(0, 8);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({720, 88});
    pageLayout->insertWidget(mainIdx, splitter, 1);
}

void TestFlowEditor::refreshFlowScrollAreas() {
    refreshBlockIndices(flowLayout_);
    refreshBlockIndices(failLayout_);
    auto refreshOne = [](QScrollArea* scroll, QVBoxLayout* layout) {
        if (!scroll || !layout)
            return;
        QWidget* content = scroll->widget();
        if (!content)
            return;
        layout->activate();
        const int h = qMax(layout->sizeHint().height(), layout->minimumSize().height());
        content->setMinimumHeight(h);
    };
    refreshOne(scroll_, flowLayout_);
    refreshOne(failScroll_, failLayout_);
}

void TestFlowEditor::refreshBlockIndices(QVBoxLayout* layout) {
    if (!layout)
        return;
    int index = 0;
    for (int i = 0; i < layout->count(); ++i) {
        auto* block = qobject_cast<TestCaseBlock*>(layout->itemAt(i)->widget());
        if (!block)
            continue;
        block->setFlowIndex(++index);
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
    menu.addSeparator();
    menu.addAction(QStringLiteral("一键清空"), this, [this, toFailRegion]() {
        const QString msg = toFailRegion ? QStringLiteral("清空测试失败执行区域？")
                                         : QStringLiteral("清空当前工站主流程编排区？不会删除磁盘 ini");
        if (!askYesNo(dialogParent_, QStringLiteral("确认"), msg))
            return;
        if (toFailRegion)
            clearFailBlocks();
        else
            clearBlocks();
        refreshFlowScrollAreas();
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
                if (!askYesNo(dialogParent_, QStringLiteral("确认删除"),
                              QStringLiteral("将永久删除 test_case 下以下功能块 ini：\r\n%1\r\n\r\n"
                                             "若当前编排区已引用，会同步移除对应块（须保存流程后生效）。")
                                  .arg(preview))) {
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
    refreshFlowScrollAreas();
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
                refreshFlowScrollAreas();
            }
            return true;
        }
        case QEvent::Resize:
            refreshFlowScrollAreas();
            break;
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

void TestFlowEditor::refreshStationCombo(const QString& selectKey, bool keepOutOfProductStation) {
    if (!stationCombo_)
        return;
    QString keyToSelect = selectKey.trimmed();
    if (keyToSelect.isEmpty() && keepOutOfProductStation)
        keyToSelect = currentStationKey();
    if (keyToSelect.isEmpty() && keepOutOfProductStation)
        keyToSelect = TestCaseStore::loadSelectedFlowStationKey();
    if (keyToSelect.isEmpty())
        keyToSelect = QStringLiteral("FREE_WORK");

    // 按当前产品只展示对应工站（自由工站/默认工站始终保留）
    QString productName = SETTINGS.value(QStringLiteral("Mes/Product_Name")).toString().trimmed();
    if (productName.isEmpty())
        productName = SETTINGS.value(QStringLiteral("MES/Product_Name")).toString().trimmed();
    const QVector<TestFlowStationEntry> filtered =
        TestCaseStore::loadFlowStationCatalogForProduct(productName);
    const QString resolvedSelect = TestCaseStore::resolveFlowStationKey(keyToSelect);
    QVector<TestFlowStationEntry> stations = filtered;
    bool selectInFiltered = false;
    for (const TestFlowStationEntry& entry : filtered) {
        if (entry.key == resolvedSelect) {
            selectInFiltered = true;
            break;
        }
    }
    // 主界面已选工站若被产品过滤掉，仍插入下拉首项（切换产品时不保留，见 onProductNameChanged）
    if (keepOutOfProductStation && !selectInFiltered && !resolvedSelect.isEmpty()) {
        const QString displayName = TestCaseStore::flowStationDisplayName(resolvedSelect);
        if (!displayName.isEmpty()) {
            stations.prepend({resolvedSelect, displayName});
        }
    }

    QSignalBlocker blocker(stationCombo_);
    stationCombo_->clear();
    for (const TestFlowStationEntry& entry : stations) {
        stationCombo_->addItem(entry.displayName, entry.key);
        const int row = stationCombo_->count() - 1;
        QString tip = entry.displayName;
        if (entry.key == resolvedSelect && !selectInFiltered && !productName.isEmpty()) {
            tip = QStringLiteral("%1（当前主界面工站，与产品型号 %2 的工站过滤不一致）")
                      .arg(entry.displayName, productName);
        }
        stationCombo_->setItemData(row, tip, Qt::ToolTipRole);
    }

    int idx = stationCombo_->findData(resolvedSelect.isEmpty() ? keyToSelect : resolvedSelect);
    if (idx < 0)
        idx = stationCombo_->findText(TestCaseStore::flowStationDisplayName(keyToSelect));
    if (idx < 0 && !stations.isEmpty())
        idx = 0;
    if (idx >= 0)
        stationCombo_->setCurrentIndex(idx);
    adjustComboPopupToContents(stationCombo_);
}

void TestFlowEditor::syncFromPersistedSelection() {
    if (!uiBound_ || !stationCombo_)
        return;
    QString key = TestCaseStore::resolveFlowStationKey(TestCaseStore::loadSelectedFlowStationKey());
    if (key.isEmpty())
        key = QStringLiteral("FREE_WORK");
    refreshStationCombo(key);
    const QString selectedKey = currentStationKey();
    if (selectedKey.isEmpty())
        return;
    if (selectedKey != lastLoadedStationKey_) {
        if (hasUnsavedChanges()) {
            if (!confirmDiscardOrSaveOnLeave())
                return;
        }
        persistSelectedStation(selectedKey);
        reloadCurrentStation();
    }
    stationComboPrevIndex_ = stationCombo_->currentIndex();
}

void TestFlowEditor::onProductNameChanged() {
    if (!uiBound_ || !stationCombo_)
        return;
    refreshStationCombo(QString(), false);
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
    menu->addSeparator();
    QAction* const actDownloadSteps = menu->addAction(QStringLiteral("下载用例库"));
    QAction* const actUploadSteps = menu->addAction(QStringLiteral("上传用例库"));

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
    connect(actDownloadSteps, &QAction::triggered, this, &TestFlowEditor::startDownloadStepsLibrary,
            Qt::QueuedConnection);
    connect(actUploadSteps, &QAction::triggered, this, &TestFlowEditor::startUploadStepsLibrary,
            Qt::QueuedConnection);
    return menu;
}

void TestFlowEditor::setupStationComboContextMenu() {
    if (!stationCombo_)
        return;

    stationCombo_->setToolTip(
        QStringLiteral("选择要编辑的测试工站（随 Mes/Product_Name 产品型号过滤）；"
                       "数据来自 test_case/总的测试流程.ini；右键可管理工站与用例库同步"));

    if (auto* manageBtn =
            stationCombo_->parentWidget()->findChild<QToolButton*>(QStringLiteral("toolButton_testFlowStationMenu"))) {
        // 固定菜单 + aboutToShow 刷新可用态；勿在 currentIndexChanged 里反复 setMenu，否则 InstantPopup 下 QAction 可能不触发
        auto* menu = new QMenu(manageBtn);
        applyFlowContextMenuStyle(menu);
        QAction* const actNew = menu->addAction(QStringLiteral("新建工站"));
        QAction* const actRename = menu->addAction(QStringLiteral("重命名工站"));
        QAction* const actCopy = menu->addAction(QStringLiteral("复制工站"));
        QAction* const actDel = menu->addAction(QStringLiteral("删除工站"));
        menu->addSeparator();
        QAction* const actDownloadSteps = menu->addAction(QStringLiteral("下载用例库"));
        QAction* const actUploadSteps = menu->addAction(QStringLiteral("上传用例库"));
        connect(actNew, &QAction::triggered, this, [this]() { promptAddFlowStation(); }, Qt::QueuedConnection);
        connect(actRename, &QAction::triggered, this, [this]() { promptRenameCurrentFlowStation(); },
                Qt::QueuedConnection);
        connect(actCopy, &QAction::triggered, this, [this]() { promptCopyCurrentFlowStation(); }, Qt::QueuedConnection);
        connect(actDel, &QAction::triggered, this, [this]() { promptRemoveCurrentFlowStation(); }, Qt::QueuedConnection);
        connect(actDownloadSteps, &QAction::triggered, this, &TestFlowEditor::startDownloadStepsLibrary,
                Qt::QueuedConnection);
        connect(actUploadSteps, &QAction::triggered, this, &TestFlowEditor::startUploadStepsLibrary,
                Qt::QueuedConnection);
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
    bool usedCloud = false;
    QString preferredKey;
    if (!promptFlowStationKeyFromCloud(dialogParent_, displayName, &preferredKey, &usedCloud))
        return;
    QString err;
    if (!TestCaseStore::addFlowStation(displayName, &err, preferredKey)) {
        QMessageBox::warning(dialogParent_, QStringLiteral("无法新建"), err);
        return;
    }
    const QString newKey = TestCaseStore::resolveFlowStationKey(displayName);
    refreshStationCombo(newKey);
    persistSelectedStation(currentStationKey());
    reloadCurrentStation();
    if (usedCloud) {
        QMessageBox::information(dialogParent_, QStringLiteral("已新建"),
                                 QStringLiteral("工站「%1」已创建，云端编号 %2")
                                     .arg(displayName, newKey));
    }
}

void TestFlowEditor::promptSerialUiConfig() {
    if (!dialogParent_)
        return;
    const QString key = currentStationKey();
    if (key.isEmpty()) {
        QMessageBox::warning(dialogParent_, QStringLiteral("无法配置"), QStringLiteral("请先选择工站"));
        return;
    }

    TestCaseSerialUiConfig cfg = TestCaseStore::loadStationSerialUiConfig(key);

    QDialog dlg(dialogParent_);
    dlg.setWindowTitle(QStringLiteral("工位串口显示配置"));
    auto* root = new QVBoxLayout(&dlg);

    auto* stationHint = new QLabel(
        QStringLiteral("当前工站：%1").arg(TestCaseStore::flowStationDisplayName(key)), &dlg);
    root->addWidget(stationHint);

    auto* workUiGroup = new QGroupBox(QStringLiteral("工位界面串口"), &dlg);
    auto* workUiForm = new QFormLayout(workUiGroup);

    auto* dongleVisible = new QCheckBox(QStringLiteral("显示"), workUiGroup);
    dongleVisible->setChecked(cfg.dongleVisible);
    QComboBox* dongleBaudCombo =
        makeSerialBaudCombo(workUiGroup, cfg.dongleBaudRate > 0 ? cfg.dongleBaudRate : 921600);
    auto* dongleRow = new QHBoxLayout;
    dongleRow->addWidget(dongleVisible);
    dongleRow->addStretch(1);
    dongleRow->addWidget(new QLabel(QStringLiteral("波特率"), workUiGroup));
    dongleRow->addWidget(dongleBaudCombo);
    workUiForm->addRow(QStringLiteral("Dongle串口"), dongleRow);

    auto* jigVisible = new QCheckBox(QStringLiteral("显示"), workUiGroup);
    jigVisible->setChecked(cfg.jigVisible);
    auto* jigLabel = new QLineEdit(cfg.jigLabel, workUiGroup);
    jigLabel->setPlaceholderText(QStringLiteral("治具串口"));
    QComboBox* jigBaudCombo = makeSerialBaudCombo(workUiGroup, cfg.jigBaudRate);
    auto* jigRow = new QHBoxLayout;
    jigRow->addWidget(jigVisible);
    jigRow->addWidget(jigLabel, 1);
    jigRow->addWidget(new QLabel(QStringLiteral("波特率"), workUiGroup));
    jigRow->addWidget(jigBaudCombo);
    workUiForm->addRow(QStringLiteral("治具串口"), jigRow);

    auto* productVisible = new QCheckBox(QStringLiteral("显示"), workUiGroup);
    productVisible->setChecked(cfg.productVisible);
    auto* productLabel = new QLineEdit(cfg.productLabel, workUiGroup);
    productLabel->setPlaceholderText(QStringLiteral("产品串口(仪器)"));
    QComboBox* productBaudCombo = makeSerialBaudCombo(workUiGroup, cfg.productBaudRate);
    auto* productRow = new QHBoxLayout;
    productRow->addWidget(productVisible);
    productRow->addWidget(productLabel, 1);
    productRow->addWidget(new QLabel(QStringLiteral("波特率"), workUiGroup));
    productRow->addWidget(productBaudCombo);
    workUiForm->addRow(QStringLiteral("产品串口"), productRow);

    auto* usbVisible = new QCheckBox(QStringLiteral("显示"), workUiGroup);
    usbVisible->setChecked(cfg.usbVisible);
    auto* usbLabel = new QLineEdit(cfg.usbLabel, workUiGroup);
    usbLabel->setPlaceholderText(QStringLiteral("万用表串口"));
    QComboBox* usbBaudCombo = makeSerialBaudCombo(workUiGroup, cfg.usbBaudRate);
    auto* usbRow = new QHBoxLayout;
    usbRow->addWidget(usbVisible);
    usbRow->addWidget(usbLabel, 1);
    usbRow->addWidget(new QLabel(QStringLiteral("波特率"), workUiGroup));
    usbRow->addWidget(usbBaudCombo);
    workUiForm->addRow(QStringLiteral("万用表串口"), usbRow);
    root->addWidget(workUiGroup);

    auto* menuGroup = new QGroupBox(QStringLiteral("顶部菜单"), &dlg);
    auto* menuForm = new QFormLayout(menuGroup);
    auto* fixtureMenuLabel = new QLineEdit(cfg.fixtureMenuLabel, menuGroup);
    fixtureMenuLabel->setPlaceholderText(QStringLiteral("连接治具串口"));
    menuForm->addRow(QStringLiteral("连接菜单名称"), fixtureMenuLabel);
    QComboBox* fixtureBaudCombo = makeSerialBaudCombo(menuGroup, cfg.fixtureBaudRate > 0 ? cfg.fixtureBaudRate : 9600);
    menuForm->addRow(QStringLiteral("治具波特率"), fixtureBaudCombo);
    auto* plcFixtureMenuLabel = new QLineEdit(cfg.plcFixtureMenuLabel, menuGroup);
    plcFixtureMenuLabel->setPlaceholderText(QStringLiteral("连接PLC治具串口"));
    menuForm->addRow(QStringLiteral("PLC连接菜单名称"), plcFixtureMenuLabel);
    QComboBox* plcFixtureBaudCombo =
        makeSerialBaudCombo(menuGroup, cfg.plcFixtureBaudRate > 0 ? cfg.plcFixtureBaudRate : 9600);
    menuForm->addRow(QStringLiteral("PLC治具波特率"), plcFixtureBaudCombo);
    root->addWidget(menuGroup);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const auto normalizedLabel = [](QLineEdit* edit, const QString& fallback) -> QString {
        const QString text = edit->text().trimmed();
        return text.isEmpty() ? fallback : text;
    };

    TestCaseSerialUiConfig next;
    next.dongleVisible = dongleVisible->isChecked();
    next.dongleBaudRate = serialBaudFromCombo(dongleBaudCombo, 921600);
    next.jigVisible = jigVisible->isChecked();
    next.jigBaudRate = serialBaudFromCombo(jigBaudCombo, 115200);
    next.productVisible = productVisible->isChecked();
    next.usbVisible = usbVisible->isChecked();
    next.jigLabel = normalizedLabel(jigLabel, QStringLiteral("治具串口"));
    next.productLabel = normalizedLabel(productLabel, QStringLiteral("产品串口(仪器)"));
    next.productBaudRate = serialBaudFromCombo(productBaudCombo, 115200);
    next.usbLabel = normalizedLabel(usbLabel, QStringLiteral("万用表串口"));
    next.usbBaudRate = serialBaudFromCombo(usbBaudCombo, 115200);
    next.fixtureMenuLabel = normalizedLabel(fixtureMenuLabel, QStringLiteral("连接治具串口"));
    next.fixtureBaudRate = serialBaudFromCombo(fixtureBaudCombo, 9600);
    next.plcFixtureMenuLabel = normalizedLabel(plcFixtureMenuLabel, QStringLiteral("连接PLC治具串口"));
    next.plcFixtureBaudRate = serialBaudFromCombo(plcFixtureBaudCombo, 9600);

    if (!TestCaseStore::saveStationSerialUiConfig(key, next)) {
        QMessageBox::warning(dialogParent_, QStringLiteral("保存失败"), QStringLiteral("无法写入工站 flow.ini"));
        return;
    }
    persistSelectedStation(key);
    QMessageBox::information(dialogParent_, QStringLiteral("已保存"),
                             QStringLiteral("串口显示配置已写入当前工站 flow.ini，已打开的自由工站会立即生效。"));
}

void TestFlowEditor::promptStationBindingsConfig() {
    if (!dialogParent_)
        return;
    const QString key = currentStationKey();
    if (key.isEmpty()) {
        QMessageBox::warning(dialogParent_, QStringLiteral("无法配置"), QStringLiteral("请先选择工站"));
        return;
    }

    TestCaseStationBindings cfg = TestCaseStore::loadStationBindings(key);

    QDialog dlg(dialogParent_);
    dlg.setWindowTitle(QStringLiteral("工站设备能力绑定"));
    dlg.setWindowFlags(dlg.windowFlags() | Qt::WindowMaximizeButtonHint);

    auto* root = new QVBoxLayout(&dlg);
    root->setSpacing(8);
    root->setContentsMargins(12, 10, 12, 10);

    // 顶部卡片：工站信息 + 重新扫描按钮横向并排，大幅节省纵向空间
    auto* topCard = new QWidget(&dlg);
    auto* topLay = new QHBoxLayout(topCard);
    topLay->setContentsMargins(0, 0, 0, 0);
    topLay->setSpacing(12);

    auto* tipLabel = new QLabel(
        QStringLiteral(
            "<b>当前工站：%1</b><br>"
            "<span style='color: #64748b; font-size: 12px;'>"
            "勾选本站会用到的产品协议、治具与外设；步骤配置时将只显示已绑定能力的指令。<br>"
            "未手工保存过绑定时，系统会自动按本站现有步骤扫描并写入（存量工站无需逐站重配）。</span>")
            .arg(TestCaseStore::flowStationDisplayName(key)),
        topCard);
    tipLabel->setWordWrap(true);
    topLay->addWidget(tipLabel, 1);

    auto* scanBtn = new QPushButton(QStringLiteral("按现有步骤重新扫描"), topCard);
    scanBtn->setToolTip(QStringLiteral("扫描本站主流程与失败区步骤里的通道/协议/外设，勾选结果覆盖当前对话框（需再点确定才保存）"));
    scanBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    topLay->addWidget(scanBtn, 0, Qt::AlignVCenter);

    root->addWidget(topCard);

    auto fillCheckList = [](QListWidget* list, const QStringList& keys, const QStringList& labels,
                            const QStringList& selected) {
        list->clear();
        for (int i = 0; i < keys.size(); ++i) {
            auto* item = new QListWidgetItem(i < labels.size() ? labels.at(i) : keys.at(i), list);
            item->setData(Qt::UserRole, keys.at(i));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(selected.contains(keys.at(i)) ? Qt::Checked : Qt::Unchecked);
        }
    };

    // 主体部分放入可滚动的 QScrollArea 中，确保小屏/矮屏笔记本下底部确定/取消按钮永不遮挡
    auto* scrollArea = new QScrollArea(&dlg);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* scrollContent = new QWidget(scrollArea);
    auto* scrollLayout = new QHBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 4, 0, 4);
    scrollLayout->setSpacing(10);

    // 左列：产品协议、其它通道、程控电源/SCPI
    auto* leftCol = new QVBoxLayout();
    leftCol->setSpacing(8);

    auto* productGroup = new QGroupBox(QStringLiteral("产品协议（可多选）"), scrollContent);
    auto* productList = new QListWidget(productGroup);
    productList->setMinimumHeight(100);
    {
        QStringList keys;
        QStringList labels;
        for (TestCaseProductProtocol p :
             {TestCaseProductProtocol::Qfctp, TestCaseProductProtocol::Qpb, TestCaseProductProtocol::Qroot,
              TestCaseProductProtocol::Qroot2, TestCaseProductProtocol::Qaiot, TestCaseProductProtocol::QaiotV2}) {
            keys << DeviceCmdCatalog::productProtocolToIni(p);
            labels << DeviceCmdCatalog::productProtocolUiLabel(p);
        }
        fillCheckList(productList, keys, labels, cfg.productProtocols);
    }
    auto* productLay = new QVBoxLayout(productGroup);
    productLay->setSpacing(6);
    productLay->addWidget(productList);
    auto* enableProduct = new QCheckBox(QStringLiteral("启用产品通道指令"), productGroup);
    enableProduct->setChecked(cfg.enableProduct);
    productLay->addWidget(enableProduct);
    auto* enableProductSerial = new QCheckBox(QStringLiteral("启用产品串口指令"), productGroup);
    enableProductSerial->setChecked(cfg.enableProductSerial);
    productLay->addWidget(enableProductSerial);
    leftCol->addWidget(productGroup);

    auto* channelGroup = new QGroupBox(QStringLiteral("其它通道"), scrollContent);
    auto* channelLay = new QHBoxLayout(channelGroup);
    channelLay->setSpacing(12);
    auto* enableDongle = new QCheckBox(QStringLiteral("Dongle"), channelGroup);
    enableDongle->setChecked(cfg.enableDongle);
    channelLay->addWidget(enableDongle);
    auto* enableCloud = new QCheckBox(QStringLiteral("云端（三元组等）"), channelGroup);
    enableCloud->setChecked(cfg.enableCloud);
    channelLay->addWidget(enableCloud);
    channelLay->addStretch();
    leftCol->addWidget(channelGroup);

    auto* scpiGroup = new QGroupBox(QStringLiteral("程控电源 / SCPI（可多选）"), scrollContent);
    auto* scpiList = new QListWidget(scpiGroup);
    scpiList->setMinimumHeight(90);
    {
        const QStringList keys = ScpiPeriphCmdCatalog::allDeviceKeys();
        QStringList labels;
        for (const QString& k : keys)
            labels << ScpiPeriphCmdCatalog::deviceUiLabel(ScpiPeriphCmdCatalog::deviceFromIni(k));
        fillCheckList(scpiList, keys, labels, cfg.scpiDevices);
    }
    auto* scpiLay = new QVBoxLayout(scpiGroup);
    scpiLay->addWidget(scpiList);
    leftCol->addWidget(scpiGroup);

    // 右列：治具、Modbus 外设 / PLC
    auto* rightCol = new QVBoxLayout();
    rightCol->setSpacing(8);

    auto* fixtureGroup = new QGroupBox(QStringLiteral("治具（可多选）"), scrollContent);
    auto* fixtureList = new QListWidget(fixtureGroup);
    fixtureList->setMinimumHeight(110);
    {
        QStringList keys;
        QStringList labels;
        for (TestCaseFixtureProtocol p :
             {TestCaseFixtureProtocol::Xwd, TestCaseFixtureProtocol::UsbCamera,
              TestCaseFixtureProtocol::GigECamera, TestCaseFixtureProtocol::Pcba,
              TestCaseFixtureProtocol::Asd9026a, TestCaseFixtureProtocol::JieliBtBox,
              TestCaseFixtureProtocol::VesLight}) {
            keys << FixturePcbaCmdCatalog::fixtureProtocolToIni(p);
            labels << FixturePcbaCmdCatalog::fixtureProtocolUiLabel(p);
        }
        fillCheckList(fixtureList, keys, labels, cfg.fixtures);
    }
    auto* fixtureLay = new QVBoxLayout(fixtureGroup);
    fixtureLay->addWidget(fixtureList);
    rightCol->addWidget(fixtureGroup);

    auto* modbusGroup = new QGroupBox(QStringLiteral("Modbus 外设 / PLC（可多选）"), scrollContent);
    auto* modbusList = new QListWidget(modbusGroup);
    modbusList->setMinimumHeight(110);
    {
        const QStringList keys = ModbusPeriphCmdCatalog::allDeviceKeys();
        QStringList labels;
        for (const QString& k : keys)
            labels << ModbusDeviceCatalog::deviceRouteUiLabel(ModbusDeviceCatalog::deviceRouteFromIni(k));
        fillCheckList(modbusList, keys, labels, cfg.modbusDevices);
    }
    auto* modbusLay = new QVBoxLayout(modbusGroup);
    modbusLay->addWidget(modbusList);
    rightCol->addWidget(modbusGroup);

    scrollLayout->addLayout(leftCol, 1);
    scrollLayout->addLayout(rightCol, 1);
    scrollArea->setWidget(scrollContent);
    root->addWidget(scrollArea, 1);

    auto applyCfgToUi = [=](const TestCaseStationBindings& b) {
        QStringList protoKeys;
        QStringList protoLabels;
        for (TestCaseProductProtocol p :
             {TestCaseProductProtocol::Qfctp, TestCaseProductProtocol::Qpb, TestCaseProductProtocol::Qroot,
              TestCaseProductProtocol::Qroot2, TestCaseProductProtocol::Qaiot, TestCaseProductProtocol::QaiotV2}) {
            protoKeys << DeviceCmdCatalog::productProtocolToIni(p);
            protoLabels << DeviceCmdCatalog::productProtocolUiLabel(p);
        }
        fillCheckList(productList, protoKeys, protoLabels, b.productProtocols);
        enableProduct->setChecked(b.enableProduct);
        enableProductSerial->setChecked(b.enableProductSerial);
        enableDongle->setChecked(b.enableDongle);
        enableCloud->setChecked(b.enableCloud);
        QStringList fixKeys;
        QStringList fixLabels;
        for (TestCaseFixtureProtocol p :
             {TestCaseFixtureProtocol::Xwd, TestCaseFixtureProtocol::UsbCamera,
              TestCaseFixtureProtocol::GigECamera, TestCaseFixtureProtocol::Pcba,
              TestCaseFixtureProtocol::Asd9026a, TestCaseFixtureProtocol::JieliBtBox,
              TestCaseFixtureProtocol::VesLight}) {
            fixKeys << FixturePcbaCmdCatalog::fixtureProtocolToIni(p);
            fixLabels << FixturePcbaCmdCatalog::fixtureProtocolUiLabel(p);
        }
        fillCheckList(fixtureList, fixKeys, fixLabels, b.fixtures);
        {
            const QStringList keys = ModbusPeriphCmdCatalog::allDeviceKeys();
            QStringList labels;
            for (const QString& k : keys)
                labels << ModbusDeviceCatalog::deviceRouteUiLabel(ModbusDeviceCatalog::deviceRouteFromIni(k));
            fillCheckList(modbusList, keys, labels, b.modbusDevices);
        }
        {
            const QStringList keys = ScpiPeriphCmdCatalog::allDeviceKeys();
            QStringList labels;
            for (const QString& k : keys)
                labels << ScpiPeriphCmdCatalog::deviceUiLabel(ScpiPeriphCmdCatalog::deviceFromIni(k));
            fillCheckList(scpiList, keys, labels, b.scpiDevices);
        }
    };

    connect(scanBtn, &QPushButton::clicked, &dlg, [&, key]() {
        const TestCaseStationBindings scanned = TestCaseStore::inferStationBindingsFromFlowSteps(key);
        applyCfgToUi(scanned);
        QMessageBox::information(
            &dlg, QStringLiteral("扫描完成"),
            QStringLiteral("已按本站现有步骤勾选能力，请确认后点「确定」保存到 flow.ini。"));
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    root->addWidget(buttons, 0);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    // 屏幕自适应：针对小屏笔记本（1366x768、缩放 125%/150% 等矮屏）动态计算最优窗口大小与居中显示
    dlg.setMinimumSize(560, 360);
    QScreen* scr = nullptr;
    if (dialogParent_ && dialogParent_->window())
        scr = dialogParent_->window()->screen();
    if (!scr)
        scr = QGuiApplication::primaryScreen();

    const QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1024, 768);
    const int maxH = qMax(360, avail.height() - 50);
    const int maxW = qMax(560, avail.width() - 50);
    const int targetW = qBound(560, qMin(760, int(avail.width() * 0.7)), maxW);
    const int targetH = qBound(360, qMin(560, int(avail.height() * 0.75)), maxH);
    dlg.resize(targetW, targetH);

    if (dialogParent_ && dialogParent_->window() && dialogParent_->window()->isVisible()) {
        const QRect parentGeo = dialogParent_->window()->geometry();
        dlg.move(parentGeo.center().x() - targetW / 2, parentGeo.center().y() - targetH / 2);
    } else {
        dlg.move(avail.center().x() - targetW / 2, avail.center().y() - targetH / 2);
    }

    QRect geo = dlg.geometry();
    if (geo.bottom() > avail.bottom())
        geo.moveBottom(avail.bottom() - 10);
    if (geo.top() < avail.top())
        geo.moveTop(avail.top() + 10);
    if (geo.right() > avail.right())
        geo.moveRight(avail.right() - 10);
    if (geo.left() < avail.left())
        geo.moveLeft(avail.left() + 10);
    dlg.setGeometry(geo);

    if (dlg.exec() != QDialog::Accepted)
        return;

    auto collectChecked = [](QListWidget* list) -> QStringList {
        QStringList out;
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem* item = list->item(i);
            if (item && item->checkState() == Qt::Checked)
                out << item->data(Qt::UserRole).toString();
        }
        return out;
    };

    TestCaseStationBindings next;
    next.configured = true;
    next.productProtocols = collectChecked(productList);
    next.enableProduct = enableProduct->isChecked();
    next.enableProductSerial = enableProductSerial->isChecked();
    next.enableDongle = enableDongle->isChecked();
    next.enableCloud = enableCloud->isChecked();
    next.fixtures = collectChecked(fixtureList);
    next.modbusDevices = collectChecked(modbusList);
    next.scpiDevices = collectChecked(scpiList);

    if (!TestCaseStore::saveStationBindings(key, next)) {
        QMessageBox::warning(dialogParent_, QStringLiteral("保存失败"), QStringLiteral("无法写入工站 flow.ini"));
        return;
    }
    persistSelectedStation(key);
    QMessageBox::information(
        dialogParent_, QStringLiteral("已保存"),
        QStringLiteral("设备能力绑定已写入当前工站 flow.ini [StationBindings]。\n"
                       "之后编辑步骤时，请用「目标设备」选择本站已绑定的能力。\n"
                       "不影响自动扫码 [PreStart_Monitor] 与 PLC/XWD 共享串口。"));
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

    bool usedCloud = false;
    QString preferredKey;
    if (!promptFlowStationKeyFromCloud(dialogParent_, newName, &preferredKey, &usedCloud))
        return;

    QString newKey;
    QString err;
    if (!TestCaseStore::copyFlowStation(sourceKey, newName, entries, stopFlow, &newKey, &err, preferredKey)) {
        QMessageBox::warning(dialogParent_, QStringLiteral("无法复制"), err);
        return;
    }
    // 复制目录后覆盖主流程；显式再写失败区，避免仅拷贝未含未保存编辑
    TestCaseStore::saveStationFlowItems(newKey, entries, failEntries, stopFlow);

    lastLoadedStationKey_.clear();
    refreshStationCombo(newKey);
    persistSelectedStation(newKey);
    reloadCurrentStation();
    const QString keyHint = usedCloud ? QStringLiteral("，云端编号 %1").arg(newKey) : QString();
    QMessageBox::information(dialogParent_, QStringLiteral("已复制"),
                             QStringLiteral("已创建工站「%1」%2，流程已写入 总的测试流程.ini")
                                 .arg(TestCaseStore::flowStationDisplayName(newKey), keyHint));
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
    if (!askYesNo(dialogParent_, QStringLiteral("确认删除"),
                  QStringLiteral("删除工站「%1」及其在 总的测试流程.ini 中的流程配置？\r\n"
                                 "不会删除 test_case 下的功能块 ini。")
                      .arg(label))) {
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
    connect(block, &TestCaseBlock::clearRegionRequested, this, [this](TestCaseBlock* b) {
        QVBoxLayout* lay = flowLayoutOfBlock(b);
        const bool toFailRegion = lay == failLayout_;
        const QString msg = toFailRegion ? QStringLiteral("清空测试失败执行区域？")
                                         : QStringLiteral("清空当前工站主流程编排区？不会删除磁盘 ini");
        if (!askYesNo(dialogParent_, QStringLiteral("确认"), msg))
            return;
        if (toFailRegion)
            clearFailBlocks();
        else
            clearBlocks();
        refreshFlowScrollAreas();
    });
    connect(block, &TestCaseBlock::removeFromFlowRequested, this, [this](TestCaseBlock* b) {
        if (selectedBlock_ == b)
            setSelectedBlock(nullptr);
        if (QVBoxLayout* lay = flowLayoutOfBlock(b))
            lay->removeWidget(b);
        b->deleteLater();
        QTimer::singleShot(0, this, [this]() { refreshFlowScrollAreas(); });
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
    refreshFlowScrollAreas();
}

void TestFlowEditor::appendFailBlock(const QString& caseName, bool enabled) {
    TestCaseBlock* block = createBlock(failLayout_, failContainer_, caseName, enabled);
    if (!block || !failLayout_)
        return;
    failLayout_->addWidget(block);
    setSelectedBlock(block);
    refreshFlowScrollAreas();
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
    refreshFlowScrollAreas();
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
    // UI 改流程后清「已同步洁净」缓存，量产开测须重新对齐正式版
    TestCasePolicyService::markDirtyAfterUiEdit(key);
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
        const auto saveAnswer = askQuestion(
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

    if (!askYesNo(dialogParent_, QStringLiteral("上传本工站用例"),
                  QStringLiteral("上传当前工站「%1」到云端草稿（不影响其他工站）。\n"
                                 "说明：%2\n\n网页「合入 + 发布」后，其他电脑才能下载到。\n\n确认上传？")
                      .arg(label, remark))) {
        return;
    }

    persistSelectedStation(stationKey);
    auto* watcher = new QFutureWatcher<TestCaseSyncService::SyncResult>(this);
    connect(watcher, &QFutureWatcher<TestCaseSyncService::SyncResult>::finished, this, [this, watcher]() {
        const TestCaseSyncService::SyncResult result = watcher->result();
        if (result.ok) {
            const QString resolvedKey = TestCaseStore::resolveFlowStationKey(result.stationKey.trimmed());
            if (!resolvedKey.isEmpty()) {
                refreshStationCombo(resolvedKey);
                persistSelectedStation(resolvedKey);
                reloadCurrentStation();
            }
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

    if (!askYesNo(dialogParent_, QStringLiteral("上传用例库"),
                  QStringLiteral("上传本机共享用例库（test_case/steps）到云端草稿。\n"
                                 "说明：%1\n\n不会上传各工站 profiles；网页「合入 + 发布」后，"
                                 "其他电脑才能「下载用例库」。\n\n确认上传？")
                      .arg(remark))) {
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
    if (!askYesNo(dialogParent_, QStringLiteral("下载用例库"),
                  QStringLiteral("从云端下载已发布的共享用例库，仅覆盖本机 test_case/steps。\n"
                                 "不会动各工站 profiles；未发布的草稿也拉不到。\n\n确认下载？"))) {
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

                if (!askYesNo(dialogParent_, QStringLiteral("下载工站用例"),
                              QStringLiteral("从云端下载已发布的正式用例，仅覆盖本机工站「%1」。\n"
                                             "不会动其他工站；未发布的草稿也拉不到。\n\n确认下载？")
                                  .arg(label))) {
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

    // 不挂 dialogParent_，避免继承壳层 QSS 导致布局比例异常
    QMessageBox box(QMessageBox::Question, QStringLiteral("未保存的更改"),
                    QStringLiteral("测试流程编排已修改，是否保存？"), QMessageBox::NoButton, nullptr);
    box.setWindowModality(Qt::ApplicationModal);
    auto* btnSave = box.addButton(QStringLiteral("保存"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(btnSave);
    box.adjustSize();
    if (dialogParent_) {
        const QRect ag = dialogParent_->window() ? dialogParent_->window()->frameGeometry()
                                                 : dialogParent_->frameGeometry();
        box.move(ag.center() - QPoint(box.width() / 2, box.height() / 2));
    }
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
    QTimer::singleShot(0, this, [this]() { refreshFlowScrollAreas(); });
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
