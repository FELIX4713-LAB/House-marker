#include "paintobject.h"
#include "config.h"
#include "house.h"
#include <QPainter>
#include <QEvent>
#include <QMouseEvent>

#define PI 3.14159265358979323846

extern std::map<LayObject*, std::shared_ptr<LayObject>> obj_ptr_map;
extern std::map<std::shared_ptr<LayObject>, std::shared_ptr<BaseNode>> obj2node;

void OutlineWall::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    paint(painter);
}

void OutlineWall::paint(QPainter& painter)
{
    painter.setRenderHint(QPainter::Antialiasing);
    if (!completed)
    {
        painter.setPen(QPen(QColor("black")));
        for (int i = 1; i < points.size(); ++i)
            painter.drawLine(points[i-1], points[i]);
    }
    else
    {
        painter.setBrush(Config::getInstance().bg_color);
        QPolygon polygon;
        for (const QPointF& p: points)
        {
            polygon.push_back(p.toPoint());
        }
        painter.drawPolygon(polygon);
    }
}

double distance(QPointF a, QPointF b)
{
    return sqrt(pow(a.rx()-b.rx(), 2) + pow(a.ry()-b.ry(), 2));
}

double calc_angle(QPointF a, QPointF b)
{
    double dx = b.rx() - a.rx();
    if (abs(dx) < 1e-5)
        return 90;
    else
    {
        double dy = b.ry() - a.ry();
        double radian = std::atan2(dy, dx);
        double degree = radian * (180 / PI);
        return degree;
    }
}

void OutlineWall::align(int delta, int angle_delta)
{
    if (distance(points.front(), points.back()) > 1e-5)
        points.push_back(points.front());

    for (int i = points.size()-2; i > 0; --i)
    {
        if (distance(points[i], points[i+1]) < delta)
        {
            points.erase(points.begin() + i);
        }
    }
    for (int i = 1; i < points.size(); ++i)
    {
        double angle = abs(calc_angle(points[i-1], points[i]));
        if (abs(angle) < angle_delta || abs(angle - 180) < angle_delta)
        {
            points[i].setY(points[i-1].ry());
        }
        else if (abs(angle - 90) < angle_delta)
        {
            points[i].setX(points[i-1].rx());
        }
    }
    points.front() = points.back();
    update();
}


BaseRectObject::BaseRectObject(QWidget *parent, const QRect &rect): BaseObject(parent)
{
    installEventFilter(this);
    setGeometry(rect);
    sizeF = rect.size();
}

bool BaseRectObject::eventFilter(QObject *watched, QEvent *event)
{
    if (House::getInstance().mode == OperateMode::DRAG)
    {
        return QObject::eventFilter(watched, event);
    }

    //左键点击选取
    if (watched == this && event->type() == QEvent::MouseButtonPress && static_cast<QMouseEvent*>(event)->buttons()&Qt::LeftButton)
    {
        if (House::getInstance().mode == OperateMode::ADD_SELECT && !is_selected)
        {
            is_selected = true;
        }
        else if (!is_selected)
        {
            GraphArea::getInstance().clear_multi_select();
        }

        setFocus();
        grab_start = static_cast<QMouseEvent*>(event)->pos();
        House::getInstance().object_status_change(this);           //自定义QWidget显示与Q_OBJECT冲突,所以不用信号传递
        if (auto obj = dynamic_cast<LayObject*>(this))
        {
            is_selected = true;
            GraphArea::getInstance().selected_node = obj2node[obj_ptr_map[obj]];
            GraphArea::getInstance().graph_update();
        }
        return true;
    }
    //实现移动时拖拽
    else if (event->type() == QEvent::MouseMove)
    {
        QPoint delta = static_cast<QMouseEvent*>(event)->pos() - grab_start;
        House::getInstance().multi_move(delta);
        House::getInstance().object_status_change(this);
        return true;
    }
    //鼠标释放,退出拖拽状态
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        House::getInstance().object_status_change(this);
        return true;
    }
    //滚轮调整大小
    else if (watched == this && event->type() == QEvent::Wheel)
    {
        int delta = static_cast<QWheelEvent*>(event)->angleDelta().y();
        if (delta > 0) {
            sizeF = sizeF * Config::getInstance().scale_ratio;
        } else if (delta < 0) {
            sizeF = sizeF / Config::getInstance().scale_ratio;
        }
        this->resize(sizeF.toSize());
        House::getInstance().object_status_change(this);
        return true;
    }
    //下层控件需要检测键盘事件,不能返回true
    else if (watched == this && event->type() == QEvent::KeyPress)
    {
        int key = static_cast<QKeyEvent*>(event)->key();
        if (key == Qt::Key_Space) {
            this->resize(height(), width());
        }
        House::getInstance().object_status_change(this);
    }

    return QObject::eventFilter(watched, event);
}

void BaseRectObject::scale(float ratio)
{
    sizeF *= ratio;
    resize(sizeF.toSize());
}


LayObject::LayObject(QWidget* parent, const QRect& rect, QString type_name): BaseRectObject(parent, rect), type_name(type_name)
{
    setGeometry(rect);
    setStyleSheet(Config::getInstance().obj_info[type_name]["style_sheet"]);
}

void LayObject::paintEvent(QPaintEvent *event)
{
    QString style_sheet = styleSheet();
    if (is_selected)
    {
        style_sheet = style_sheet.replace("none", QString("%1px dashed black").arg(Config::getInstance().paint_area_selected_line_width));
    }
    else
        style_sheet = Config::getInstance().obj_info[type_name]["style_sheet"];

    this->setStyleSheet(style_sheet);
}


