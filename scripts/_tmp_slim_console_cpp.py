# -*- coding: utf-8 -*-
"""Slim bindConsoleShellWidgets / initConsoleShell; remove hand-drawn status icons."""
from pathlib import Path

path = Path("mainwindow.cpp")
text = path.read_bytes().decode("utf-8")
nl = "\r\n" if "\r\n" in text else "\n"

# ---- 1) Remove makeConsoleStatusIcon + applyConsoleStatusIcon + enum ----
start = text.find("enum ConsoleStatusIconKind")
end = text.find("// 预览页 CSS 进场")
if start < 0 or end < 0:
    raise SystemExit(f"status icon block markers missing {start} {end}")
text = text[:start] + text[end:]

# ---- 2) Replace bindConsoleShellWidgets ----
bind_start = text.find("void MainWindow::bindConsoleShellWidgets()")
bind_end = text.find("// 滚动区包裹后 Tab 页是 QScrollArea", bind_start)
if bind_start < 0 or bind_end < 0:
    raise SystemExit("bindConsoleShellWidgets markers missing")

new_bind = r'''void MainWindow::bindConsoleShellWidgets() {
    uartStatusLabel = uiFind<QLabel>(this, "uartStatusLabel");
    bleStatusLabel = uiFind<QLabel>(this, "bleStatusLabel");
    WifiStatusLabel = uiFind<QLabel>(this, "WifiStatusLabel");
    macLabel = uiFind<QLabel>(this, "macLabel");
    cloudLoginLabel = uiFind<QLabel>(this, "cloudLoginLabel");
    board_sn = uiFind<QLabel>(this, "board_sn");
    product_sn = uiFind<QLabel>(this, "product_sn");
    sub_pid = uiFind<QLabel>(this, "sub_pid");
    sku_id = uiFind<QLabel>(this, "sku_id");
    consolePageTitle_ = uiFind<QLabel>(this, "consolePageTitle");
    consolePageSub_ = uiFind<QLabel>(this, "consolePageSub");
    consoleStack_ = uiFind<QStackedWidget>(this, "consoleStack");
    // 壳子 / Splitter / ScrollArea 已在 mainwindow.ui；状态行等仍可能被 Designer 收成 Layout
    consoleShell_ = ui->consoleShell;
    consoleSidebar_ = ui->consoleSidebar;
    consoleMid_ = ui->consoleMid;
    consoleRail_ = ui->consoleRail;
    consoleConnectPage_ = ui->consoleConnectPage;
    consoleStatusDevice_ = wrapNamedLayout(uiFind<QLayout>(this, "consoleStatusCardLay"), uiFind<QLayout>(this, "consoleStatusDeviceLay"), QStringLiteral("consoleStatusDevice"));
    consoleStatusMotor_ = wrapNamedLayout(uiFind<QLayout>(this, "consoleStatusCardLay"), uiFind<QLayout>(this, "consoleStatusMotorLay"), QStringLiteral("consoleStatusMotor"));
    consoleStatusPump_ = wrapNamedLayout(uiFind<QLayout>(this, "consoleStatusCardLay"), uiFind<QLayout>(this, "consoleStatusPumpLay"), QStringLiteral("consoleStatusPump"));
    consoleStatusValve_ = wrapNamedLayout(uiFind<QLayout>(this, "consoleStatusCardLay"), uiFind<QLayout>(this, "consoleStatusValveLay"), QStringLiteral("consoleStatusValve"));
    consoleMetricBattery_ = wrapNamedLayout(uiFind<QLayout>(this, "consoleStatusCardLay"), uiFind<QLayout>(this, "consoleMetricBatteryLay"), QStringLiteral("consoleMetricBattery"));
    consoleMetricTemp_ = wrapNamedLayout(uiFind<QLayout>(this, "consoleStatusCardLay"), uiFind<QLayout>(this, "consoleMetricTempLay"), QStringLiteral("consoleMetricTemp"));
    consoleFieldReset_ = wrapNamedLayout(uiFind<QLayout>(this, "consoleCardStep1Lay"), uiFind<QLayout>(this, "consoleFieldResetLay"), QStringLiteral("consoleFieldReset"));
    consoleHeader_ = wrapNamedLayout(uiFind<QLayout>(this, "consoleMidLay"), uiFind<QLayout>(this, "consoleHeaderLay"), QStringLiteral("consoleHeader"));
    consoleTitleBox_ = wrapNamedLayout(uiFind<QLayout>(this, "consoleMidLay"), uiFind<QLayout>(this, "consoleTitleLay"), QStringLiteral("consoleTitleBox"));
    wrapNamedLayout(uiFind<QLayout>(this, "consoleSideGrid"), uiFind<QLayout>(this, "consoleSideBodyLay"), QStringLiteral("consoleSideBody"));
}

'''
new_bind = new_bind.replace("\n", nl)
text = text[:bind_start] + new_bind + text[bind_end:]

