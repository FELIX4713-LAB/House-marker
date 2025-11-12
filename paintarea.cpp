#include "paintarea.h"
#include "config.h"
#include "house.h"
#include <QtMath>
#include <QVector>
#include <queue>
#include <functional>

// 全局变量
std::vector<std::pair<QPoint, QPoint>> align_lines;

PaintArea::PaintArea(QWidget *parent, int _width, int _height)
    : QWidget(parent), width(_width), height(_height), output_flag(false),
    pix(std::make_unique<QPixmap>(width, height)),
    preview(std::make_shared<BaseRectObject>(this)),
    paint_target(PaintObject::NONE),
    walls_detected(false)
{
    preview->hide();

    int display_width = std::min(width, 2048);
    int display_height = std::min(height, 2048);

    this->setGeometry(0, 0, display_width, display_height);
    House::getInstance().outline = std::make_shared<OutlineWall>(this);
    House::getInstance().outline->setGeometry(0, 0, display_width, display_height);
    House::getInstance().paint_area = this;
    installEventFilter(this);
}

void PaintArea::load_img(const QString& path)
{
    if (path != "")
    {
        QPixmap tmp = QPixmap(width, height);
        if (!tmp.load(path)) {
            qDebug() << "Failed to load image:" << path;
            return;
        }

        tmp = tmp.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        bg_pix = std::make_unique<QPixmap>(width, height);
        bg_pix->fill(Qt::transparent);
        QPainter painter(bg_pix.get());
        painter.setOpacity(Config::getInstance().bg_transpacence);
        painter.drawPixmap(0, 0, tmp);
        painter.end();

        // 加载图片后自动进行墙体检测
        detectWalls();

        update();
    }
}

void PaintArea::unload_img()
{
    bg_pix.reset();
    clearWalls();
    update();
}

void PaintArea::save_pix(const QString& path)
{
    output_flag = true;
    update();

    connect(this, &PaintArea::paint_finished, [=]() {
        if (path != "")
        {
            if (Config::getInstance().paint_area_size <= 2048) {
                pix->save(path);
            } else {
                QPixmap scaled_pix = pix->scaled(2048, 2048, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                scaled_pix.save(path);
            }
        }
        output_flag = false;
        update();
    });
}

void PaintArea::detectWalls()
{
    if (!bg_pix || bg_pix->isNull()) {
        qDebug() << "No image loaded for wall detection";
        return;
    }

    QImage originalImage = bg_pix->toImage();
    wallDetection(originalImage);
}

void PaintArea::clearWalls()
{
    wall_contours.clear();
    walls_detected = false;
    update();
}

QVector<QPoint> PaintArea::traceWallContours(const QImage& edgeImage)
{
    QVector<QPoint> contourPoints;
    int width = edgeImage.width();
    int height = edgeImage.height();

    // 使用区域生长法追踪轮廓
    QImage visited(width, height, QImage::Format_Grayscale8);
    visited.fill(0);

    // 8个方向的邻域
    int dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            // 如果当前点是边缘点且未被访问
            if (qRed(edgeImage.pixel(x, y)) > 128 && qRed(visited.pixel(x, y)) == 0) {
                std::queue<QPoint> queue;
                queue.push(QPoint(x, y));
                visited.setPixel(x, y, qRgb(255, 255, 255));

                while (!queue.empty()) {
                    QPoint current = queue.front();
                    queue.pop();
                    contourPoints.append(current);

                    // 检查8邻域
                    for (int i = 0; i < 8; ++i) {
                        int nx = current.x() + dx[i];
                        int ny = current.y() + dy[i];

                        if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                            if (qRed(edgeImage.pixel(nx, ny)) > 128 &&
                                qRed(visited.pixel(nx, ny)) == 0) {
                                visited.setPixel(nx, ny, qRgb(255, 255, 255));
                                queue.push(QPoint(nx, ny));
                            }
                        }
                    }
                }
            }
        }
    }

    return contourPoints;
}

