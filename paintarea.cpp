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
    : QWidget(parent),
    width(_width),
    height(_height),
    preview(std::make_shared<BaseRectObject>(this)),
    paint_target(PaintObject::NONE),
    output_flag(false),
    pix(std::make_unique<QPixmap>(width, height)),
    walls_detected(false),
    yoloDetector(std::make_unique<YOLODetector>(this))
{
    preview->hide();

    int display_width = std::min(width, 512);
    int display_height = std::min(height, 1000);

    this->setGeometry(0, 0, display_width, display_height);
    House::getInstance().outline = std::make_shared<OutlineWall>(this);
    House::getInstance().outline->setGeometry(0, 0, display_width, display_height);
    House::getInstance().paint_area = this;
    installEventFilter(this);

    connect(yoloDetector.get(), &YOLODetector::detectionFinished,
            this, &PaintArea::onWallDetectionFinished);
}

void PaintArea::load_img(const QString& path)
{
    if (path.isEmpty()) {
        qDebug() << "Empty image path";
        return;
    }

    QPixmap tmp;
    if (!tmp.load(path)) {
        qDebug() << "Failed to load image:" << path;
        return;
    }

    // 固定目标宽度为512，计算对应的高度（按原比例缩放）
    const int targetWidth = 512;
    // 计算原图宽高比
    double aspectRatio = static_cast<double>(tmp.height()) / tmp.width();
    // 根据固定宽度计算目标高度（确保至少为1，避免无效尺寸）
    int targetHeight = qMax(1, static_cast<int>(targetWidth * aspectRatio));

    // 按计算出的目标尺寸缩放图片（保持比例，平滑缩放）
    QPixmap scaledPix = tmp.scaled(
        targetWidth,
        targetHeight,
        Qt::KeepAspectRatio,  // 强制保持原比例（避免拉伸）
        Qt::SmoothTransformation  // 平滑缩放，减少锯齿
        );

    // 更新画布尺寸为缩放后的图片尺寸
    width = targetWidth;
    height = targetHeight;

    // 创建背景图并绘制缩放后的图片
    bg_pix = std::make_unique<QPixmap>(width, height);
    bg_pix->fill(Qt::transparent);  // 透明背景
    QPainter painter(bg_pix.get());
    painter.setOpacity(Config::getInstance().bg_transpacence);  // 保持原有透明度设置
    painter.drawPixmap(0, 0, scaledPix);  // 绘制缩放后的图片
    painter.end();

    // 重新调整窗口显示尺寸（可选，根据需求决定是否跟随图片尺寸）
    this->setGeometry(this->x(), this->y(), width, height);

    // 加载后自动检测墙体
    detectWalls();
    update();
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

    qDebug() << "Starting wall detection with YOLO...";

    // 显示检测中的状态
    wall_contours.clear();
    walls_detected = false;
    update();

    // 转换为QImage并检测
    QImage image = bg_pix->toImage();
    qDebug() << "Image format:" << image.format();
    qDebug() << "Image size:" << image.size();
    if (!yoloDetector->detectWalls(image)) {
        qDebug() << "Failed to start YOLO detection, using traditional method";
        wallDetection(image);
    }
}

void PaintArea::onWallDetectionFinished(bool success)
{
    if (success) {
        wall_contours = yoloDetector->getWallContours();
        walls_detected = true;
        qDebug() << "YOLO detection completed. Found" << wall_contours.size() << "wall segments";
        emit walls_detected_signal(wall_contours.size());
    } else {
        qDebug() << "YOLO detection failed, using traditional method";
        if (bg_pix && !bg_pix->isNull()) {
            wallDetection(bg_pix->toImage());
        }
    }
    update();
}

void PaintArea::clearWalls()
{
    wall_contours.clear();
    walls_detected = false;
    update();
}

