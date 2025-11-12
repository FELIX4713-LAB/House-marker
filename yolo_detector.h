#ifndef YOLODETECTOR_H
#define YOLODETECTOR_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QVector>
#include <QPoint>
#include <QJsonArray>
#include <QImage>

class YOLODetector : public QObject
{
    Q_OBJECT
public:
    explicit YOLODetector(QObject *parent = nullptr);
    ~YOLODetector();

    bool detectWalls(const QImage& image);
    QVector<QVector<QPoint>> getWallContours() const { return wall_contours; }
    bool isAvailable() const { return yoloAvailable; }

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    QProcess *pythonProcess;
    QVector<QVector<QPoint>> wall_contours;
    QString scriptPath;
    bool yoloAvailable;

    void setupPythonScript();
    void parseDetectionResults(const QByteArray& output);

signals:
    void detectionFinished(bool success);
};

#endif // YOLODETECTOR_H
