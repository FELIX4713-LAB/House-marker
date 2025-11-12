#include "grapharea.h"
#include "config.h"
#include "house.h"
#include <QMouseEvent>
#include <QStyle>
#include <QLayout>

int node_w;
int node_h;
int tab_size;
int gap_size;

// maps
std::map<LayObject*, std::shared_ptr<LayObject>> obj_ptr_map;
std::map<BaseNode*, std::shared_ptr<BaseNode>> node_ptr_map;
std::map<std::shared_ptr<LayObject>, std::shared_ptr<BaseNode>> obj2node;


//=====================================================================================
//                                 utils
//=====================================================================================
template <typename Op>
bool travel(std::shared_ptr<BaseNode> root, Op op)
{
    if(op(root))
        return true;
    if (auto theme_node = std::dynamic_pointer_cast<ThemeNode>(root))
    {
        for (std::shared_ptr<BaseNode> child: theme_node->nodes)
        {
            if (travel(child, op))
                return true;
        }
    }
    return false;
}


void debug_print(std::shared_ptr<BaseNode> node, bool is_start=true)
{
    if (is_start)
        qDebug() << "=============================";
    qDebug().noquote() << QString(node->level, '\t') + QString(std::dynamic_pointer_cast<ThemeNode>(node)? "theme": "object") << node.get();
    if (auto theme_node = std::dynamic_pointer_cast<ThemeNode>(node))
    {
        for (auto child: theme_node->nodes)
        {
            debug_print(child, false);
        }
    }
    if (is_start)
        qDebug() << "=============================\n";
}

//=====================================================================================
//                                   Base Node
//=====================================================================================


BaseNode::BaseNode(QWidget *parent)
    : QWidget(parent), level(0)
{
    this->setGeometry(0, 0, node_w, node_h);
    this->show();
    this->installEventFilter(this);
}

int BaseNode::node_update(int y, int level)
{
    this->level = level;
    h_start = y;
    this->move(level * tab_size, y);

    if (auto obj_node = dynamic_cast<ObjectNode*>(this))
    {
        y += node_h;
    }
    else if (auto theme_node = dynamic_cast<ThemeNode*>(this))
    {
        y += node_h;
        for (auto node: theme_node->nodes)
        {
            node->father = theme_node;
            y = node->node_update(y, level + 1);
        }
    }
    this->setStyleSheet(GraphArea::getInstance().get_styleSheet(this));

    h_end = y;
    return y;
}

bool BaseNode::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == this && event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent* mouse_ev = static_cast<QMouseEvent*>(event);
        if (mouse_ev->buttons() & Qt::LeftButton)
        {
            is_grab = true;
            grab_start_y = mouse_ev->pos().y();
            GraphArea::getInstance().selected_node = node_ptr_map[this];
            GraphArea::getInstance().clear_multi_select();
            if (auto obj_node = dynamic_cast<ObjectNode*>(this))
            {
                obj_node->obj->is_selected = true;
                House::getInstance().selected = obj_node->obj;
                emit House::getInstance().selected_update_signal();
            }
            else if (auto theme_node = dynamic_cast<ThemeNode*>(this))
            {
                travel(node_ptr_map[this], [&](std::shared_ptr<BaseNode> node){
                    if (auto obj_node = std::dynamic_pointer_cast<ObjectNode>(node))
                    {
                        obj_node->obj->is_selected = true;
                    }
                    return false;
                });
            }

            return true;
        }
    }
    else if (is_grab && event->type() == QEvent::MouseMove)
    {
        int grab_delta_y = static_cast<QMouseEvent*>(event)->pos().y() - grab_start_y;

        if (auto theme = dynamic_cast<ThemeNode*>(this))
        {
            for (std::shared_ptr<BaseNode> child: theme->nodes)
            {
                travel(child, [=](std::shared_ptr<BaseNode> n)->bool{
                    n->move(n->level * tab_size, n->y() + grab_delta_y);
                    return false;
                });
            }
        }
        this->move(level * tab_size, this->y() + grab_delta_y);

        return true;
    }
    else if (is_grab && event->type() == QEvent::MouseButtonRelease)
    {
        is_grab = false;        
        int release_y = mapToParent(static_cast<QMouseEvent*>(event)->pos()).y();

        if (release_y < h_start || release_y > h_end)
        {
            GraphArea::getInstance().node_grab_handle(this);
            GraphArea::getInstance().node_release_handle(this);
        }
        GraphArea::getInstance().graph_update();

        return true;
    }

    return QObject::eventFilter(watched, event);
}


