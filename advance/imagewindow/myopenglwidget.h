#ifndef MYOPENGLWIDGET_H
#define MYOPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>

class MyOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

  public:
    MyOpenGLWidget(QWidget* parent = nullptr);
    ~MyOpenGLWidget();

  protected:
    void initializeGL() override;         // 初始化OpenGL
    void resizeGL(int w, int h) override; // 调整视口
    void paintGL() override;              // 绘制内容

  private:
    QOpenGLShaderProgram* program; // 着色器程序
    QOpenGLBuffer vbo;             // 顶点缓冲区对象
    QOpenGLBuffer vao;             // 顶点数组对象
};

#endif // MYOPENGLWIDGET_H
