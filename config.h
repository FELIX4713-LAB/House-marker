#ifndef CONFIG_H
#define CONFIG_H
#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <QString>
#include <QColor>

class Config
{
private:
    Config()
    {
        parse_from_file("./house.conf", "./lay_object.conf", "./metric_range.txt");
    }
public:
    static Config& getInstance()
    {
        static Config instance;
        return instance;
    }

    void parse_from_file(const char* conf_path, const char* obj_conf_path, const char* metric_range_path)
    {
        std::string _bg_color = "black";
        theme_node_color = "pink";

        std::string line;
        std::stringstream ss;

        // general config
        std::ifstream f_in(conf_path, std::ios::in);
        if (f_in.is_open())
        {
            std::string k, v;
            int v_int;
            float v_f;
            while (std::getline(f_in, line))
            {
                ss.clear();
                ss.str(line);
                ss >> k >> v;
                if (k == "paint_area_size")             paint_area_size         = atoi(v.c_str());
                if (k == "move_step")                   move_step               = atoi(v.c_str());
                if (k == "object_min_size")             object_min_size         = atoi(v.c_str());
                if (k == "scale_ratio")                 scale_ratio             = atof(v.c_str());
                if (k == "background_color")            _bg_color               = v.c_str();
                if (k == "background_transparence")     bg_transpacence         = atof(v.c_str());

                if (k == "graph_node_height")           graph_node_height       = atoi(v.c_str());
                if (k == "graph_node_width")           graph_node_width        = atoi(v.c_str());
                if (k == "graph_edit_gap_size")         graph_edit_gap_size     = atoi(v.c_str());
                if (k == "graph_edit_tab_size")         graph_edit_tab_size     = atoi(v.c_str());
                if (k == "theme_node_color")            theme_node_color        = v.c_str();

            }
            f_in.close();
        }
        else
        {
            std::ofstream f_out(conf_path, std::ios::out);
            f_out << "paint_area_size"              << ' ' << paint_area_size       << std::endl;
            f_out << "move_step"                    << ' ' << move_step             << std::endl;
            f_out << "object_min_size"              << ' ' << object_min_size       << std::endl;
            f_out << "scale_ratio"                  << ' ' << scale_ratio           << std::endl;
            f_out << "background_color"             << ' ' << _bg_color             << std::endl;
            f_out << "background_transparence"      << ' ' << bg_transpacence       << std::endl;

            f_out << "graph_node_height"            << ' ' << graph_node_height     << std::endl;
            f_out << "graph_node_width"            << ' ' << graph_node_width      << std::endl;
            f_out << "graph_edit_gap_size"          << ' ' << graph_edit_gap_size   << std::endl;
            f_out << "graph_edit_tab_size"          << ' ' << graph_edit_tab_size   << std::endl;
            f_out << "theme_node_color"             << ' ' << theme_node_color      << std::endl;
            f_out.close();
        }

        // layout objects config
        std::ifstream f_obj_conf(obj_conf_path, std::ios::in);
        if (f_obj_conf.is_open())
        {
            std::string obj_name, obj_color;
            while (std::getline(f_obj_conf, line))
            {
                ss.clear();
                ss.str(line);
                ss >> obj_name >> obj_color;
                obj_info[obj_name.c_str()]["color"] = QString(obj_color.c_str());
            }
            f_obj_conf.close();
        }
        else
        {
            obj_info["独立展柜"]["color"] = "orange";
            obj_info["台面展柜"]["color"] = "#ff7b7b";      //red
            obj_info["展板"]["color"] = "yellow";
            obj_info["入墙展柜"]["color"] = "#7de3ff";      //blue
            obj_info["支撑墙"]["color"] = "gray";
            obj_info["出入口"]["color"] = "#90ff49";         // green

            std::ofstream f_out(obj_conf_path, std::ios::out);
            f_out << "独立展柜" << ' ' << obj_info["独立展柜"]["color"].toStdString() << std::endl;
            f_out << "台面展柜" << ' ' << obj_info["台面展柜"]["color"].toStdString() << std::endl;
            f_out << "展板" << ' ' << obj_info["展板"]["color"].toStdString() << std::endl;
            f_out << "入墙展柜" << ' ' << obj_info["入墙展柜"]["color"].toStdString() << std::endl;
            f_out << "支撑墙" << ' ' << obj_info["支撑墙"]["color"].toStdString() << std::endl;
            f_out << "出入口" << ' ' << obj_info["出入口"]["color"].toStdString() << std::endl;
            f_out.close();
        }

        char buffer[1024];

        for (const auto& pair: obj_info)
        {
            sprintf(buffer, "QWidget { border: none; background-color: %s; }", pair.second.at("color").toStdString().c_str());
            QString style_sheet(buffer);
            obj_info[pair.first]["style_sheet"] = style_sheet;
        }

        bg_color = QColor(_bg_color.c_str());
        bg_color.setAlpha(55);

        // metric range
        std::ifstream f_metric(metric_range_path, std::ios::in);
        if (f_metric.is_open())
        {
            std::string metric_range_str;
            for (int i = 0; i < 6 && std::getline(f_metric, metric_range_str); ++i)
                ranges[i] = QString(metric_range_str.c_str());
        }
        else
        {
            std::ofstream f_out(metric_range_path, std::ios::out);
            for (int i = 0; i < 6; ++i)
                f_out << ranges[i].toStdString() << '\n';
            f_out.close();
        }
    }

    // paint area
    int paint_area_size = 1024;
    int move_step = 1;
    int object_min_size = 10;
    float scale_ratio = 1.1;
    float bg_transpacence = 0.5;
    int paint_area_selected_line_width = 3;
    QColor bg_color;    // outline fill color
    QString select_rect_styleSheet = "QWidget { border: 1px solid black; background-color: transparent; }";

    // graph edit area
    int graph_node_height = 25;
    int graph_node_width = 50;
    int graph_edit_gap_size = 30;
    int graph_edit_tab_size = 25;
    std::string theme_node_color;
    int graph_area_selected_line_width = 4;

    // layout objects
    QString label_select_mode = "框选模式";
    QString label_outline = "轮廓";
    std::map<QString, std::map<QString, QString>> obj_info;

    // metric range
    std::vector<QString> ranges = std::vector<QString>(6, "0-1");
};

#endif // CONFIG_H
