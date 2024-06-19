#include "imagewindow.h"
#include "AbIni.h"
#include "ui_imagewindow.h"
#include <QDebug>
#include <QMessageBox>

ImageWindow::ImageWindow(QWidget *parent)
    : QWidget(parent), ui(new Ui::ImageWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);  // 添加这行代码

}
ImageWindow::~ImageWindow()
{
    delete ui;
}