ObjectNode::ObjectNode(QWidget *parent, std::shared_ptr<LayObject> obj)
    : BaseNode(parent), obj(obj)
{
}


//=====================================================================================
//                                  Theme Node
//=====================================================================================

ThemeNode::ThemeNode(QWidget *parent)
    : BaseNode(parent)
{
    this->resize(node_w * 10, node_h);
}

void ThemeNode::add_node(std::shared_ptr<BaseNode> node)
{
    if (std::dynamic_pointer_cast<ThemeNode>(node))
    {
        nodes.push_back(node);
    }
    else
    {
        nodes.push_front(node);
    }
    node->father = this;
}

void ThemeNode::remove_node(std::shared_ptr<BaseNode> node)
{
    for (auto it = this->nodes.begin(); it != this->nodes.end(); ++it)
    {
        if (*it == node)
        {
            this->nodes.erase(it);
            return;
        }
    }
}

//=====================================================================================
//                                      Graph Area
//=====================================================================================

GraphArea::GraphArea(): QWidget(nullptr)
{
    node_w = Config::getInstance().graph_node_width;
    node_h = Config::getInstance().graph_node_height;
    tab_size = Config::getInstance().graph_edit_tab_size;
    gap_size = Config::getInstance().graph_edit_gap_size;

    global_theme = std::make_shared<ThemeNode>(this);
    node_ptr_map[global_theme.get()] = global_theme;

    line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setMinimumWidth(1000);
    line->setFixedHeight(2);
    line->setLineWidth(2);
    line->move(0, global_theme->geometry().bottom() + gap_size);
}

QString GraphArea::get_styleSheet(BaseNode *node)
{
    auto locked = selected_node.lock();
    bool is_single_selected = locked && locked.get() == node;
    bool is_multi_selected = false;

    QString border_style = is_single_selected? "dashed": "solid";
    QString content_color;
    if (dynamic_cast<ThemeNode*>(node))
    {
        content_color = "pink";
    }
    else if (auto obj_node = dynamic_cast<ObjectNode*>(node))
    {
        content_color = Config::getInstance().obj_info[obj_node->obj->type_name]["color"];
        is_multi_selected = obj_node->obj->is_selected;
    }
    else
        content_color = "white";

    int width = (is_single_selected || is_multi_selected)? Config::getInstance().graph_area_selected_line_width: 1;

    return QString("QWidget { border: %1px %2 black; background-color: %3; }")
        .arg(width).arg(border_style).arg(content_color);
}

QString GraphArea::get_styleSheet(std::shared_ptr<BaseNode> node)
{
    return get_styleSheet(node.get());
}

void GraphArea::add_obj(std::shared_ptr<LayObject> obj)
{
    std::shared_ptr<BaseNode> node = std::make_shared<ObjectNode>(this, obj);
    obj2node[obj] = node;
    node_ptr_map[node.get()] = node;
    obj_ptr_map[obj.get()] = obj;
    free_nodes.push_back(node);

    graph_update();
}


void GraphArea::remove_obj(std::shared_ptr<LayObject> obj)
{
    bool ret = travel(global_theme, [&](std::shared_ptr<BaseNode> node){
        std::shared_ptr<ObjectNode> obj_node = std::dynamic_pointer_cast<ObjectNode>(node);
        if (obj_node && obj_node->obj == obj)
        {
            obj_node->father->remove_node(node);
            return true;
        }
        return false;
    });

    if (!ret)
    {
        for (auto it = free_nodes.begin(); it != free_nodes.end(); ++it)
        {
            if (std::dynamic_pointer_cast<ObjectNode>(*it)->obj == obj)
            {
                free_nodes.erase(it);
                break;
            }
        }
    }

    node_ptr_map.erase(obj2node[obj].get());
    obj_ptr_map.erase(obj.get());
    obj2node.erase(obj);

    graph_update();
}