# ---- 3) Replace initConsoleShell body (keep function through end before installConsoleSegmentedSwitches) ----
init_start = text.find("void MainWindow::initConsoleShell()")
init_end = text.find("void MainWindow::installConsoleSegmentedSwitches()")
if init_start < 0 or init_end < 0:
    raise SystemExit("initConsoleShell markers missing")

new_init = r'''void MainWindow::initConsoleShell() {
    // Splitter / ScrollArea / 复位分段 / 卡片尺寸已在 .ui；这里只接信号、做运行时行为
    if (auto* split = ui->consoleShellSplit) {
        split->setStretchFactor(0, 0);
        split->setStretchFactor(1, 1);
        split->setStretchFactor(2, 0);
        split->setSizes({242, 1200, 320});
        connect(split, &QSplitter::splitterMoved, this, [this]() {
            consoleSidePhotoFitSize_ = QSize();
            updateConsoleSidePhotoFit();
            updateConsoleLogPromoFit();
            applyConsoleBusinessGridColumns(ui, consoleMid_, consoleShell_);
        });
    }
    if (consoleShell_)
        consoleShell_->installEventFilter(this);

    if (auto* railSplit = ui->consoleRailSplit) {
        railSplit->setStretchFactor(0, 0);
        railSplit->setStretchFactor(1, 1);
        if (QWidget* vp = ui->consoleStatusScroll ? ui->consoleStatusScroll->viewport() : nullptr)
            vp->setAutoFillBackground(false);
        const QList<QWidget*> statusRows = {
            consoleStatusDevice_,
            consoleStatusMotor_,
            consoleStatusPump_,
            consoleStatusValve_,
            consoleMetricBattery_,
            consoleMetricTemp_,
        };
        for (QWidget* row : statusRows) {
            if (!row)
                continue;
            row->setMinimumHeight(28);
            row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }
        if (ui->verticalGroupBox) {
            ui->verticalGroupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
            ui->verticalGroupBox->setMinimumHeight(0);
        }
        QTimer::singleShot(0, this, [this, railSplit]() {
            if (!ui->consoleStatusCard || !railSplit || !consoleRail_)
                return;
            const int railH = consoleRail_->height();
            const int handle = qMax(railSplit->handleWidth(), 10);
            const int minStatus = 88;
            const int minLog = 120;
            const int contentH = qMax(minStatus, ui->consoleStatusCard->sizeHint().height());
            int statusH = qBound(minStatus, contentH, qMax(minStatus, railH - minLog - handle));
            railSplit->setSizes({statusH, qMax(minLog, railH - statusH - handle)});
            updateConsoleLogPromoFit();
        });
        connect(railSplit, &QSplitter::splitterMoved, this, [this]() { updateConsoleLogPromoFit(); });
    }
    if (consoleConnectPage_) {
        if (auto* lay = consoleConnectPage_->layout())
            lay->setSizeConstraint(QLayout::SetMinimumSize);
    }

    if (ui->consoleBg) {
        ui->consoleBg->setAttribute(Qt::WA_TransparentForMouseEvents);
        ui->consoleBg->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        ui->consoleBg->setMinimumSize(0, 0);
    }
    if (ui->consoleSidePhoto) {
        ui->consoleSidePhoto->setAttribute(Qt::WA_TransparentForMouseEvents);
        ui->consoleSidePhoto->setScaledContents(false);
        ui->consoleSidePhoto->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        ui->consoleSidePhoto->setMinimumSize(0, 0);
        ui->consoleSidePhoto->installEventFilter(this);
        updateConsoleSidePhotoFit();
    }
    if (ui->consoleSideFade)
        ui->consoleSideFade->setAttribute(Qt::WA_TransparentForMouseEvents);
    if (ui->consolePromo) {
        ui->consolePromo->setAttribute(Qt::WA_TransparentForMouseEvents);
        ui->consolePromo->setAttribute(Qt::WA_TranslucentBackground);
        ui->consolePromo->setAutoFillBackground(false);
        QPalette promoPal = ui->consolePromo->palette();
        promoPal.setColor(QPalette::Window, Qt::transparent);
        promoPal.setColor(QPalette::Base, Qt::transparent);
        ui->consolePromo->setPalette(promoPal);
        ui->consolePromo->setScaledContents(false);
        ui->consolePromo->setAlignment(Qt::AlignBottom | Qt::AlignHCenter);
        ui->consolePromo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        ui->consolePromo->setMinimumSize(0, 0);
        ui->consolePromo->lower();
    }
    if (ui->consoleLogCard) {
        ui->consoleLogCard->installEventFilter(this);
        updateConsoleLogPromoFit();
    }
    if (ui->consoleBg)
        ui->consoleBg->lower();
    if (consoleShell_)
        consoleShell_->raise();

    consoleNavGroup_ = new QButtonGroup(this);
    consoleNavGroup_->setExclusive(true);
    consoleNavGroup_->addButton(ui->consoleNavConnect, 0);
    consoleNavGroup_->addButton(ui->consoleNavBiz, 1);
    consoleNavGroup_->addButton(ui->consoleNavPeriph, 2);
    consoleNavGroup_->addButton(ui->consoleNavDongle, 3);
    consoleNavGroup_->addButton(ui->consoleNavOta, 4);
    consoleNavGroup_->addButton(ui->consoleNavMisc, 5);
    connect(consoleNavGroup_, &QButtonGroup::idClicked, this, &MainWindow::selectConsoleNav);
    // .ui 已带 icon；此处再处理白底透明，选中态用 *-on.png
    applyConsoleNavIcons(
        {ui->consoleNavConnect, ui->consoleNavBiz, ui->consoleNavPeriph, ui->consoleNavDongle, ui->consoleNavOta,
         ui->consoleNavMisc},
        {QStringLiteral("nav-connect"), QStringLiteral("nav-business"), QStringLiteral("nav-peripheral"),
         QStringLiteral("nav-dongle"), QStringLiteral("nav-ota"), QStringLiteral("nav-misc")});
    if (ui->consoleLogClearBtn)
        ui->consoleLogClearBtn->setIcon(QIcon(QStringLiteral(":/stytle/picture/status/icon-refresh.png")));

    connect(ui->consoleSettingsBtn, &QToolButton::clicked, this, [this]() {
        if (settingMenuAction)
            settingMenuAction->trigger();
    });
    QMenu* helpMenu = nullptr;
    if (ui->menubar) {
        const auto acts = ui->menubar->actions();
        for (QAction* act : acts) {
            if (act && act->menu() && act->text().contains(QStringLiteral("帮助"))) {
                helpMenu = act->menu();
                break;
            }
        }
    }
    if (helpMenu) {
        connect(ui->consoleHelpBtn, &QToolButton::clicked, this, [this, helpMenu]() {
            helpMenu->popup(ui->consoleHelpBtn->mapToGlobal(QPoint(0, ui->consoleHelpBtn->height())));
        });
    }
    connect(ui->consoleLogClearBtn, &QPushButton::clicked, this, [this]() {
        if (ui->log)
            ui->log->clear();
        if (ui->msgEdit)
            ui->msgEdit->clear();
    });

    // 复位分段：控件在 .ui（consoleSegReset），这里只绑逻辑
    if (ui->is_reset_dongle && ui->resetOnBtn && ui->resetOffBtn) {
        auto* onBtn = ui->resetOnBtn;
        auto* offBtn = ui->resetOffBtn;
        auto syncResetSeg = [this, onBtn, offBtn]() {
            const bool on = ui->is_reset_dongle && ui->is_reset_dongle->isChecked();
            onBtn->setProperty("segActive", on);
            offBtn->setProperty("segActive", !on);
            offBtn->setProperty("segDanger", !on);
            onBtn->style()->unpolish(onBtn);
            onBtn->style()->polish(onBtn);
            offBtn->style()->unpolish(offBtn);
            offBtn->style()->polish(offBtn);
        };
        connect(onBtn, &QPushButton::clicked, this, [this, syncResetSeg]() {
            if (ui->is_reset_dongle)
                ui->is_reset_dongle->setChecked(true);
            syncResetSeg();
        });
        connect(offBtn, &QPushButton::clicked, this, [this, syncResetSeg]() {
            if (ui->is_reset_dongle)
                ui->is_reset_dongle->setChecked(false);
            syncResetSeg();
        });
        connect(ui->is_reset_dongle, &QCheckBox::toggled, this, [syncResetSeg](bool) { syncResetSeg(); });
        onBtn->setProperty("consoleSegBound", true);
        offBtn->setProperty("consoleSegBound", true);
        syncResetSeg();
    }

    if (ui->tabWidget)
        ui->tabWidget->tabBar()->hide();
    if (ui->consoleHeaderLay && ui->uartStatusLabel
        && consoleHeader_ && !consoleHeader_->findChild<QWidget*>(QStringLiteral("consoleFlowHost"))) {
        auto* headerLay = ui->consoleHeaderLay;
        QWidget* pillHost = new QWidget(consoleHeader_);
        pillHost->setObjectName(QStringLiteral("consoleFlowHost"));
        auto* flow = new ConsoleFlowLayout(pillHost, 0, 6, 6);
        const QList<QWidget*> pills = {
            ui->uartStatusLabel, ui->bleStatusLabel, ui->WifiStatusLabel,
            ui->macLabel, ui->cloudLoginLabel, ui->consoleFactoryLabel,
            ui->consoleBuildLabel,
        };
        for (QWidget* w : pills) {
            if (!w)
                continue;
            headerLay->removeWidget(w);
            w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            flow->addWidget(w);
        }
        pillHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        headerLay->insertWidget(0, pillHost, 1);
        consoleHeader_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    }
    if (ui->macLabel)
        ui->macLabel->setText(ui->macLabel->text().trimmed());
    if (ui->statusbar)
        ui->statusbar->hide();
    if (ui->menubar)
        ui->menubar->hide();
    if (ui->consoleConnectGrid && ui->consoleCardStep3) {
        ui->consoleConnectGrid->removeWidget(ui->consoleCardStep3);
        ui->consoleCardStep3->hide();
    }
    static const char* kHideConnectLegacy[] = {
        "consoleFlowNodePending4",
        "consoleFlowLabel4",
        "consoleFlowLine3",
        "label_dongle_serial",
        "label_pick_device",
        "label_name_range",
        "label_rssi_range",
        "label_2",
    };
    for (const char* name : kHideConnectLegacy) {
        if (QWidget* w = uiFind<QWidget>(this, name))
            w->hide();
    }
    if (auto* flowGrid = qobject_cast<QGridLayout*>(uiFind<QLayout>(this, "consoleFlowTrackLay"))) {
        flowGrid->setColumnStretch(1, 1);
        flowGrid->setColumnStretch(3, 1);
    }
    if (ui->consoleConnectGrid && ui->consoleCardStep4)
        ui->consoleConnectGrid->addWidget(ui->consoleCardStep4, 1, 0, 1, 2);
    if (ui->consoleNavConnect)
        ui->consoleNavConnect->setChecked(true);
    consoleUiRestoring = true;
    const int navId = qBound(0, SETTINGS.value(QStringLiteral("Window/ConsoleNavId"), 0).toInt(), 5);
    if (consoleNavGroup_) {
        if (QAbstractButton* btn = consoleNavGroup_->button(navId))
            btn->setChecked(true);
    }
    selectConsoleNav(navId);
    consoleRestoreTabByName(ui->tabWidget_debug_production,
                            SETTINGS.value(QStringLiteral("Window/ConsoleTabProduction")).toString());
    consoleRestoreTabByName(ui->tabWidget_debug_peripheral,
                            SETTINGS.value(QStringLiteral("Window/ConsoleTabPeripheral")).toString());
    consoleRestoreTabByName(ui->tabWidget_debug_dongle,
                            SETTINGS.value(QStringLiteral("Window/ConsoleTabDongle")).toString());
    consoleRestoreTabByName(ui->tabWidget_ota, SETTINGS.value(QStringLiteral("Window/ConsoleTabOta")).toString());
    consoleRestoreTabByName(ui->tabWidget_debug_misc,
                            SETTINGS.value(QStringLiteral("Window/ConsoleTabMisc")).toString());
    consoleRestoreTabByName(ui->consoleLogTabs, SETTINGS.value(QStringLiteral("Window/ConsoleLogTab")).toString());
    consoleUiRestoring = false;

    auto bindPersistTab = [this](QTabWidget* tw, const QString& key) {
        if (!tw || tw->property("consoleTabPersistBound").toBool())
            return;
        tw->setProperty("consoleTabPersistBound", true);
        connect(tw, &QTabWidget::currentChanged, this, [this, tw, key](int) {
            if (consoleUiRestoring)
                return;
            const QString name = consoleTabLogicalName(tw);
            if (!name.isEmpty())
                SETTINGS.setValue(key, name);
        });
    };
    bindPersistTab(ui->tabWidget_debug_production, QStringLiteral("Window/ConsoleTabProduction"));
    bindPersistTab(ui->tabWidget_debug_peripheral, QStringLiteral("Window/ConsoleTabPeripheral"));
    bindPersistTab(ui->tabWidget_debug_dongle, QStringLiteral("Window/ConsoleTabDongle"));
    bindPersistTab(ui->tabWidget_ota, QStringLiteral("Window/ConsoleTabOta"));
    bindPersistTab(ui->tabWidget_debug_misc, QStringLiteral("Window/ConsoleTabMisc"));
    bindPersistTab(ui->consoleLogTabs, QStringLiteral("Window/ConsoleLogTab"));

    updateMainStyle(QStringLiteral("console_shell.qss"));
    if (ui->consoleLogTabs) {
        ui->consoleLogTabs->setDocumentMode(true);
        if (QTabBar* bar = ui->consoleLogTabs->tabBar())
            bar->setDrawBase(false);
    }
    QTimer::singleShot(0, this, [this]() { updateConsoleNarrowLayout(); });
    const QList<QPlainTextEdit*> logEdits = {ui->log, ui->msgEdit};
    for (QPlainTextEdit* edit : logEdits) {
        if (!edit)
            continue;
        edit->setFrameShape(QFrame::NoFrame);
        edit->setLineWidth(0);
        edit->setAutoFillBackground(false);
        if (QWidget* vp = edit->viewport())
            vp->setAutoFillBackground(false);
        if (QTextFrame* root = edit->document()->rootFrame()) {
            QTextFrameFormat fmt = root->frameFormat();
            fmt.setBottomMargin(120);
            root->setFrameFormat(fmt);
        }
    }
    installConsoleSegmentedSwitches();
    applyConsoleBtnRoles(this);
    {
        const QList<QWidget*> switchRows = {consoleStatusMotor_, consoleStatusPump_, consoleStatusValve_};
        for (QWidget* row : switchRows) {
            if (!row)
                continue;
            row->setCursor(Qt::PointingHandCursor);
            row->setProperty("consoleSwitchOn", false);
            row->setToolTip(QStringLiteral("点击开关"));
            row->installEventFilter(this);
            const auto kids = row->findChildren<QWidget*>();
            for (QWidget* w : kids)
                w->installEventFilter(this);
        }
    }
    syncConsoleConnectSeg();
    updateConsoleConnectFlow();
    QTimer::singleShot(0, this, [this]() {
        syncConsoleConnectSeg();
        updateConsoleConnectFlow();
    });
}

'''
new_init = new_init.replace("\n", nl)
text = text[:init_start] + new_init + text[init_end:]

