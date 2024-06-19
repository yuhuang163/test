#ifndef IMAGEWINDOW_H
#define IMAGEWINDOW_H

#include <QWidget>
#include <QLabel>

namespace Ui {
class ImageWindow;
}

class ImageWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ImageWindow(QWidget *parent = nullptr);

    ~ImageWindow();


private:
    Ui::ImageWindow *ui;
    QLabel *imageLabel;
};

#endif // IMAGEWINDOW_H