QVector<QVector<QPoint>> PaintArea::findContinuousWalls(const QVector<QPoint>& contourPoints)
{
    QVector<QVector<QPoint>> walls;
    if (contourPoints.isEmpty()) return walls;

    // 将lambda函数显式声明为std::function
    std::function<QVector<QPoint>(const QVector<QPoint>&, double)> douglasPeucker;
    douglasPeucker = [&douglasPeucker](const QVector<QPoint>& points, double epsilon) -> QVector<QPoint> {
        if (points.size() <= 2) return points;

        // 找到离首尾连线最远的点
        double maxDistance = 0;
        int index = 0;
        QPoint p1 = points.first();
        QPoint p2 = points.last();

        for (int i = 1; i < points.size() - 1; ++i) {
            QPoint p = points[i];
            double distance = std::abs((p2.y() - p1.y()) * p.x() -
                                       (p2.x() - p1.x()) * p.y() +
                                       p2.x() * p1.y() - p2.y() * p1.x()) /
                              std::sqrt(std::pow(p2.y() - p1.y(), 2) + std::pow(p2.x() - p1.x(), 2));

            if (distance > maxDistance) {
                maxDistance = distance;
                index = i;
            }
        }

        // 如果最大距离大于阈值，递归处理
        if (maxDistance > epsilon) {
            QVector<QPoint> result1 = douglasPeucker(points.mid(0, index + 1), epsilon);
            QVector<QPoint> result2 = douglasPeucker(points.mid(index), epsilon);

            result1.removeLast(); // 避免重复点
            return result1 + result2;
        } else {
            return {points.first(), points.last()};
        }
    };

    // 将轮廓点分组为连续的墙体段
    QVector<QPoint> currentWall;
    const int maxGap = 5; // 最大允许的断点距离

    for (int i = 0; i < contourPoints.size(); ++i) {
        if (currentWall.isEmpty()) {
            currentWall.append(contourPoints[i]);
        } else {
            QPoint lastPoint = currentWall.last();
            QPoint currentPoint = contourPoints[i];

            // 计算两点之间的距离
            double distance = std::sqrt(std::pow(currentPoint.x() - lastPoint.x(), 2) +
                                        std::pow(currentPoint.y() - lastPoint.y(), 2));

            if (distance <= maxGap) {
                currentWall.append(currentPoint);
            } else {
                // 当前段结束，开始新的一段
                if (currentWall.size() > 10) { // 只保留足够长的墙体段
                    QVector<QPoint> simplified = douglasPeucker(currentWall, 2.0);
                    walls.append(simplified);
                }
                currentWall.clear();
                currentWall.append(currentPoint);
            }
        }
    }

    // 添加最后一段
    if (currentWall.size() > 10) {
        QVector<QPoint> simplified = douglasPeucker(currentWall, 2.0);
        walls.append(simplified);
    }

    return walls;
}

void PaintArea::wallDetection(const QImage& inputImage)
{
    wall_contours.clear();

    if (inputImage.isNull()) return;

    // 转换为灰度图像
    QImage grayImage = inputImage.convertToFormat(QImage::Format_Grayscale8);
    int width = grayImage.width();
    int height = grayImage.height();

    // 步骤1: 简单的二值化处理（针对户型图的特性）
    QImage binaryImage(width, height, QImage::Format_Grayscale8);

    // 计算图像的平均亮度
    long totalBrightness = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            totalBrightness += qRed(grayImage.pixel(x, y));
        }
    }
    int averageBrightness = totalBrightness / (width * height);

    // 基于平均亮度进行二值化
    int wallThreshold = averageBrightness /*- 15*/; // 墙体通常较暗
    if (wallThreshold < 50) wallThreshold = 50; // 确保阈值不太低

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int brightness = qRed(grayImage.pixel(x, y));
            if (brightness < wallThreshold) {
                binaryImage.setPixel(x, y, qRgb(255, 255, 255)); // 墙体为白色
            } else {
                binaryImage.setPixel(x, y, qRgb(0, 0, 0)); // 背景为黑色
            }
        }
    }

    // 步骤2: 形态学操作（去除噪声）
    QImage morphImage = binaryImage;

    // 简单的腐蚀操作
    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            if (qRed(binaryImage.pixel(x, y)) == 255) {
                // 检查3x3邻域，如果周围有背景点，则腐蚀当前点
                bool hasBackground = false;
                for (int i = -1; i <= 1; ++i) {
                    for (int j = -1; j <= 1; ++j) {
                        if (qRed(binaryImage.pixel(x+j, y+i)) == 0) {
                            hasBackground = true;
                            break;
                        }
                    }
                    if (hasBackground) break;
                }
                if (hasBackground) {
                    morphImage.setPixel(x, y, qRgb(0, 0, 0));
                }
            }
        }
    }

    // 步骤3: 追踪轮廓并找到连续墙体
    QVector<QPoint> contourPoints = traceWallContours(morphImage);
    wall_contours = findContinuousWalls(contourPoints);

    walls_detected = true;
    qDebug() << "Wall detection completed. Found" << wall_contours.size() << "wall segments";
    emit walls_detected_signal(wall_contours.size());

    update();
}

