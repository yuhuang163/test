#ifndef FACTORY_ANALYZER_H
#define FACTORY_ANALYZER_H

#include <QMainWindow>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDesktopServices>
#include <QUrl>

QT_BEGIN_NAMESPACE
namespace Ui {
class factory_analyzer;
}
QT_END_NAMESPACE

class factory_analyzer : public QMainWindow
{
    Q_OBJECT

public:
    factory_analyzer(QWidget *parent = nullptr);
    ~factory_analyzer();

        void showlog(QString msg);
QStringList processOutputLines;
private slots:
    void on_pushButton_clicked();

    void on_pushButton_2_clicked();

    void on_pushButton_3_clicked();

    void pushFileToGaoTongDevice(const QString &localFile);
       void pushFileToZiYanDevice(const QString &localFile);

    void on_pushButton_4_clicked();

       void on_pushButton_5_clicked();

    void on_pushButton_6_clicked();

protected:
    void dragEnterEvent(QDragEnterEvent *event)override;
    // 放下事件
    void dropEvent(QDropEvent *event)override;

private:
    Ui::factory_analyzer *ui;
};
#endif // FACTORY_ANALYZER_H