void GraphArea::add_theme()
{
    std::shared_ptr<BaseNode> locked = selected_node.lock();
    std::shared_ptr<ThemeNode> parent_theme = std::dynamic_pointer_cast<ThemeNode>(locked);

    std::shared_ptr<ThemeNode> new_theme = std::make_shared<ThemeNode>(this);
    node_ptr_map[new_theme.get()] = new_theme;
    // selected_node = new_theme;
    selected_node = global_theme;
    if (parent_theme)
        parent_theme->add_node(new_theme);
    else
        global_theme->add_node(new_theme);

    std::vector<std::shared_ptr<BaseNode>> obj_nodes;
    for (auto pair: obj2node)
    {
        if (pair.first->is_selected && pair.first->type_name != "支撑墙")   // temp
        {
            if (pair.second->father)
                pair.second->father->remove_node(pair.second);
            else
                free_nodes.erase(std::find(free_nodes.begin(), free_nodes.end(), pair.second));
            obj_nodes.push_back(pair.second);
        }
    }
    for (auto node: obj_nodes)
        node->father = new_theme.get();
    new_theme->nodes.insert(new_theme->nodes.begin(), obj_nodes.begin(), obj_nodes.end());

    graph_update();
}

void GraphArea::remove_select()
{
    std::shared_ptr<BaseNode> locked = selected_node.lock();

    if (std::dynamic_pointer_cast<ThemeNode>(locked))
    {
        travel(locked, [&](std::shared_ptr<BaseNode> node){
            if (std::dynamic_pointer_cast<ObjectNode>(node))
                free_nodes.push_back(node);
            else if (std::dynamic_pointer_cast<ThemeNode>(node) && node != global_theme)
                node_ptr_map.erase(node.get());
            if (locked == global_theme)
            {
                global_theme->remove_node(node);
            }
            return false;
        });
        if (locked != global_theme)
        {
            locked->father->remove_node(locked);
            node_ptr_map.erase(locked.get());
            selected_node.reset();
        }
        graph_update();
    }
    else if (std::dynamic_pointer_cast<ObjectNode>(locked))
    {
        if (locked->father)
        {
            free_nodes.push_back(locked);
            locked->father->remove_node(locked);
            locked->father = nullptr;
            selected_node.reset();
            graph_update();
        }
    }
}

void GraphArea::clear()
{
    global_theme.reset(new ThemeNode(this));
    free_nodes.clear();
    grab_node.reset();
    obj_ptr_map.clear();
    node_ptr_map.clear();
    node_ptr_map[global_theme.get()] = global_theme;
    obj2node.clear();
    graph_update();
}

void GraphArea::graph_update()
{
    int y = global_theme->node_update(0, 0);
    y += gap_size;

    line->move(0, y);
    y = line->geometry().bottom();

    auto locked = selected_node.lock();
    for (auto node: free_nodes)
    {
        node->father = nullptr;
        node->level = 0;
        node->move(0, y);
        node->h_start = y;
        node->h_end = y + node_h;
        node->setStyleSheet(get_styleSheet(node));
        y += node_h;
    }
    int area_height = y + node_h;
    height_change_callback(area_height);
    // debug_print(global_theme);
}

void GraphArea::clear_multi_select()
{
    for (auto pair: obj2node)
    {
        pair.first->is_selected = false;
    }
}

void GraphArea::node_grab_handle(BaseNode *node)
{
    if (node == global_theme.get()) return;

    // check free nodes
    for (auto it = free_nodes.begin(); it != free_nodes.end(); ++it)
    {
        if (it->get() == node)
        {
            grab_node = *it;
            free_nodes.erase(it);
            return;
        }
    }

    // check processed nodes
    travel(global_theme, [&](std::shared_ptr<BaseNode> n){
        if (n.get() == node)
        {
            grab_node = n;
            n->father->remove_node(n);
            return true;
        }
        return false;
    });
}

