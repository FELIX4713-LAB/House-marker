#include "functiondockwidget.h"
#include "config.h"

FunctionDockWidget::FunctionDockWidget(QWidget *parent)
    : QDockWidget("功能面板", parent)
{
    setupUI();
}

void FunctionDockWidget::setupUI()
{
    QWidget *mainWidget = new QWidget();
    setWidget(mainWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);

    // 减少布局间距
    mainLayout->setSpacing(4);  // 默认值通常是6-8
    mainLayout->setContentsMargins(6, 6, 6, 6);  // 减少边距

    // === 属性组 ===
    QGroupBox *attributeGroup = new QGroupBox("属性");
    QFormLayout *attributeLayout = new QFormLayout(attributeGroup);

    comboBox_type = new QComboBox();

    spinBox_0 = new QSpinBox();
    spinBox_0->setRange(-10000, 10000);
    spinBox_1 = new QSpinBox();
    spinBox_1->setRange(-10000, 10000);
    spinBox_2 = new QSpinBox();
    spinBox_2->setRange(0, 10000);
    spinBox_3 = new QSpinBox();
    spinBox_3->setRange(0, 10000);

    btn_adjust_object = new QPushButton("确认");
    btn_rm_select = new QPushButton("删除");

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(btn_adjust_object);
    buttonLayout->addWidget(btn_rm_select);

    attributeLayout->addRow("类型:", comboBox_type);
    attributeLayout->addRow("X:", spinBox_0);
    attributeLayout->addRow("Y:", spinBox_1);
    attributeLayout->addRow("W:", spinBox_2);
    attributeLayout->addRow("H:", spinBox_3);
    attributeLayout->addRow(buttonLayout);

    mainLayout->addWidget(attributeGroup);

    // === 操作组 ===
    QGroupBox *operationGroup = new QGroupBox("操作");
    QVBoxLayout *operationLayout = new QVBoxLayout(operationGroup);

    QHBoxLayout *opRow1 = new QHBoxLayout();
    delete_btn = new QPushButton("删除");
    align_btn = new QPushButton("对齐");
    //check_btn = new QPushButton("勾勾(A)");
    opRow1->addWidget(delete_btn);
    opRow1->addWidget(align_btn);
    //opRow1->addWidget(check_btn);

    QHBoxLayout *opRow2 = new QHBoxLayout();
    btn_create_theme = new QPushButton("创建主题");
    btn_remove_node = new QPushButton("删除节点");
    opRow2->addWidget(btn_create_theme);
    opRow2->addWidget(btn_remove_node);

    operationLayout->addLayout(opRow1);
    operationLayout->addLayout(opRow2);

    mainLayout->addWidget(operationGroup);

    // === 缩放控制组 ===
    QGroupBox *scaleGroup = new QGroupBox("缩放控制");
    QHBoxLayout *scaleLayout = new QHBoxLayout(scaleGroup);

    btn_global_scale_down = new QPushButton("全局缩小");
    btn_global_scale_up = new QPushButton("全局放大");
    neat_btn = new QPushButton("整齐规");

    scaleLayout->addWidget(btn_global_scale_down);
    scaleLayout->addWidget(btn_global_scale_up);
    scaleLayout->addWidget(neat_btn);

    mainLayout->addWidget(scaleGroup);

    // === 参数调整组 ===
    QGroupBox *paramGroup = new QGroupBox("参数调整");
    QFormLayout *paramLayout = new QFormLayout(paramGroup);

    symmetry_slider = new QSlider(Qt::Horizontal);
    symmetry_slider->setRange(0, 100);
    rhythm_slider = new QSlider(Qt::Horizontal);
    rhythm_slider->setRange(0, 100);
    visibility_slider = new QSlider(Qt::Horizontal);
    visibility_slider->setRange(0, 100);
    safety_slider = new QSlider(Qt::Horizontal);
    safety_slider->setRange(0, 100);
    channel_slider = new QSlider(Qt::Horizontal);
    channel_slider->setRange(0, 100);

    paramLayout->addRow("对称均 0-1:", symmetry_slider);
    paramLayout->addRow("节奏韵 0-1:", rhythm_slider);
    paramLayout->addRow("可视可 0-1:", visibility_slider);
    paramLayout->addRow("公共安 0-1:", safety_slider);
    paramLayout->addRow("通道宽 0-1:", channel_slider);

    //auto_calc_check = new QCheckBox("打开文件时自动计算");
    //paramLayout->addRow(auto_calc_check);

    mainLayout->addWidget(paramGroup);

    // === 对齐控制组 ===
    QGroupBox *alignGroup = new QGroupBox("对齐控制");
    QFormLayout *alignLayout = new QFormLayout(alignGroup);

    same_w = new QSpinBox();
    same_w->setRange(0, 10000);
    same_h = new QSpinBox();
    same_h->setRange(0, 10000);
    btn_sameW = new QPushButton("相同宽度");
    btn_sameH = new QPushButton("相同高度");

    QHBoxLayout *sameWidthLayout = new QHBoxLayout();
    sameWidthLayout->addWidget(same_w);
    sameWidthLayout->addWidget(btn_sameW);

    QHBoxLayout *sameHeightLayout = new QHBoxLayout();
    sameHeightLayout->addWidget(same_h);
    sameHeightLayout->addWidget(btn_sameH);

    delta = new QSpinBox();
    delta->setRange(0, 1000);
    delta->setValue(20);
    angle = new QSpinBox();
    angle->setRange(0, 180);
    angle->setValue(10);
    btn_align = new QPushButton("对齐");

    QHBoxLayout *alignParamLayout = new QHBoxLayout();
    alignParamLayout->addWidget(new QLabel("距离:"));
    alignParamLayout->addWidget(delta);
    alignParamLayout->addWidget(new QLabel("角度:"));
    alignParamLayout->addWidget(angle);
    alignParamLayout->addWidget(btn_align);

    QHBoxLayout *alignButtonsLayout = new QHBoxLayout();
    btn_align_2 = new QPushButton("对齐选中");
    btn_average = new QPushButton("平均分布");
    alignButtonsLayout->addWidget(btn_align_2);
    alignButtonsLayout->addWidget(btn_average);

    alignLayout->addRow("宽度统一:", sameWidthLayout);
    alignLayout->addRow("高度统一:", sameHeightLayout);
    alignLayout->addRow(alignParamLayout);
    alignLayout->addRow(alignButtonsLayout);

    mainLayout->addWidget(alignGroup);

    // === 计算组 ===
    QGroupBox *calcGroup = new QGroupBox("计算");
    QFormLayout *calcLayout = new QFormLayout(calcGroup);

    pbt_calc = new QPushButton("计算");
    min_distance_spin = new QSpinBox();
    min_distance_spin->setRange(0, 1000);
    min_distance_spin->setValue(20);
    min_angle_spin = new QSpinBox();
    min_angle_spin->setRange(0, 180);
    min_angle_spin->setValue(10);
    align_contour_check = new QCheckBox("对齐轮廓点");

    calcLayout->addRow(pbt_calc);
    calcLayout->addRow("最小距离:", min_distance_spin);
    calcLayout->addRow("最小角度:", min_angle_spin);
    calcLayout->addRow(align_contour_check);

    mainLayout->addWidget(calcGroup);

    // 添加弹性空间
    mainLayout->addStretch();

    // 设置最小尺寸
    setMinimumWidth(330);
}

