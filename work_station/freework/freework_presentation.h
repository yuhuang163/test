#pragma once

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPointer>
#include <QTableWidget>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QtGlobal>
#include <QVBoxLayout>
#include "AbIni.h"
#include "ui_qfreework.h"

namespace FreeWorkPresentation {
inline QIcon icon(const char* name) {
    return QIcon(QStringLiteral(":/stytle/picture/fw/%1.png").arg(QLatin1String(name)));
}

// The overlay follows the model, including resets and asynchronous row insertion.
class EmptyTableHint : public QObject {
    QPointer<QAbstractItemModel> model_;
    QWidget* hint_;
public:
    explicit EmptyTableHint(QTableWidget* table) : QObject(table->viewport()), model_(table->model()), hint_(new QWidget(table->viewport())) {
        hint_->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* layout = new QVBoxLayout(hint_);
        layout->setSpacing(6);
        layout->addStretch();
        auto* picture = new QLabel(hint_);
        picture->setPixmap(QPixmap(QStringLiteral(":/stytle/picture/fw/icon-empty.png"))
                               .scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        picture->setAlignment(Qt::AlignCenter);
        layout->addWidget(picture);
        auto* title = new QLabel(QStringLiteral("暂无测试数据"), hint_);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet(QStringLiteral("color:#304b7e; font-size:14px; font-weight:600; border:0;"));
        layout->addWidget(title);
        auto* detail = new QLabel(QStringLiteral("请连接设备并开始测试"), hint_);
        detail->setAlignment(Qt::AlignCenter);
        detail->setStyleSheet(QStringLiteral("color:#58709a; font-size:12px; border:0;"));
        layout->addWidget(detail);
        layout->addStretch();
        table->viewport()->installEventFilter(this);
        connect(table->model(), &QAbstractItemModel::rowsInserted, this, [this]() { refresh(); });
        connect(table->model(), &QAbstractItemModel::rowsRemoved, this, [this]() { refresh(); });
        connect(table->model(), &QAbstractItemModel::modelReset, this, [this]() { refresh(); });
        refresh();
    }
    void refresh() {
        hint_->setGeometry(hint_->parentWidget()->rect());
        hint_->setVisible(model_ && model_->rowCount() == 0);
    }
    bool eventFilter(QObject* object, QEvent* event) override {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show)
            refresh();
        return QObject::eventFilter(object, event);
    }
};

inline QString cardIconName(const QString& role, bool mes) {
    if (mes)
        return QStringLiteral("icon-factory");
    if (role == QStringLiteral("busy"))
        return QStringLiteral("icon-play");
    if (role == QStringLiteral("ok"))
        return QStringLiteral("icon-flask");
    if (role == QStringLiteral("fail"))
        return QStringLiteral("icon-stop");
    return QStringLiteral("icon-hourglass");
}

inline QPixmap bleStatusPixmap(bool connected, int px = 20) {
    QPixmap src(QStringLiteral(":/stytle/picture/fw/icon-bluetooth.png"));
    if (src.isNull())
        return {};
    QImage img = src.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const int r = qRed(line[x]);
            const int g = qGreen(line[x]);
            const int b = qBlue(line[x]);
            const int a = qAlpha(line[x]);
            if (a == 0)
                continue;
            // 资源黑底改为透明，适配白底工位栏
            if (r < 40 && g < 40 && b < 40) {
                line[x] = qRgba(0, 0, 0, 0);
                continue;
            }
            if (!connected) {
                const int gray = (r + g + b) / 3;
                line[x] = qRgba(gray, gray, gray, a * 170 / 255);
            }
        }
    }
    return QPixmap::fromImage(img).scaled(px, px, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

/** 蓝牙连接状态：蓝图标=已连接，灰图标=未连接；悬停看文案。 */
inline void applyBleStatusIcon(QLabel* label, bool connected) {
    if (!label)
        return;
    label->clear();
    label->setPixmap(bleStatusPixmap(connected));
    label->setToolTip(connected ? QStringLiteral("蓝牙已连接") : QStringLiteral("蓝牙未连接"));
    label->setFixedSize(24, 26);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QStringLiteral("background:transparent;border:0;padding:0;margin:0;"));
}

