#ifndef FUNCTIONDOCKWIDGET_H
#define FUNCTIONDOCKWIDGET_H

#include <QDockWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>

class FunctionDockWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit FunctionDockWidget(QWidget *parent = nullptr);

    // 属性控件
    QComboBox *comboBox_type;
    QSpinBox *spinBox_0;
    QSpinBox *spinBox_1;
    QSpinBox *spinBox_2;
    QSpinBox *spinBox_3;
    QPushButton *btn_adjust_object;
    QPushButton *btn_rm_select;

    // 操作控件
    QPushButton *btn_create_theme;
    QPushButton *btn_remove_node;
    QPushButton *delete_btn;
    QPushButton *align_btn;
    QPushButton *check_btn;

    // 缩放控件
    QPushButton *btn_global_scale_down;
    QPushButton *btn_global_scale_up;
    QPushButton *neat_btn;

    // 参数调整控件
    QSlider *symmetry_slider;
    QSlider *rhythm_slider;
    QSlider *visibility_slider;
    QSlider *safety_slider;
    QSlider *channel_slider;

    QCheckBox *auto_calc_check;

    // 对齐控件
    QSpinBox *same_w;
    QSpinBox *same_h;
    QPushButton *btn_sameW;
    QPushButton *btn_sameH;
    QSpinBox *delta;
    QSpinBox *angle;
    QPushButton *btn_align;
    QPushButton *btn_align_2;
    QPushButton *btn_average;

    // 计算控件
    QPushButton *pbt_calc;
    QSpinBox *min_distance_spin;
    QSpinBox *min_angle_spin;
    QCheckBox *align_contour_check;

private:
    void setupUI();




};

#endif // FUNCTIONDOCKWIDGET_H
