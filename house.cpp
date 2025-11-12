#include "house.h"
#include <QMessageBox>
#include <QFile>
#include <memory>
#include <utility>
#include "config.h"

bool House::add(std::shared_ptr<BaseObject> obj)
{
    // 过滤太小的object
    if (obj->width() < Config::getInstance().object_min_size && obj->height() < Config::getInstance().object_min_size)
        return false;

    if (auto lay_obj = std::dynamic_pointer_cast<LayObject>(obj))
    {
        lay_objs.push_back(lay_obj);
        GraphArea::getInstance().add_obj(lay_obj);
    }
    selected = obj;
    emit House::getInstance().selected_update_signal();
    obj->show();
    obj->setFocus();
    emit object_num_change();
    return true;
}


void House::remove_selected()
{
    auto new_objs = std::vector<std::shared_ptr<LayObject>>();
    for (auto obj: lay_objs)
    {
        if (!obj->is_selected)
        {
            new_objs.push_back(obj);
        }
        else
        {
            GraphArea::getInstance().remove_obj(obj);
        }
    }
    std::swap(new_objs, lay_objs);
    emit object_num_change();
}

void House::align_selected(bool average)
{
    float aver_x = 0, aver_y = 0, delta_x = 0, delta_y = 0;
    int count = 0;
    std::vector<std::shared_ptr<LayObject>> selected_objs;
    for (auto obj: lay_objs)
    {
        if (obj->is_selected)
        {
            count++;
            aver_x += obj->x();
            aver_y += obj->y();
            selected_objs.push_back(obj);
        }
    }

    if (count == 0) return;
    aver_x /= count;
    aver_y /= count;
    for (auto obj: selected_objs)
    {
        delta_x += std::abs(obj->x() - aver_x);
        delta_y += std::abs(obj->y() - aver_y);
    }

    bool align_x = delta_x < delta_y;
    if (!average)
    {
        for (auto obj: selected_objs)
        {
            if (align_x)
                obj->move(aver_x, obj->y());
            else
                obj->move(obj->x(), aver_y);
        }
    }
    else
    {
        if (selected_objs.size() <= 1)
            return;

        std::vector<std::pair<int, std::shared_ptr<LayObject>>> coords;
        int total_len = 0;
        for (auto obj: selected_objs)
        {
            int coord = align_x? obj->y(): obj->x();
            coords.push_back(std::make_pair(coord, obj));
            if (obj != selected_objs.back())
                total_len += align_x? obj->height(): obj->width();
        }
        std::sort(coords.begin(), coords.end(), [=](std::pair<int, std::shared_ptr<LayObject>> l, std::pair<int, std::shared_ptr<LayObject>> r){
            return l.first <= r.first;
        });
        int gap = (coords.back().first - coords.front().first - total_len) / (selected_objs.size() - 1);
        if (gap <= 0)
            return;

        int ruler = coords.begin()->first;
        for (int i = 0; i < coords.size(); ++i)
        {
            if (align_x)
            {
                coords[i].second->move(aver_x, ruler);
                ruler += coords[i].second->height();
            }
            else
            {
                coords[i].second->move(ruler, aver_y);
                ruler += coords[i].second->width();
            }
            ruler += gap;
        }
    }
}

void House::copy_selected()
{
    std::vector<std::shared_ptr<LayObject>> copy_objs;
    int min_x, min_y;
    min_x = min_y = Config::getInstance().paint_area_size;
    for (auto obj: lay_objs)
    {
        if (obj->is_selected)
        {
            copy_objs.emplace_back(std::make_shared<LayObject>(*obj));
            obj->is_selected = false;
            copy_objs.back()->is_selected = true;
            min_x = std::min(min_x, obj->x());
            min_y = std::min(min_y, obj->y());
        }
    }

    for (auto obj: copy_objs)
    {
        obj->move(obj->pos() - QPoint(min_x, min_y));
        House::getInstance().add(obj);
    }

}



