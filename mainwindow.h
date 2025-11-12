#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "paintarea.h"
#include "functiondockwidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    bool eventFilter(QObject *watched, QEvent *event) override;

    void import_file(QString path, int idx=-1);
    void export_file(QString path);

    // 移除重复的DockWidget函数声明（这些已经在QMainWindow中存在）
    // void addDockWidget(Qt::DockWidgetArea , QDockWidget * dockwidget);
    // void addDockWidget(Qt::DockWidgetArea area, QDockWidget * dockwidget, Qt::Orientation orientation);
    // void splitDockWidget(QDockWidget * first, QDockWidget * second, Qt::Orientation orientation);
    // void tabifyDockWidget(QDockWidget * first, QDockWidget * second);
    // void setDockNestingEnabled(bool enabled);

private slots:
    // 移除重复的 update_object_info 声明，只保留一个
    void on_comboBox_currentTextChanged(const QString &arg1);
    void on_action_import_triggered();
    void on_action_export_triggered();
    void on_action_save_json_triggered();
    void on_action_unload_img_triggered();
    void on_btn_adjust_object_clicked();
    void on_btn_rm_select_clicked();
    void on_btn_global_scale_up_clicked();
    void on_btn_global_scale_down_clicked();
    void on_action_clear_triggered();
    void on_btn_sameW_clicked();
    void on_btn_sameH_clicked();
    void on_btn_align_clicked();
    void on_action_opendir_triggered();
    void object_num_change();
    void on_btn_align_2_clicked();
    void on_btn_average_clicked();
    void on_pbt_calc_clicked();
    void toggleFullscreen();
    void exitFullscreen();
    void adjustWindowSize();

    void on_control_clicked();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    Ui::MainWindow *ui;
    PaintArea* paint_area;
    QTimer* timer;
    QString cur_json_path;
    std::pair<std::vector<QString>, int> file_list;

    // 添加功能面板 DockWidget
    FunctionDockWidget *functionDock;

    // 私有函数
    void connectFunctionDockSignals();
    void update_object_info();  // 只保留这一个声明
};

#endif // MAINWINDOW_H
