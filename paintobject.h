#ifndef PAINTOBJECT_H
#define PAINTOBJECT_H

#include <QWidget>

enum PaintObject
{
    NONE = 0,
    OUTLINE,
    LAY_OBJECT
};


class BaseObject: public QWidget
{
public:
    explicit BaseObject(QWidget* parent=nullptr): QWidget(parent) {}
};


class OutlineWall: public BaseObject
{
public:
    OutlineWall(QWidget* parent): BaseObject(parent){}
    void paint(QPainter&);
    void backspace()
    {
        if (points.size() > 0) points.pop_back();
    }
    void align(int delta, int angle_delta);

    // 用YOLO识别结果替换手动绘制的轮廓
    void setYOLOContours(const QVector<QVector<QPoint>>& contours);

    // 清除所有轮廓
    void clearContours() {
        points.clear();
        completed = false;
        update();
    }

    // 获取轮廓顶点
    std::vector<QPointF> getContourPoints() const {
        return points;
    }

    // 检查是否有轮廓数据
    bool hasContours() const {
        return !points.empty();
    }

protected:
    void paintEvent(QPaintEvent *event) override;

public:
    bool completed = false;
    std::vector<QPointF> points;
    std::shared_ptr<QPointF> preview;
};


class BaseRectObject: public BaseObject
{
public:
    BaseRectObject(QWidget* parent, const QRect& rect=QRect());
    void setSizeF(float w, float h) {sizeF.setWidth(w); sizeF.setHeight(h); resize(w, h);}
    void setSizeF(QSize size) {sizeF.setWidth(size.width()); sizeF.setHeight(size.height()); resize(size);}
    void scale(float ratio);
    bool eventFilter(QObject *watched, QEvent *event) override;

public:
    bool is_selected = false;

protected:
    QPoint grab_start;
    QSizeF sizeF;           //只在缩放时使用,防止因精度损失变形
};


class LayObject: public BaseRectObject
{
public:
    LayObject(QWidget* parent, const QRect& rect, QString type_name);
    LayObject(const LayObject &other): LayObject((QWidget*)other.parent(), other.geometry(), other.type_name){}

protected:
    void paintEvent(QPaintEvent *event) override;

public:
    QString type_name;
};




#endif // PAINTOBJECT_H