void House::clear()
{
    selected.reset();
    outline->points.clear();
    outline->completed = false;
    lay_objs.clear();
    GraphArea::getInstance().clear();
    emit object_num_change();
}

void House::global_scale(float ratio)
{
    for (auto& point: outline->points) {
        point.setX(point.x() * ratio);
        point.setY(point.y() * ratio);
    }
    outline->update();

    for (auto& obj: lay_objs) {
        obj->move(obj->pos() * ratio);
        obj->scale(ratio);
        obj->update();
    }

    emit House::getInstance().selected_update_signal();
}


void House::multi_move(QPoint delta, bool total_move)
{
    if (total_move)
    {
        for (QPointF& p: outline->points)
        {
            p.setX(p.x() + delta.x());
            p.setY(p.y() + delta.y());
        }
        outline->update();
    }
    for (auto& obj: lay_objs)
    {
        if (total_move || obj->is_selected)
        {
            QPoint new_pos(obj->x() + delta.x(), obj->y() + delta.y());
            obj->move(new_pos);
            obj->update();
        }
    }
}

bool House::load_from_json(const QString &path)
{
    clear();

    json j;
    std::ifstream file(path.toStdString());
    if (!file.is_open())
    {
        QMessageBox::warning(nullptr, "warning", "无法加载json文件！", QMessageBox::Ok);
        return false;
    }
    file >> j;
    file.close();

    if (j.contains("outline"))
    {
        for (const auto& p: j["outline"].get<std::vector<std::vector<int>>>())
        {
            outline->points.emplace_back(p[0], p[1]);
        }
    }

    if (j.contains("layout_objects"))
    {
        std::vector<int> pos, size;
        std::string type;
        for (const auto& obj_json: j["layout_objects"])
        {
            obj_json["position"].get_to(pos);
            obj_json["size"].get_to(size);
            obj_json["type"].get_to(type);

            std::shared_ptr<LayObject> obj = std::make_shared<LayObject>(paint_area, QRect(pos[0], pos[1], size[0], size[1]), QString::fromStdString(type));
            obj->setSizeF(size[0], size[1]);
            lay_objs.push_back(obj);
            GraphArea::getInstance().add_obj(obj);
            obj->raise();
            obj->show();
        }
    }

    if (j.contains("graph"))
    {
        GraphArea::getInstance().import_graph(lay_objs, j["graph"].get<std::vector<std::pair<int, int>>>());
    }

    outline->completed = true;
    emit object_num_change();
    return true;
}

json House::make_json()
{
    json j;

    // outline
    for (QPointF& p: outline->points) {
        j["outline"].push_back({p.x(), p.y()});
    }

    // layout objects
    for (int i = 0; i < lay_objs.size(); ++i) {
        json obj_json;
        obj_json["id"] = i;
        obj_json["position"] = {lay_objs[i]->x(), lay_objs[i]->y()};
        obj_json["size"] = {lay_objs[i]->width(), lay_objs[i]->height()};
        obj_json["type"] = lay_objs[i]->type_name.toStdString();
        j["layout_objects"].push_back(obj_json);
    }

    // graph
    std::map<std::shared_ptr<LayObject>, int> obj_map;
    for (int i = 0; i < lay_objs.size(); ++i)
        obj_map[lay_objs[i]] = i;
    std::vector<std::pair<int, int>> graph_links = GraphArea::getInstance().export_graph(obj_map);
    for (const auto& pair: graph_links)
    {
        j["graph"].push_back(pair);
    }

    return j;
}

bool House::export_json(const QString& path)
{
    QString data = get_json_str();
    QFile file(path);
    try
    {
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            return false;
        }
        QTextStream out(&file);
        out << data;
        file.close();
    }
    catch (...)
    {
        QMessageBox::warning(nullptr, "warning", "导出json失败", QMessageBox::Ok);
        file.close();
        return false;
    }
    return true;
}

QString House::get_json_str()
{
    auto j = make_json();
    std::string json_str = j.dump(4);
    return QString::fromStdString(json_str);
}


