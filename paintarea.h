#ifndef PAINTAREA_H
#define PAINTAREA_H

#include <QWidget>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QEvent>
#include <QDebug>
#include <QMimeData>
#include <QFileDialog>
#include <memory>
#include <vector>
#include "paintobject.h"

class PaintArea : public QWidget
{
    Q_OBJECT
public:
    explicit PaintArea(QWidget *parent = nullptr, int _width=1024, int _height=1024);
    void load_img(const QString&);
    void unload_img();
    void save_pix(const QString&);
    void detectWalls();  // 墙体检测函数
    void clearWalls();   // 清除检测结果

    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void paintEvent(QPaintEvent*) override;
    QRect calc_rect();
    void wallDetection(const QImage& inputImage);
    QVector<QPoint> traceWallContours(const QImage& edgeImage);
    QVector<QVector<QPoint>> findContinuousWalls(const QVector<QPoint>& contourPoints);

public:
    PaintObject paint_target;
    QString paint_lay_obj_name;
    std::shared_ptr<BaseRectObject> preview;

private:
    int width;
    int height;
    QPoint global_grab_origin;
    std::unique_ptr<QPixmap> bg_pix;
    std::unique_ptr<QPixmap> pix;
    std::pair<QPoint, QPoint> point_pair;
    bool output_flag;

    // 墙体检测相关
    QVector<QVector<QPoint>> wall_contours;  // 检测到的墙体轮廓（折线）
    bool walls_detected;


signals:
    void paint_finished();
    void walls_detected_signal(int wall_count);
};

#endif // PAINTAREA_H
