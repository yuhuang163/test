#include "MyOpenGLWidget.h"
#include <QOpenGLShader>
#include <QOpenGLShaderProgram>
#include <QVector3D>
#include <QMatrix4x4>

MyOpenGLWidget::MyOpenGLWidget(QWidget* parent)
    : QOpenGLWidget(parent),
      program(nullptr) {
}

MyOpenGLWidget::~MyOpenGLWidget() {
    makeCurrent();  // 确保当前OpenGL上下文可用
    vbo.destroy();  // 销毁VBO
    vao.destroy();  // 销毁VAO
    delete program; // 删除着色器程序
}

void MyOpenGLWidget::initializeGL() {
    initializeOpenGLFunctions(); // 初始化OpenGL函数

    // 创建并编译着色器程序
    program = new QOpenGLShaderProgram(this);
    program->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/vertex_shader.glsl");
    program->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/fragment_shader.glsl");
    program->link();
    program->bind();

    // 定义矩形的顶点
    GLfloat vertices[] = {
        // 顶点坐标
        -0.5f, -0.5f, -0.5f, // 前面左下
        0.5f, -0.5f, -0.5f,  // 前面右下
        0.5f, 0.5f, -0.5f,   // 前面右上
        -0.5f, 0.5f, -0.5f,  // 前面左上

        -0.5f, -0.5f, 0.5f, // 后面左下
        0.5f, -0.5f, 0.5f,  // 后面右下
        0.5f, 0.5f, 0.5f,   // 后面右上
        -0.5f, 0.5f, 0.5f   // 后面左上
    };

    GLuint indices[] = {
        0, 1, 2, 0, 2, 3, // 前面
        4, 5, 6, 4, 6, 7, // 后面
        0, 1, 5, 0, 5, 4, // 底面
        2, 3, 7, 2, 7, 6, // 顶面
        0, 3, 7, 0, 7, 4, // 左面
        1, 2, 6, 1, 6, 5  // 右面
    };

    // 创建顶点缓冲区对象（VBO）和顶点数组对象（VAO）
    vao.create();
    vao.bind();
    vbo.create();
    vbo.bind();
    vbo.allocate(vertices, sizeof(vertices));

    // 配置顶点属性指针
    program->enableAttributeArray(0);
    program->setAttributeBuffer(0, GL_FLOAT, 0, 3, 3 * sizeof(GLfloat));

    // 创建元素缓冲区对象（EBO）
    GLuint ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    vao.release();
}

void MyOpenGLWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h); // 设置视口大小

    // 更新投影矩阵
    QMatrix4x4 projection;
    projection.perspective(45.0f, float(w) / float(h), 0.1f, 100.0f);
    program->setUniformValue("projection", projection);
}

void MyOpenGLWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // 清空颜色和深度缓冲区
    glEnable(GL_DEPTH_TEST);                            // 开启深度测试

    // 计算模型矩阵和视图矩阵
    QMatrix4x4 model;
    model.rotate(45.0f, 1.0f, 0.0f, 0.0f); // 绕X轴旋转

    QMatrix4x4 view;
    view.translate(0.0f, 0.0f, -3.0f); // 视图变换，调整摄像机位置

    // 传递矩阵给着色器
    program->setUniformValue("model", model);
    program->setUniformValue("view", view);

    // 绘制矩形
    program->bind();
    vao.bind();
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr); // 绘制36个三角形
    vao.release();
    program->release();
}