# ---- 4) Slim installPhysicalDevicePads card sizing ----
old_pads = '''void MainWindow::installPhysicalDevicePads() {
    if (!ui)
        return;

    // 连接页卡片均分
    if (ui->consoleConnectGrid) {
        ui->consoleConnectGrid->setColumnStretch(0, 1);
        ui->consoleConnectGrid->setColumnStretch(1, 1);
        ui->consoleConnectGrid->setRowStretch(0, 1);
        ui->consoleConnectGrid->setRowStretch(1, 1);
    }
    for (QFrame* card : {ui->consoleCardStep1, ui->consoleCardStep2, ui->consoleCardStep4}) {
        if (!card)
            continue;
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        card->setMinimumHeight(168);
        if (auto* lay = qobject_cast<QVBoxLayout*>(card->layout())) {
            if (!card->property("consoleCardSpaced").toBool()) {
                lay->addStretch(1);
                card->setProperty("consoleCardSpaced", true);
            }
        }
    }'''

new_pads = '''void MainWindow::installPhysicalDevicePads() {
    if (!ui)
        return;

    // 连接页卡片尺寸 / 网格 stretch 已在 .ui；这里只补 stretch 占位，避免内容贴底
    for (QFrame* card : {ui->consoleCardStep1, ui->consoleCardStep2, ui->consoleCardStep4}) {
        if (!card)
            continue;
        if (auto* lay = qobject_cast<QVBoxLayout*>(card->layout())) {
            if (!card->property("consoleCardSpaced").toBool()) {
                lay->addStretch(1);
                card->setProperty("consoleCardSpaced", true);
            }
        }
    }'''

if old_pads.replace("\n", nl) in text:
    text = text.replace(old_pads.replace("\n", nl), new_pads.replace("\n", nl), 1)
elif old_pads in text:
    text = text.replace(old_pads, new_pads, 1)
else:
    print("WARN: installPhysicalDevicePads block not found exactly")

# narrow layout still uses findChild consoleShellSplit - ui member also ok
text = text.replace(
    'auto* split = consoleShell_->findChild<QSplitter*>(QStringLiteral("consoleShellSplit"));',
    'auto* split = ui->consoleShellSplit;',
)

path.write_bytes(text.encode("utf-8"))
print("mainwindow.cpp updated")
print("makeConsoleStatusIcon left:", "makeConsoleStatusIcon" in text)
print("applyConsoleStatusIcon left:", "applyConsoleStatusIcon" in text)
print("hand-drawn help left:", "drawEllipse(QRectF(4, 4, 24, 24))" in text)