void GraphArea::node_release_handle(BaseNode *node)
{
    if (!grab_node) return;

    int target_center = node->y() + node_h / 2;
    if (target_center < global_theme->geometry().bottom())
    {
        global_theme->add_node(grab_node);
    }
    else if (target_center < line->geometry().top() - gap_size)
    {
        std::deque<std::shared_ptr<BaseNode>> q;
        std::shared_ptr<BaseNode> p;
        q.push_back(global_theme);
        bool inserted = false;
        while (!q.empty() && !inserted)
        {
            p = q.front();
            q.pop_front();
            auto theme = std::dynamic_pointer_cast<ThemeNode>(p);
            if (target_center >= theme->geometry().top() && target_center <= theme->geometry().bottom())
            {
                theme->add_node(grab_node);
                break;
            }
            else
            {
                for (auto it = theme->nodes.begin(); it != theme->nodes.end(); ++it)
                {
                    if (target_center >= it->get()->h_start && target_center <= it->get()->h_end)
                    {
                        if (std::dynamic_pointer_cast<ThemeNode>(*it))
                        {
                            q.push_back(*it);
                        }
                        else if (dynamic_cast<ThemeNode*>(node))
                        {
                            theme->add_node(grab_node);
                        }
                        else if (std::dynamic_pointer_cast<ObjectNode>(*it))
                        {
                            theme->nodes.insert(it, grab_node);
                            inserted = true;
                        }
                        break;
                    }
                }
            }
        }
    }
    else if (target_center < line->geometry().bottom() || std::dynamic_pointer_cast<ThemeNode>(grab_node))
    {
        global_theme->add_node(grab_node);
    }
    else if (target_center >= line->geometry().bottom() && std::dynamic_pointer_cast<ObjectNode>(grab_node))
    {
        auto it = free_nodes.begin();
        for (; it != free_nodes.end(); ++it)
        {
            QRect geom = it->get()->geometry();
            if (target_center > geom.top() && target_center < geom.bottom())
            {
                break;
            }
        }
        free_nodes.insert(it, grab_node);
    }
    else
    {
        free_nodes.push_back(grab_node);
    }
    grab_node.reset();
}


std::vector<std::pair<int, int> > GraphArea::export_graph(const std::map<std::shared_ptr<LayObject>, int>& obj_map)
{
    std::map<ThemeNode*, int> theme_id;
    int id = -1;
    travel(global_theme, [&](std::shared_ptr<BaseNode> node){
        if (auto theme = std::dynamic_pointer_cast<ThemeNode>(node))
        {
            theme_id[theme.get()] = id--;
        }
        return false;
    });

    std::vector<std::pair<int, int>> links;
    travel(global_theme, [&](std::shared_ptr<BaseNode> node){
        if (node != global_theme)
        {
            int start = theme_id[node->father];
            int end;
            if (auto theme = std::dynamic_pointer_cast<ThemeNode>(node))
            {
                end = theme_id[theme.get()];
            }
            else if (auto obj_node = std::dynamic_pointer_cast<ObjectNode>(node))
            {
                end = obj_map.at(obj_node->obj);
            }
            links.push_back({start, end});
        }
        return false;
    });

    return links;
}

void GraphArea::import_graph(const std::vector<std::shared_ptr<LayObject>> &obj_nodes, const std::vector<std::pair<int, int> > &links)
{
    std::map<int, std::shared_ptr<ThemeNode>> theme_map;
    theme_map[-1] = global_theme;
    auto construct_theme_map = [&](int idx){
        if (idx < 0 && theme_map.find(idx) == theme_map.end())
        {
            theme_map[idx] = std::make_shared<ThemeNode>(this);
            node_ptr_map[theme_map[idx].get()] = theme_map[idx];
        }
    };
    for (auto pair: links)
    {
        construct_theme_map(pair.first);
        construct_theme_map(pair.second);
    }

    auto get_node_ptr = [&](int idx)->std::shared_ptr<BaseNode>
    {
        if (idx < 0)
            return theme_map[idx];
        else
            return obj2node[obj_nodes[idx]];
    };

    for (auto pair: links)
    {
        std::shared_ptr<ThemeNode> start = std::dynamic_pointer_cast<ThemeNode>(get_node_ptr(pair.first));
        std::shared_ptr<BaseNode> end = get_node_ptr(pair.second);
        start->add_node(end);
        auto end_it = std::find(free_nodes.begin(), free_nodes.end(), end);
        if (end_it != free_nodes.end())
            free_nodes.erase(end_it);
    }
    graph_update();
}