void House::paint(QPainter& painter)
{
    outline->paint(painter);

    for (auto& obj: lay_objs)
    {
        QBrush b = QBrush(QColor(Config::getInstance().obj_info[obj->type_name]["color"]));
        painter.fillRect(obj->geometry(), b);
    }
}

void House::object_status_change(BaseObject *obj)
{
    int max_w = Config::getInstance().paint_area_size;
    int max_h = Config::getInstance().paint_area_size;
    int min_size = Config::getInstance().object_min_size;

    if (auto rect_obj = dynamic_cast<BaseRectObject*>(obj))
    {
        // limit size
        int w = rect_obj->width(), h = rect_obj->height();
        if (w < min_size && h < min_size)
            rect_obj->setSizeF(min_size, min_size);
        else if (w < min_size)
            rect_obj->setSizeF(min_size, h);
        else if (h < min_size)
            rect_obj->setSizeF(w, min_size);

        // limit position
        int x = rect_obj->x(), y = rect_obj->y();
        if (x < 0)
            x = 0;
        else if (x + rect_obj->width() > max_w)
            x = max_w - rect_obj->width();
        if (y < 0)
            y = 0;
        else if (y + rect_obj->height() > max_h)
            y = max_h - rect_obj->height();
        if (x != rect_obj->x() || y != rect_obj->y())
            rect_obj->move(x, y);

        for (auto& lay_obj: lay_objs) {
            if (lay_obj.get() == obj) {
                selected = lay_obj;
                emit House::getInstance().selected_update_signal();
                return;
            }
        }

    }
}

void House::update_preview(QPoint pos)
{
    if (normal_outline && !outline->points.empty()) {
        QPointF tmp = outline->points.back();
        if (abs(tmp.x() - pos.x()) < abs(tmp.y() - pos.y())) {
            pos.setX(tmp.x());
        } else {
            pos.setY(tmp.y());
        }
    }

    outline->preview = std::make_shared<QPointF>(pos);
    selected = outline;
    emit selected_update_signal();
    paint_area->update();
}

void House::apply_preview()
{
    outline->points.push_back(*outline->preview);
    outline->preview.reset();
    paint_area->update();
}

void House::seal()
{
    if (normal_outline && outline->points.size() > 2)
    {
        QPointF front = outline->points.front();
        QPointF back = outline->points.back();
        int i = outline->points.size()-1;
        if (abs(front.x() - back.x()) < abs(front.y() - back.y()))
        {
            while (abs(outline->points[i-1].x() - back.x()) < 1e-5) {
                --i;
            }
            for (int j = i; j < outline->points.size(); ++j) {
                outline->points[j].setX(front.x());
            }
        }
        else
        {
            while (abs(outline->points[i-1].y() - back.y()) < 1e-5) {
                --i;
            }
            for (int j = i; j < outline->points.size(); ++j) {
                outline->points[j].setX(front.y());
            }
        }
        outline->points.push_back(outline->points.front());
    }
    if (outline->points.size() > 2)
    {
        outline->points.push_back(outline->points.front());
        std::vector<QPointF> new_outline(outline->points.begin(), outline->points.begin()+2);
        for (int i = 2; i < outline->points.size(); ++i){
            int size = new_outline.size();
            if (is_collinear(new_outline[size-1], new_outline[size-2], outline->points[i])) {
                new_outline.pop_back();
            }
            new_outline.push_back(outline->points[i]);
        }
        using std::swap;
        swap(new_outline, outline->points);
    }
    outline->completed = true;

    outline->preview.reset();
    paint_area->update();
}

void House::align(int delta, int angle)
{
    outline->align(angle, delta);
}



bool House::is_collinear(const QPointF &p1, const QPointF &p2, const QPointF &p3)
{
    qreal x1 = p2.x() - p1.x();
    qreal y1 = p2.y() - p1.y();
    qreal x2 = p3.x() - p1.x();
    qreal y2 = p3.y() - p1.y();

    qreal crossProduct = x1 * y2 - y1 * x2;
    return std::abs(crossProduct) < 1e-5;
}

