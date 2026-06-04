#ifndef DRAGGABLECHECKBOX_H
#define DRAGGABLECHECKBOX_H

#include <QtWidgets/QCheckBox>
#include <QtGui/QDrag>
#include <QtCore/QMimeData>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

class DraggableCheckBox : public QCheckBox
{
    Q_OBJECT

public:
    DraggableCheckBox(const QString& text, int index, QWidget* parent = nullptr);

    int getIndex() const;
    void setIndex(int newIndex);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void performDrag();

    QPoint startPos;
    int index;
};


#endif // DRAGGABLECHECKBOX_H
