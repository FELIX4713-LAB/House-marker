#ifndef GRAPHAREA_H
#define GRAPHAREA_H

#include <QWidget>
#include <deque>
#include <set>
#include <QLine>
#include <QFrame>
#include "paintobject.h"

class ThemeNode;
class BaseNode: public QWidget
{
public:
    BaseNode(QWidget* parent);
    int node_update(int start_h, int level);

public:
    ThemeNode* father;
    bool is_grab;
    int grab_start_y;
    int h_start;
    int h_end;
    int level;
    bool eventFilter(QObject *watched, QEvent *event);
};


class ObjectNode: public BaseNode
{
public:
    ObjectNode(QWidget* parent, std::shared_ptr<LayObject> obj);

public:
    std::shared_ptr<LayObject> obj;
};


class ThemeNode: public BaseNode
{
public:
    ThemeNode(QWidget* parent);
    bool remove_obj(std::shared_ptr<LayObject> obj);
    void add_node(std::shared_ptr<BaseNode> node);
    void remove_node(std::shared_ptr<BaseNode> node);

public:
    std::deque<std::shared_ptr<BaseNode>> nodes;
};


class GraphArea: public QWidget
{
public:
    static GraphArea& getInstance()
    {
        static GraphArea self;
        return self;
    }
    // node style
    QString get_styleSheet(BaseNode* node);
    QString get_styleSheet(std::shared_ptr<BaseNode> node);
    // modify
    void add_obj(std::shared_ptr<LayObject> obj);
    void remove_obj(std::shared_ptr<LayObject> obj);
    void add_theme();
    void remove_select();
    void clear();
    // other
    void graph_update();
    void clear_multi_select();
    void node_grab_handle(BaseNode* node);
    void node_release_handle(BaseNode* node);
    // import & export
    void import_graph(const std::vector<std::shared_ptr<LayObject>>& obj_nodes, const std::vector<std::pair<int, int>>& links);
    std::vector<std::pair<int, int>> export_graph(const std::map<std::shared_ptr<LayObject>, int>& obj_map);

private:
    GraphArea();

public:
    // content
    std::shared_ptr<ThemeNode> global_theme;
    std::vector<std::shared_ptr<BaseNode>> free_nodes;

    std::weak_ptr<BaseNode> selected_node;
    std::function<void(int)> height_change_callback;
private:
    std::shared_ptr<BaseNode> grab_node;
    QFrame* line;
};

#endif // GRAPHAREA_H
