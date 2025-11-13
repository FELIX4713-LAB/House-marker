#include "yolo_detector.h"
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QDebug>
#include <QBuffer>
#include <QTimer>
#include <QPainter>

YOLODetector::YOLODetector(QObject *parent)
    : QObject(parent), yoloAvailable(false)
{
    pythonProcess = new QProcess(this);
    connect(pythonProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &YOLODetector::onProcessFinished);
    connect(pythonProcess, &QProcess::errorOccurred,
            this, &YOLODetector::onProcessError);

    setupPythonScript();
}

YOLODetector::~YOLODetector()
{
    if (pythonProcess->state() == QProcess::Running) {
        pythonProcess->terminate();
        pythonProcess->waitForFinished(3000);
    }
}

void YOLODetector::setupPythonScript()
{
    scriptPath = QDir::tempPath() + "/wall_detector.py";

    QFile scriptFile(scriptPath);
    if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&scriptFile);
        out << R"(# -*- coding: utf-8 -*-
import cv2
import numpy as np
import sys
import json
import os
import traceback

print("DEBUG: Python script started", file=sys.stderr)

def close_single_wall_contours(contours, image_shape):
    """将单段墙体轮廓强制闭合"""
    closed_contours = []

    for contour in contours:
        if len(contour) >= 2:
            # 如果只有2个点（单线段），创建矩形闭合
            if len(contour) == 2:
                print(f"DEBUG: Closing single segment wall with 2 points", file=sys.stderr)
                p1 = contour[0]
                p2 = contour[1]

                # 计算线段方向和长度
                dx = p2[0] - p1[0]
                dy = p2[1] - p1[1]
                length = np.sqrt(dx*dx + dy*dy)

                if length > 0:
                    # 创建墙体厚度（垂直于线段方向）
                    wall_thickness = max(5, min(15, int(length * 0.1)))  # 动态厚度

                    # 计算垂直方向
                    if abs(dx) > abs(dy):  # 近似水平线
                        closed_contour = [
                            [p1[0], p1[1] - wall_thickness//2],
                            [p2[0], p2[1] - wall_thickness//2],
                            [p2[0], p2[1] + wall_thickness//2],
                            [p1[0], p1[1] + wall_thickness//2]
                        ]
                    else:  # 近似垂直线
                        closed_contour = [
                            [p1[0] - wall_thickness//2, p1[1]],
                            [p1[0] + wall_thickness//2, p1[1]],
                            [p2[0] + wall_thickness//2, p2[1]],
                            [p2[0] - wall_thickness//2, p2[1]]
                        ]
                    closed_contours.append(closed_contour)
                else:
                    closed_contours.append(contour)

            # 如果只有3个点，添加第4个点形成四边形
            elif len(contour) == 3:
                print(f"DEBUG: Closing 3-point wall contour", file=sys.stderr)
                # 复制第一个点作为最后一个点，形成闭合
                closed_contour = contour + [contour[0]]
                closed_contours.append(closed_contour)

            # 对于4个点以上的轮廓，确保首尾相连
            else:
                # 检查是否已经闭合（首尾点相同）
                first_point = contour[0]
                last_point = contour[-1]
                distance = np.sqrt((first_point[0]-last_point[0])**2 + (first_point[1]-last_point[1])**2)

                if distance > 5:  # 首尾点距离较大，需要闭合
                    print(f"DEBUG: Closing multi-point wall contour with {len(contour)} points", file=sys.stderr)
                    closed_contour = contour + [contour[0]]
                    closed_contours.append(closed_contour)
                else:
                    # 已经近似闭合，直接使用
                    closed_contours.append(contour)

    print(f"DEBUG: Closed {len(closed_contours)} wall contours", file=sys.stderr)
    return closed_contours

def detect_walls_optimized(image_path):
    """优化的墙体检测方法，专门处理户型图"""
    try:
        print(f"DEBUG: Loading image from {image_path}", file=sys.stderr)

        # 处理透明背景
        img = cv2.imread(image_path, cv2.IMREAD_UNCHANGED)
        if img is None:
            print("ERROR: Failed to load image", file=sys.stderr)
            return []

        print(f"DEBUG: Image loaded, shape: {img.shape}, channels: {img.shape[2] if len(img.shape) > 2 else 1}", file=sys.stderr)

        # 处理透明背景
        if len(img.shape) == 3 and img.shape[2] == 4:
            print("DEBUG: Processing transparent background", file=sys.stderr)
            # 分离alpha通道
            b, g, r, alpha = cv2.split(img)

            # 创建白色背景
            white_bg = np.ones_like(b) * 255

            # 根据alpha通道混合前景和背景
            alpha_float = alpha.astype(float) / 255.0
            b = (b * alpha_float + white_bg * (1 - alpha_float)).astype(np.uint8)
            g = (g * alpha_float + white_bg * (1 - alpha_float)).astype(np.uint8)
            r = (r * alpha_float + white_bg * (1 - alpha_float)).astype(np.uint8)

            # 合并通道
            img = cv2.merge([b, g, r])
            print("DEBUG: Transparent background converted to white", file=sys.stderr)
        else:
            print("DEBUG: No transparency detected, using original image", file=sys.stderr)
            if len(img.shape) == 2:
                # 如果是灰度图，转换为BGR
                img = cv2.cvtColor(img, cv2.COLOR_GRAY2BGR)

        # 转换为灰度图
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

        # 方法1: 尝试自适应阈值（适用于光照不均的图片）
        binary_adaptive = cv2.adaptiveThreshold(gray, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
                                               cv2.THRESH_BINARY_INV, 11, 2)

        # 方法2: 尝试全局阈值（适用于对比度强的图片）
        _, binary_global = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY_INV + cv2.THRESH_OTSU)

        # 方法3: 边缘检测
        edges = cv2.Canny(gray, 50, 150)

        # 合并三种方法的结果
        combined = cv2.bitwise_or(binary_adaptive, binary_global)
        combined = cv2.bitwise_or(combined, edges)

        # 形态学操作强化墙体线条
        kernel = np.ones((2,2), np.uint8)
        combined = cv2.morphologyEx(combined, cv2.MORPH_CLOSE, kernel)
        combined = cv2.morphologyEx(combined, cv2.MORPH_OPEN, kernel)

        # 寻找轮廓
        contours, _ = cv2.findContours(combined, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        print(f"DEBUG: Found {len(contours)} raw contours", file=sys.stderr)

        wall_contours = []
        min_area = gray.shape[0] * gray.shape[1] * 0.001  # 动态最小面积

        for i, contour in enumerate(contours):
            area = cv2.contourArea(contour)
            if area > min_area:
                # 计算轮廓周长
                perimeter = cv2.arcLength(contour, True)
                if perimeter == 0:
                    continue

                # 简化轮廓（根据轮廓复杂度调整epsilon）
                epsilon = 0.005 * perimeter  # 更精细的简化
                approx = cv2.approxPolyDP(contour, epsilon, True)

                # 转换为点列表
                points = []
                for point in approx:
                    x = int(point[0][0])
                    y = int(point[0][1])
                    points.append([x, y])

                # 只保留有意义的轮廓（至少2个点）
                if len(points) >= 2:
                    wall_contours.append(points)
                    print(f"DEBUG: Added contour {i} with {len(points)} points, area={area:.1f}", file=sys.stderr)

        print(f"DEBUG: Before closing: {len(wall_contours)} wall contours", file=sys.stderr)

        # 强制闭合单段墙体
        closed_wall_contours = close_single_wall_contours(wall_contours, gray.shape)

        print(f"DEBUG: After closing: {len(closed_wall_contours)} closed wall contours", file=sys.stderr)

        return closed_wall_contours

    except Exception as e:
        print(f"ERROR in wall detection: {str(e)}", file=sys.stderr)
        print(f"TRACEBACK: {traceback.format_exc()}", file=sys.stderr)
        return []

def main():
    if len(sys.argv) != 2:
        print('ERROR: Usage: python wall_detector.py <image_path>', file=sys.stderr)
        sys.exit(1)

    image_path = sys.argv[1]

    if not os.path.exists(image_path):
        print(f'ERROR: Image file not found: {image_path}', file=sys.stderr)
        sys.exit(1)

    try:
        # 执行墙体检测
        contours = detect_walls_optimized(image_path)

        # 输出JSON结果
        result_json = json.dumps(contours, separators=(',', ':'))
        print(result_json)
        print("DEBUG: JSON output completed", file=sys.stderr)

    except Exception as e:
        print(f'ERROR in main: {str(e)}', file=sys.stderr)
        print(f'TRACEBACK: {traceback.format_exc()}', file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
)";
        scriptFile.close();
        yoloAvailable = true;
        qDebug() << "Python script updated with wall closing functionality";
    } else {
        yoloAvailable = false;
        qDebug() << "Failed to create Python script";
    }
}

bool YOLODetector::detectWalls(const QImage& image)
{
    if (!yoloAvailable) {
        qDebug() << "YOLO detector not available";
        return false;
    }

    // 将QImage保存为临时文件
    QString tempImagePath = QDir::tempPath() + "/temp_detection_image.png";

    qDebug() << "Saving temporary image to:" << tempImagePath;

    // 处理透明背景
    QImage processedImage = image;
    if (processedImage.hasAlphaChannel()) {
        qDebug() << "Image has alpha channel, removing transparency";

        // 创建白色背景
        QImage whiteBg(processedImage.size(), QImage::Format_RGB32);
        whiteBg.fill(Qt::white);

        // 在白色背景上绘制原图
        QPainter painter(&whiteBg);
        painter.drawImage(0, 0, processedImage);
        painter.end();

        processedImage = whiteBg;
    } else if (processedImage.format() != QImage::Format_RGB32) {
        processedImage = processedImage.convertToFormat(QImage::Format_RGB32);
    }

    if (!processedImage.save(tempImagePath, "PNG")) {
        qDebug() << "Failed to save temporary image";
        return false;
    }

    qDebug() << "Temporary image saved successfully, size:" << processedImage.size();

    // 执行Python脚本
    QStringList arguments;
    arguments << scriptPath << tempImagePath;

    qDebug() << "Starting Python process with script:" << scriptPath;

    pythonProcess->start("python", arguments);

    if (!pythonProcess->waitForStarted(5000)) {
        qDebug() << "Failed to start Python process";
        return false;
    }

    qDebug() << "Python process started successfully";
    return true;
}

void YOLODetector::onProcessError(QProcess::ProcessError error)
{
    qDebug() << "Python process error:" << error;
    qDebug() << "Error string:" << pythonProcess->errorString();
    emit detectionFinished(false);
}

void YOLODetector::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QByteArray output = pythonProcess->readAllStandardOutput();
    QByteArray error = pythonProcess->readAllStandardError();

    qDebug() << "=== Python Process Finished ===";
    qDebug() << "Exit code:" << exitCode;
    qDebug() << "Exit status:" << exitStatus;
    qDebug() << "Stdout length:" << output.length();
    qDebug() << "Stderr:" << error;
    qDebug() << "=== End Python Output ===";

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        parseDetectionResults(output);
    } else {
        qDebug() << "Python process failed";
        wall_contours.clear();
        emit detectionFinished(false);
    }
}

void YOLODetector::parseDetectionResults(const QByteArray& output)
{
    wall_contours.clear();

    if (output.isEmpty()) {
        qDebug() << "Empty output from Python script";
        emit detectionFinished(false);
        return;
    }

    qDebug() << "Raw JSON output:" << output;

    try {
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(output, &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            qDebug() << "JSON parse error:" << parseError.errorString();
            qDebug() << "At offset:" << parseError.offset;
            emit detectionFinished(false);
            return;
        }

        if (doc.isArray()) {
            QJsonArray contoursArray = doc.array();
            qDebug() << "JSON array size:" << contoursArray.size();

            for (int i = 0; i < contoursArray.size(); ++i) {
                QJsonValue contourValue = contoursArray[i];
                if (contourValue.isArray()) {
                    QVector<QPoint> contour;
                    QJsonArray pointsArray = contourValue.toArray();

                    for (int j = 0; j < pointsArray.size(); ++j) {
                        QJsonValue pointValue = pointsArray[j];
                        if (pointValue.isArray()) {
                            QJsonArray pointArray = pointValue.toArray();
                            if (pointArray.size() >= 2) {
                                int x = pointArray[0].toInt();
                                int y = pointArray[1].toInt();
                                contour.append(QPoint(x, y));
                            }
                        }
                    }

                    if (contour.size() >= 2) {
                        wall_contours.append(contour);
                        qDebug() << "Added contour" << i << "with" << contour.size() << "points";
                    }
                }
            }

            qDebug() << "YOLO detection completed. Found" << wall_contours.size() << "wall segments";
            emit detectionFinished(true);
        } else {
            qDebug() << "Output is not a JSON array";
            emit detectionFinished(false);
        }
    } catch (const std::exception& e) {
        qDebug() << "Exception parsing JSON:" << e.what();
        emit detectionFinished(false);
    }
}