// role 决定图标；正文须先 setText 为「标题\\n副标题」纯文本再调用。
inline void decorateCard(QLabel* label, const QString& role = QStringLiteral("wait")) {
    // 若已是上一次 decorate 的 HTML，再 split 会得到脏标题；调用方须先 setText 纯文本
    const QString raw = label->text();
    if (raw.contains(QLatin1String("<table"), Qt::CaseInsensitive))
        return;
    const QStringList lines = raw.split(QLatin1Char('\n'));
    const bool mes = label->objectName() == QStringLiteral("mes_state");
    // MES busy 黄底用深色字；PASS/#00FF00 同表格「通过」用黑字；其余深色底用白字
    QString fg = QStringLiteral("#ffffff");
    if (mes && role == QStringLiteral("busy"))
        fg = QStringLiteral("#422006");
    else if (role == QStringLiteral("ok"))
        fg = QStringLiteral("#000000");
    // 同色多处出现时勿用 %N（arg 只替一次），用占位符再替换
    QString html = QStringLiteral("<table width='100%' cellspacing='0'><tr>"
        "<td width='48'><img src=':/stytle/picture/fw/%1.png' width='32' height='32'></td>"
        "<td><span style='font-size:18px;font-weight:700;color:__FG__'>%2</span><br/>"
        "<span style='font-size:12px;font-weight:400;color:__FG__'>%3</span></td>"
        "<td width='16' style='font-size:14px;color:__FG__'>●</td></tr></table>");
    html = html.arg(cardIconName(role, mes), lines.value(0).toHtmlEscaped(), lines.value(1).toHtmlEscaped());
    html.replace(QLatin1String("__FG__"), fg);
    label->setTextFormat(Qt::RichText);
    label->setText(html);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

inline bool freeWorkMultiStation() {
    return qMax(1, SETTINGS.value(QStringLiteral("User/formColumn"), 1).toInt())
             * qMax(1, SETTINGS.value(QStringLiteral("User/formRow"), 1).toInt())
         > 1;
}

/** 一拖多：缩短文案 + SN/MAC 改上下排，避免单列最小宽度过大导致整窗缩不窄。 */
inline void applyMultiStationCompact(Ui::QFreeWork* ui) {
    if (!ui || !freeWorkMultiStation())
        return;

    ui->gridLayout_7->setContentsMargins(2, 2, 2, 2);
    ui->gridLayout_2->setContentsMargins(4, 4, 4, 4);
    ui->verticalLayout_5->setSpacing(4);
    ui->horizontalLayout_12->setSpacing(4);
    ui->horizontalLayout_3->setSpacing(4);
    ui->horizontalLayout_17->setSpacing(2);
    ui->horizontalLayout_17->setDirection(QBoxLayout::TopToBottom);
    ui->horizontalLayout_6->setSpacing(4);
    ui->horizontalLayout_19->setSpacing(4);

    const QList<QWidget*> shrinkList = {
        ui->isformmes,           ui->isusemes,         ui->autoStartCheckBox,
        ui->toggleExtraTabsButton, ui->bleStatusLabel, ui->label_5, ui->label_2,
        ui->pushButton,          ui->connectButton,    ui->disconnectButton,
        ui->comNameCombo,        ui->getMac,           ui->macInput,
        ui->mes_state,           ui->test_result,      ui->stopTest,
        ui->testResultTable,     ui->tabWidget,        ui->test_time,
        ui->label_bydMesResourceCaption, ui->label_bydMesResource,
    };
    for (QWidget* w : shrinkList) {
        if (w)
            w->setMinimumWidth(0);
    }
    ui->getMac->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->macInput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->comNameCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->test_time->setMaximumWidth(64);

    ui->isformmes->setText(QStringLiteral("取MAC"));
    ui->isformmes->setToolTip(QStringLiteral("是否从MES获取MAC"));
    ui->isusemes->setText(QStringLiteral("过站"));
    ui->isusemes->setToolTip(QStringLiteral("是否过站"));
    ui->autoStartCheckBox->setText(QStringLiteral("自动"));
    ui->autoStartCheckBox->setToolTip(QStringLiteral("启用自动触发"));
    ui->toggleExtraTabsButton->setText(QStringLiteral("扩展"));
    ui->toggleExtraTabsButton->setToolTip(QStringLiteral("显示/隐藏扩展页"));
    if (ui->label_5) {
        ui->label_5->setText(QStringLiteral("SN"));
        ui->label_5->setToolTip(QStringLiteral("输入SN获取MAC"));
    }
    if (ui->label_2) {
        ui->label_2->setText(QStringLiteral("MAC"));
        ui->label_2->setToolTip(QStringLiteral("输入MAC连接"));
    }
    ui->pushButton->setText(QStringLiteral("测试"));
    ui->pushButton->setToolTip(QStringLiteral("测试(开发用)"));
    ui->connectButton->setText(QStringLiteral("连接"));
    ui->disconnectButton->setText(QStringLiteral("断开"));
    ui->testResultTable->setMinimumHeight(120);

    // 串口行：只让 COM 拉宽，按钮按内容收缩
    if (ui->horizontalLayout_6) {
        for (int i = 0; i < ui->horizontalLayout_6->count(); ++i) {
            QWidget* w = ui->horizontalLayout_6->itemAt(i)->widget();
            ui->horizontalLayout_6->setStretch(i, (w == ui->comNameCombo) ? 1 : 0);
        }
    }
}

inline void setup(Ui::QFreeWork* ui) {
    ui->gridLayout_7->setContentsMargins(12, 8, 12, 8);
    ui->gridLayout_2->setContentsMargins(8, 8, 8, 8);
    ui->verticalLayout_5->setSpacing(8);
    ui->verticalLayout_5->setStretch(1, 1);
    ui->horizontalLayout_12->setSpacing(10);
    ui->horizontalLayout_12->setStretch(0, 0);
    ui->horizontalLayout_12->setStretch(1, 1);
    ui->horizontalLayout_3->setSpacing(8);
    ui->horizontalLayout_3->setStretch(0, 1);
    ui->horizontalLayout_3->setStretch(1, 1);
    ui->horizontalLayout_17->setSpacing(12);
    ui->horizontalLayout_17->setStretch(0, 1);
    ui->horizontalLayout_17->setStretch(1, 1);
    for (QHBoxLayout* row : {ui->horizontalLayout_6, ui->horizontalLayout_productInst}) {
        row->setSpacing(8);
        for (int i = 0; i < row->count(); ++i)
            row->setStretch(i, i == 0 ? 3 : 4);
    }
    for (QLineEdit* edit : {ui->getMac, ui->macInput}) {
        QAction* copy = edit->addAction(icon("icon-copy"), QLineEdit::TrailingPosition);
        copy->setToolTip(QStringLiteral("复制"));
        QObject::connect(copy, &QAction::triggered, edit, [edit]() {
            QApplication::clipboard()->setText(edit->text());
        });
    }
    ui->pushButton->setIcon(icon("icon-gear"));
    ui->stopTest->setIcon(icon("icon-stop"));
    ui->stopTest->setIconSize(QSize(16, 16));
    for (QPushButton* button : {ui->connectButton, ui->productConnectButton}) {
        button->setIcon(icon("icon-link"));
        button->setIconSize(QSize(16, 16));
    }
    ui->pushButton->setIconSize(QSize(16, 16));
    ui->label_productInst->setText(QStringLiteral("<img src=':/stytle/picture/fw/icon-display.png' width='16' height='16'/> &nbsp; 产品串口(仪器)"));
    // 用时：图标与文字拆开，避免 QLabel 富文本 img+字垂直错位
    ui->test_time_caption->setText(QStringLiteral("用时"));
    ui->test_time_caption->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    ui->test_time->setAlignment(Qt::AlignCenter);
    ui->bleStatusLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    applyBleStatusIcon(ui->bleStatusLabel, false);
    if (ui->horizontalLayout_12) {
        const int captionIdx = ui->horizontalLayout_12->indexOf(ui->test_time_caption);
        if (captionIdx >= 0 && !ui->tab->findChild<QLabel*>(QStringLiteral("test_time_icon"))) {
            auto* iconLab = new QLabel(ui->tab);
            iconLab->setObjectName(QStringLiteral("test_time_icon"));
            iconLab->setPixmap(QPixmap(QStringLiteral(":/stytle/picture/fw/icon-timer.png"))
                                   .scaled(14, 14, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            iconLab->setFixedSize(16, 26);
            iconLab->setAlignment(Qt::AlignCenter);
            iconLab->setStyleSheet(QStringLiteral("background:transparent;border:0;padding:0;margin:0;"));
            ui->horizontalLayout_12->insertWidget(captionIdx, iconLab);
        }
        ui->horizontalLayout_12->setAlignment(ui->test_time_caption, Qt::AlignVCenter);
        ui->horizontalLayout_12->setAlignment(ui->test_time, Qt::AlignVCenter);
        ui->horizontalLayout_12->setAlignment(ui->bleStatusLabel, Qt::AlignVCenter);
        ui->horizontalLayout_12->setAlignment(ui->toggleExtraTabsButton, Qt::AlignVCenter);
    }
    ui->testResultTable->setMinimumHeight(160);
    ui->testResultTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->testResultTable->horizontalHeader()->setMinimumHeight(28);
    applyMultiStationCompact(ui);
    new EmptyTableHint(ui->testResultTable);
}
} // namespace FreeWorkPresentation