// 传统墙体检测方法
void PaintArea::wallDetection(const QImage& inputImage)
{
    wall_contours.clear();

    if (inputImage.isNull()) return;

    // 转换为灰度图像
    QImage grayImage = inputImage.convertToFormat(QImage::Format_Grayscale8);
    int width = grayImage.width();
    int height = grayImage.height();

    // 步骤1: 针对性二值化（墙体为深色，设为白色；背景为浅色，设为黑色）
    QImage binaryImage(width, height, QImage::Format_Grayscale8);
    int wallThreshold = 100; // 降低阈值，确保深色墙体被识别（可根据实际效果微调）
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int brightness = qRed(grayImage.pixel(x, y));
            if (brightness < wallThreshold) {
                binaryImage.setPixel(x, y, qRgb(255, 255, 255)); // 墙体像素设为白色
            } else {
                binaryImage.setPixel(x, y, qRgb(0, 0, 0));       // 背景设为黑色
            }
        }
    }

    // 步骤2: 形态学操作（膨胀+腐蚀，强化墙体线条并去除噪声）
    QImage morphImage = binaryImage;
    // 先膨胀（让细线条变粗，避免断裂）
    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            if (qRed(binaryImage.pixel(x, y)) == 255) {
                // 膨胀：只要3x3邻域有墙体像素，就保留当前点
                bool hasWall = true;
                for (int i = -1; i <= 1; ++i) {
                    for (int j = -1; j <= 1; ++j) {
                        if (qRed(binaryImage.pixel(x+j, y+i)) == 255) {
                            hasWall = true;
                            break;
                        }
                    }
                    if (hasWall) break;
                }
                if (hasWall) {
                    morphImage.setPixel(x, y, qRgb(255, 255, 255));
                }
            }
        }
    }
    // 再腐蚀（去除膨胀带来的多余噪声）
    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            if (qRed(morphImage.pixel(x, y)) == 255) {
                // 腐蚀：3x3邻域全为墙体像素才保留，否则设为背景
                bool allWall = true;
                for (int i = -1; i <= 1; ++i) {
                    for (int j = -1; j <= 1; ++j) {
                        if (qRed(morphImage.pixel(x+j, y+i)) == 0) {
                            allWall = false;
                            break;
                        }
                    }
                    if (!allWall) break;
                }
                if (!allWall) {
                    morphImage.setPixel(x, y, qRgb(0, 0, 0));
                }
            }
        }
    }

    // 步骤3: 追踪轮廓并找到连续墙体
    QVector<QPoint> contourPoints = traceWallContours(morphImage);
    wall_contours = findContinuousWalls(contourPoints);

    walls_detected = true;
    qDebug() << "Traditional wall detection completed. Found" << wall_contours.size() << "wall segments";
    emit walls_detected_signal(wall_contours.size());

    update();
}

QVector<QPoint> PaintArea::traceWallContours(const QImage& edgeImage)
{
    QVector<QPoint> contourPoints;
    int width = edgeImage.width();
    int height = edgeImage.height();

    QImage visited(width, height, QImage::Format_Grayscale8);
    visited.fill(0);

    int dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            if (qRed(edgeImage.pixel(x, y)) > 128 && qRed(visited.pixel(x, y)) == 0) {
                std::queue<QPoint> queue;
                queue.push(QPoint(x, y));
                visited.setPixel(x, y, qRgb(255, 255, 255));

                while (!queue.empty()) {
                    QPoint current = queue.front();
                    queue.pop();
                    contourPoints.append(current);

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

    std::function<QVector<QPoint>(const QVector<QPoint>&, double)> douglasPeucker;
    douglasPeucker = [&douglasPeucker](const QVector<QPoint>& points, double epsilon) -> QVector<QPoint> {
        if (points.size() <= 2) return points;

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

        if (maxDistance > epsilon) {
            QVector<QPoint> result1 = douglasPeucker(points.mid(0, index + 1), epsilon);
            QVector<QPoint> result2 = douglasPeucker(points.mid(index), epsilon);

            result1.removeLast();
            return result1 + result2;
        } else {
            return {points.first(), points.last()};
        }
    };

    QVector<QPoint> currentWall;
    const int maxGap = 10;
    const int minWallLength = 20;

    for (int i = 0; i < contourPoints.size(); ++i) {
        if (currentWall.isEmpty()) {
            currentWall.append(contourPoints[i]);
        } else {
            QPoint lastPoint = currentWall.last();
            QPoint currentPoint = contourPoints[i];

            double distance = std::sqrt(std::pow(currentPoint.x() - lastPoint.x(), 2) +
                                        std::pow(currentPoint.y() - lastPoint.y(), 2));

            if (distance <= maxGap) {
                currentWall.append(currentPoint);
            } else {
                if (currentWall.size() > minWallLength) {
                    QVector<QPoint> simplified = douglasPeucker(currentWall, 3.0);
                    walls.append(simplified);
                }
                currentWall.clear();
                currentWall.append(currentPoint);
            }
        }
    }

    if (currentWall.size() > minWallLength) {
        QVector<QPoint> simplified = douglasPeucker(currentWall, 3.0);
        walls.append(simplified);
    }

    return walls;
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