bool PaintArea::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == this && event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent* mouse_ev = static_cast<QMouseEvent*>(event);
        if (mouse_ev->buttons()&Qt::LeftButton)
        {
            GraphArea::getInstance().clear_multi_select();
            if (House::getInstance().mode == OperateMode::DRAG)
            {
                global_grab_origin = mouse_ev->pos();
            }
            else
            {
                QPoint mouse_pos = mouse_ev->pos();
                point_pair = {mouse_pos, mouse_pos};
                preview->setGeometry(calc_rect());
                preview->raise();
                setFocus();
                switch (paint_target)
                {
                case PaintObject::NONE:
                    preview->setStyleSheet(Config::getInstance().select_rect_styleSheet);
                    preview->show();
                    break;
                case PaintObject::OUTLINE:
                    House::getInstance().update_preview(mouse_pos);
                    break;
                case PaintObject::LAY_OBJECT:
                    preview->setStyleSheet(Config::getInstance().obj_info[paint_lay_obj_name]["style_sheet"]);
                    preview->show();
                    break;
                }
            }
            return true;
        }
        else if (mouse_ev->buttons()&Qt::RightButton)
        {
            House::getInstance().seal();
            return true;
        }
    }
    else if (watched == this && event->type() == QEvent::MouseMove)
    {
        QMouseEvent* mouse_ev = static_cast<QMouseEvent*>(event);
        if (mouse_ev->buttons() & Qt::LeftButton)
        {
            if (House::getInstance().mode == OperateMode::DRAG)
            {
                QPoint new_pos = mouse_ev->pos();
                QPoint delta = new_pos - global_grab_origin;
                House::getInstance().multi_move(delta, true);
                global_grab_origin = new_pos;
            }
            else
            {
                point_pair.second = mouse_ev->pos();
                preview->setGeometry(calc_rect());

                switch (paint_target)
                {
                case PaintObject::NONE:
                    break;
                case PaintObject::OUTLINE:
                    House::getInstance().update_preview(mouse_ev->pos());
                    break;
                case PaintObject::LAY_OBJECT:
                    House::getInstance().selected = preview;
                    emit House::getInstance().selected_update_signal();
                    break;
                }
            }
            return true;
        }
    }
    else if (watched == this && event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent* mouse_ev = static_cast<QMouseEvent*>(event);
        if (mouse_ev->button() == Qt::LeftButton && House::getInstance().mode != OperateMode::DRAG)
        {
            point_pair.second = mouse_ev->pos();
            if (!preview->isHidden())
            {
                House::getInstance().selected.reset();
                preview->hide();
            }
            std::shared_ptr<BaseObject> new_one;
            std::vector<std::shared_ptr<LayObject>> contains;
            switch (paint_target)
            {
            case PaintObject::NONE:
                GraphArea::getInstance().clear_multi_select();
                for (auto obj: House::getInstance().lay_objs)
                {
                    if (preview->geometry().contains(obj->geometry()))
                    {
                        obj->is_selected = true;
                        contains.push_back(obj);
                    }
                }
                if (!contains.empty())
                {
                    House::getInstance().selected = contains.back();
                    emit House::getInstance().selected_update_signal();
                }
                GraphArea::getInstance().graph_update();
                return true;
            case PaintObject::OUTLINE:
                House::getInstance().apply_preview();
                break;
            case PaintObject::LAY_OBJECT:
                new_one = std::make_shared<LayObject>(this, calc_rect(), paint_lay_obj_name);
                House::getInstance().add(new_one);
                break;
            }
            return true;
        }
    }


    return QWidget::eventFilter(watched, event);
}

void PaintArea::paintEvent(QPaintEvent *)
{
    pix->fill(Qt::transparent);
    QPainter pix_painter(pix.get());

    if (bg_pix && !output_flag)
        pix_painter.drawPixmap(0, 0, width, height, *bg_pix);

    // 绘制检测到的墙体轮廓（折线）
    if (!output_flag && walls_detected && !wall_contours.empty()) {
        pix_painter.setPen(QPen(QColor(255, 0, 0, 200), 3, Qt::SolidLine));

        for (const QVector<QPoint>& wall : wall_contours) {
            if (wall.size() >= 2) {
                // 绘制折线
                for (int i = 0; i < wall.size() - 1; ++i) {
                    pix_painter.drawLine(wall[i], wall[i+1]);
                }
            }
        }

        // 显示检测到的墙体段数量
        pix_painter.setPen(QPen(Qt::blue, 2));
        pix_painter.drawText(10, 20, QString("检测到 %1 段墙体").arg(wall_contours.size()));
    }

    if (output_flag)
    {
        House::getInstance().paint(pix_painter);
        emit paint_finished();
    }

    QPainter painter(this);
    painter.drawPixmap(0, 0, *pix);

    if (House::getInstance().outline->preview && !House::getInstance().outline->points.empty()) {
        painter.drawLine(House::getInstance().outline->points.back(), *House::getInstance().outline->preview);
    }
}

QRect PaintArea::calc_rect()
{
    int x = std::min({point_pair.first.x(), point_pair.second.x()});
    int y = std::min({point_pair.first.y(), point_pair.second.y()});
    int x_r = std::max({point_pair.first.x(), point_pair.second.x()});
    int y_r = std::max({point_pair.first.y(), point_pair.second.y()});
    return QRect(x, y, x_r-x, y_r-y);
}
