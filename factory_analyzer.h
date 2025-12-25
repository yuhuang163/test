#ifndef FACTORY_ANALYZER_H
#define FACTORY_ANALYZER_H

#include "qadb.h"
#include "qbulk/qbulk.h"
#include "qcustomplot.h"
#include "qshell.h"
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMainWindow>
#include <QUrl>

QT_BEGIN_NAMESPACE
namespace Ui {
class factory_analyzer;
}
QT_END_NAMESPACE

class factory_analyzer : public QMainWindow {
    Q_OBJECT

public:
    factory_analyzer(QWidget *parent = nullptr);
    ~factory_analyzer();
    QBulk *bulk = nullptr;
    void showlog(const QString &msg);
    QStringList processOutputLines;
    std::vector<QCustomPlot *> graph_value_vector;
    void graph_reset(uint8_t argument);

    // 作为类成员
    QTableWidget *table;
    QLabel *adbStatusLabel = nullptr;
    QLabel *usbStatusLabel = nullptr;

    QString adbLastOutput;
    bool ddr_press = false;
    void runCmd(const QString &cmd);
    Qadb *adb;
    Qshell *shell;

    Qshell *shellMonitor;
    QStandardItemModel *treeModel;
    QStandardItemModel *fileModel;

    QQueue<QString> commandQueue; // 命令队列
    bool isProcessing = false;    // 是否正在执行命令

    QStringList commandHistory;
    int historyIndex = -1;
    // 类成员
    bool treeExpanded = true;  // 当前状态，false=收起，true=展开
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
private slots:

    void updateForwardTable();
    void parseAndAddLine(const QString &line);
    void execAdb(const QString &args,
                                   std::function<void(const QString &output, qint64 elapsed)> callback,
                 int timeout = 3000);
    void on_pushButton_clicked();
QString execAdbBlocking(const QString &args, int timeout);
    void on_pushButton_2_clicked();

    void on_pushButton_3_clicked();

    void pushFileToGaoTongDevice(const QString &localFile);
    void pushFileToZiYanDevice(const QString &localFile);

    void on_pushButton_4_clicked();

    void on_pushButton_5_clicked();

    void on_pushButton_6_clicked();
    void runExeWithReport(const QString &exeName);

    void runProcess(const QString &program, const QStringList &arguments,
                    const QString &workDir,
                    std::function<void(int, QProcess::ExitStatus)> onFinish);

    void on_pushButton_7_clicked();

    void on_pushButton_8_clicked();

    void on_pushButton_9_clicked();
    void updateAdbStatus();

    void on_pushButton_10_clicked();

    void on_pushButton_11_clicked();

    void on_pushButton_12_clicked();

    void loadRoot();
    void loadFolder(const QString &path);

    void parseFiles(QString path, QString data);

    void on_treeView_clicked(const QModelIndex &index);

    void on_pushButton_13_clicked();
    void onTableViewContextMenu(const QPoint &pos);
    void onTreeViewContextMenu(const QPoint &pos);
    void on_pushButton_14_clicked();

    void on_pushButton_15_clicked();
    void displayCmdline(QTableWidget *table, const QString &cmdline);
    void on_pushButton_16_clicked();
    void updateQualcommComStatus();
    void on_tableView_doubleClicked(const QModelIndex &index);

    void on_pushButton_17_clicked();
    void updateBatteryLevel();

    void on_pushButton_18_clicked();

    void on_lineEdit_returnPressed();

    void on_pushButton_19_clicked();

    void on_pushButton_20_clicked();

    void on_pushButton_21_clicked();

    void on_pushButton_22_clicked();

    void on_pushButton_23_clicked();

    void on_pushButton_24_clicked();

private:
    void adbPull(const QString &remotePath);
    void adbDelete(const QString &remotePath);
    void refreshTreeAfterDelete(const QString &remotePath);
    void loadRemoteDirectory(const QString &path);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    // 放下事件
    void dropEvent(QDropEvent *event) override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::factory_analyzer *ui;
};
#endif // FACTORY_ANALYZER_H
