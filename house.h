#ifndef HOUSE_H
#define HOUSE_H


#include <QObject>
#include <QPainter>
#include "grapharea.h"
#include "paintobject.h"
#include "json.hpp"

using json = nlohmann::json;

enum class OperateMode
{
    PAINT,
    ADD_SELECT,
    DRAG
};

class House : public QObject{
    Q_OBJECT
public:
    static House& getInstance() {
        static House instance;
        return instance;
    }
    // modify
    bool add(std::shared_ptr<BaseObject> obj);
    void remove_selected();
    void align_selected(bool average);
    void copy_selected();
    void clear();
    // operate
    void global_scale(float ratio);
    void multi_move(QPoint delta, bool total_move=false);
    // import & export
    bool load_from_json(const QString& path);
    json make_json();
    bool export_json(const QString& path);
    QString get_json_str();

    // 只用于导出图片，并未重写paintEvent
    void paint(QPainter& painter);

    void object_status_change(BaseObject* obj);

    // outline
    void update_preview(QPoint pos);
    void apply_preview();
    void seal();
    void align(int delta, int angle);


private:
    House(): QObject(nullptr){}
    bool is_collinear(const QPointF& p1, const QPointF& p2, const QPointF& p3);

public:
    QWidget* paint_area;
    OperateMode mode = OperateMode::PAINT;
    bool normal_outline;
    std::weak_ptr<BaseObject> selected;
    std::shared_ptr<OutlineWall> outline;

    std::vector<std::shared_ptr<LayObject>> lay_objs;

signals:
    //提醒MainWindow选择的控件发生变化
    void selected_update_signal();
    void object_num_change();
};

#endif // HOUSE_H
