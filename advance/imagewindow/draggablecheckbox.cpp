#include "draggablecheckbox.h"
#include "qapplication.h"

DraggableCheckBox::DraggableCheckBox(const QString& text, int index, QWidget* parent)
    : QCheckBox(text, parent), index(index) // 构造函数，初始化文本和索引
{
    // 设置样式表
    setStyleSheet("border-width: 2px; border-style: solid; border-color: rgb(255, 165, 0);");
    setAcceptDrops(true); // 允许接收拖放操作
}


int DraggableCheckBox::getIndex() const {
    return index; // 获取复选框的索引
}

void DraggableCheckBox::setIndex(int newIndex) {
    index = newIndex; // 设置复选框的索引
}

void DraggableCheckBox::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) { // 检查是否为左键按下
        startPos = event->pos(); // 记录起始位置
    }
    QCheckBox::mousePressEvent(event); // 调用基类的鼠标按下事件处理
}

void DraggableCheckBox::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) { // 检查是否按住左键
        int distance = (event->pos() - startPos).manhattanLength(); // 计算拖动距离
        if (distance >= QApplication::startDragDistance()) { // 如果拖动距离足够
            performDrag(); // 执行拖动操作
        }
    }
}

void DraggableCheckBox::performDrag() {
    QMimeData* mimeData = new QMimeData; // 创建MIME数据对象
    mimeData->setText(QString::number(index)); // 设置复选框的索引为拖放数据

    QDrag* drag = new QDrag(this); // 创建拖动对象
    drag->setMimeData(mimeData); // 设置拖动的数据
    drag->setHotSpot(startPos); // 设置拖动的热点
    drag->exec(Qt::MoveAction); // 执行拖动操作
}



