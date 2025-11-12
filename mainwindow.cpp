#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QMessageBox>
#include <QVector>
#include <QStandardPaths>
#include <QScrollBar>
#include <QKeyEvent>
#include <QLayout>
#include <QDir>
#include <QDirIterator>
#include <memory>
#include <windows.h>
#include <iostream>
#include "config.h"
#include "house.h"
#include "grapharea.h"
#include <QKeyEvent>
#include <QShortcut>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , functionDock(nullptr)
{
    int paint_area_size = Config::getInstance().paint_area_size;
    ui->setupUi(this);

    setFixedSize(1400,800);

    connect(&House::getInstance(), &House::object_num_change, this, &MainWindow::object_num_change);

    this->setWindowTitle("虚拟展厅设计工具");

    //创建图标对象
    QIcon icon("C:/Users/DELL/Desktop/house_marker/logo.PNG");

    //设置图标
    this->setWindowIcon(icon);

    // === 创建功能面板 DockWidget ===
    functionDock = new FunctionDockWidget(this);
    addDockWidget(Qt::RightDockWidgetArea, functionDock);

    // 初始化类型组合框
    functionDock->comboBox_type->addItem(Config::getInstance().label_outline);
    for (auto each : Config::getInstance().obj_info) {
        functionDock->comboBox_type->addItem(each.first);
    }

    // graph edit area - 保持不变
    GraphArea::getInstance().setParent(ui->scrollAreaWidgetContents);
    QLayout* layout = new QVBoxLayout(ui->scrollAreaWidgetContents);
    layout->addWidget(&GraphArea::getInstance());

    // 修改信号连接，使用 functionDock 中的控件
    connect(functionDock->btn_create_theme, &QPushButton::clicked, [this](){
        GraphArea::getInstance().add_theme();
    });
    connect(functionDock->btn_remove_node, &QPushButton::clicked, [this](){
        GraphArea::getInstance().remove_select();
    });

    GraphArea::getInstance().height_change_callback = [&](int height){
        ui->scrollAreaWidgetContents->setFixedHeight(height);
    };
    GraphArea::getInstance().graph_update();

    // paint area - 保持不变
    paint_area = new PaintArea(ui->widget_paint, paint_area_size, paint_area_size);
    ui->widget_paint->setFixedSize(paint_area_size, paint_area_size);
    paint_area->show();

    // metric area - 保持不变
    ui->range0->setText(Config::getInstance().ranges[0]);
    ui->range1->setText(Config::getInstance().ranges[1]);
    ui->range2->setText(Config::getInstance().ranges[2]);
    ui->range3->setText(Config::getInstance().ranges[3]);
    ui->range4->setText(Config::getInstance().ranges[4]);
    ui->range5->setText(Config::getInstance().ranges[5]);

    // 模式选择组合框 - 保持不变
    ui->comboBox->addItem(Config::getInstance().label_select_mode);
    ui->comboBox->addItem(Config::getInstance().label_outline);
    for (auto each: Config::getInstance().obj_info)
    {
        ui->comboBox->addItem(each.first);
    }
    ui->comboBox->setCurrentIndex(0);

    connect(&House::getInstance(), &House::selected_update_signal, this, &MainWindow::update_object_info);

    // 连接功能面板的信号
    connectFunctionDockSignals();

    timer = new QTimer(this);
    timer->setInterval(100);
    connect(timer, &QTimer::timeout, [&]{
        int before = ui->textEdit->verticalScrollBar()->value();
        ui->textEdit->setText(House::getInstance().get_json_str());
        ui->textEdit->verticalScrollBar()->setValue(before);
    });
    timer->start();

    paint_area->installEventFilter(this);
    ui->comboBox->installEventFilter(this);
    installEventFilter(this);

    ui->textEdit->setAcceptDrops(false);
    setAcceptDrops(true);

    new QShortcut(QKeySequence(Qt::Key_F11), this, SLOT(toggleFullscreen()));
    new QShortcut(QKeySequence(Qt::Key_Escape), this, SLOT(exitFullscreen()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::connectFunctionDockSignals()
{
    // 属性调整
    connect(functionDock->btn_adjust_object, &QPushButton::clicked, this, &MainWindow::on_btn_adjust_object_clicked);
    connect(functionDock->btn_rm_select, &QPushButton::clicked, this, &MainWindow::on_btn_rm_select_clicked);

    // 缩放控制
    connect(functionDock->btn_global_scale_up, &QPushButton::clicked, this, &MainWindow::on_btn_global_scale_up_clicked);
    connect(functionDock->btn_global_scale_down, &QPushButton::clicked, this, &MainWindow::on_btn_global_scale_down_clicked);

    // 对齐控制
    connect(functionDock->btn_sameW, &QPushButton::clicked, this, &MainWindow::on_btn_sameW_clicked);
    connect(functionDock->btn_sameH, &QPushButton::clicked, this, &MainWindow::on_btn_sameH_clicked);
    connect(functionDock->btn_align, &QPushButton::clicked, this, &MainWindow::on_btn_align_clicked);
    connect(functionDock->btn_align_2, &QPushButton::clicked, this, &MainWindow::on_btn_align_2_clicked);
    connect(functionDock->btn_average, &QPushButton::clicked, this, &MainWindow::on_btn_average_clicked);

    // 计算
    connect(functionDock->pbt_calc, &QPushButton::clicked, this, &MainWindow::on_pbt_calc_clicked);

    // 其他操作
    connect(functionDock->delete_btn, &QPushButton::clicked, this, &MainWindow::on_btn_rm_select_clicked);
    connect(functionDock->align_btn, &QPushButton::clicked, this, &MainWindow::on_btn_align_clicked);
}

void MainWindow::update_object_info()
{
    auto selected_ = House::getInstance().selected.lock();
    if (!selected_) {
        return;
    }

    if (selected_ == paint_area->preview) {
        functionDock->comboBox_type->hide();
    } else {
        functionDock->comboBox_type->show();
    }

    if (auto lay_obj = std::dynamic_pointer_cast<LayObject>(selected_))
    {
        QString type_name = lay_obj->type_name;
        functionDock->comboBox_type->setCurrentText(type_name);
    }

    if (auto rect = std::dynamic_pointer_cast<BaseRectObject>(selected_)) {
        functionDock->spinBox_0->setValue(rect->x());
        functionDock->spinBox_1->setValue(rect->y());
        functionDock->spinBox_2->setValue(rect->width());
        functionDock->spinBox_3->setValue(rect->height());

        // 显示所有spinbox
        functionDock->spinBox_2->setVisible(true);
        functionDock->spinBox_3->setVisible(true);
    } else if (auto wall = std::dynamic_pointer_cast<OutlineWall>(selected_)) {
        if (wall->preview) {
            functionDock->spinBox_0->setValue(wall->preview->x());
            functionDock->spinBox_1->setValue(wall->preview->y());
        } else if (!wall->points.empty()) {
            functionDock->spinBox_0->setValue(wall->points.back().x());
            functionDock->spinBox_1->setValue(wall->points.back().y());
        }

        // 隐藏宽度和高度spinbox
        functionDock->spinBox_2->setVisible(false);
        functionDock->spinBox_3->setVisible(false);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        int key = keyEvent->key();
        if (key == Qt::Key_Control)
            House::getInstance().mode = OperateMode::ADD_SELECT;
        else if (key == Qt::Key_G)
            House::getInstance().mode = OperateMode::DRAG;
        else if (key == Qt::Key_Shift)
            House::getInstance().normal_outline = true;
        if (keyEvent->modifiers() == Qt::ControlModifier && key == Qt::Key_S)
            on_action_save_json_triggered();
        if (keyEvent->modifiers() == Qt::ControlModifier && key == Qt::Key_C)
        {
            auto selected = House::getInstance().selected.lock();
            if (auto obj = std::dynamic_pointer_cast<LayObject>(selected))
            {
                std::shared_ptr<BaseObject> copy_obj = std::make_shared<LayObject>(*obj);
                QPoint copy_obj_pos = copy_obj->pos();

                if (copy_obj_pos.x()+copy_obj->width()+30 < Config::getInstance().paint_area_size)
                    copy_obj_pos.setX(copy_obj_pos.x() + 30);
                else if (copy_obj_pos.x() > 30)
                    copy_obj_pos.setX(copy_obj_pos.x() - 30);
                else
                    copy_obj_pos.setX(Config::getInstance().paint_area_size - copy_obj->width());

                if (copy_obj_pos.y()+copy_obj->height()+30 < Config::getInstance().paint_area_size)
                    copy_obj_pos.setY(copy_obj_pos.y() + 30);
                else if (copy_obj_pos.y() > 30)
                    copy_obj_pos.setY(copy_obj_pos.y() - 30);
                else
                    copy_obj_pos.setY(Config::getInstance().paint_area_size - copy_obj->height());
                copy_obj->move(copy_obj_pos);
                House::getInstance().add(copy_obj);
            }
        }
        return true;
    }
    else if (event->type() == QEvent::KeyRelease)
    {
        int key = static_cast<QKeyEvent*>(event)->key();
        if (key == Qt::Key_Control || key == Qt::Key_G)
            House::getInstance().mode = OperateMode::PAINT;
        else if (key == Qt::Key_Shift)
            House::getInstance().normal_outline = false;
        else if (key == Qt::Key_Delete)
            on_btn_rm_select_clicked();
        else if (key == Qt::Key_Backspace)
        {
            if (paint_area->paint_target == PaintObject::OUTLINE)
            {
                House::getInstance().outline->backspace();
                House::getInstance().outline->completed = false;
                paint_area->update();
            }
            else
            {
                GraphArea::getInstance().remove_select();
            }
        }
        else if (key==Qt::Key_Left || key==Qt::Key_Right || key==Qt::Key_Up || key==Qt::Key_Down)
        {
            int x_delta = 0, y_delta = 0;
            if (key==Qt::Key_Left) x_delta -= Config::getInstance().move_step;
            if (key==Qt::Key_Right) x_delta += Config::getInstance().move_step;
            if (key==Qt::Key_Up) y_delta -= Config::getInstance().move_step;
            if (key==Qt::Key_Down) y_delta += Config::getInstance().move_step;
            House::getInstance().multi_move(QPoint(x_delta, y_delta));
        }
        else if (key == Qt::Key_PageDown)
        {
            file_list.second = (file_list.second + 1) % file_list.first.size();
            import_file(file_list.first[file_list.second], file_list.second);
        }
        else if (key == Qt::Key_PageUp)
        {
            file_list.second = (file_list.second - 1 + file_list.first.size()) % file_list.first.size();
            import_file(file_list.first[file_list.second], file_list.second);
        }
        else if (key == Qt::Key_A)
        {
            House::getInstance().align_selected(true);
        }

        return true;
    }
    else if (watched == functionDock && event->type() == QEvent::Wheel)
    {
        //滚轮调整全局大小
        int delta = static_cast<QWheelEvent*>(event)->angleDelta().y();
        if (delta > 0) {
            House::getInstance().global_scale(Config::getInstance().scale_ratio);
        } else if (delta < 0) {
            House::getInstance().global_scale(1/Config::getInstance().scale_ratio);
        }
        return true;
    }
    else if (event->type() == QEvent::DragEnter)
    {
        QDragEnterEvent *dragEnterEvent = static_cast<QDragEnterEvent*>(event);
        if (dragEnterEvent->mimeData()->hasUrls()) {
            dragEnterEvent->acceptProposedAction();
            return true;
        }
    }
    else if (event->type() == QEvent::Drop)
    {
        QDropEvent *dropEvent = static_cast<QDropEvent*>(event);
        const QMimeData *mimeData = dropEvent->mimeData();
        if (mimeData->hasUrls()) {
            QList<QUrl> urlList = mimeData->urls();
            QString filePath = urlList.at(0).toLocalFile();
            import_file(filePath);
            dropEvent->acceptProposedAction();
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

void MainWindow::import_file(QString path, int idx)
{
    QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "png" || suffix == "jpg" || suffix == "jpeg" || suffix == "bmp" || suffix == "gif")
    {
        paint_area->load_img(path);
    }
    else if (suffix == "json")
    {
        if (idx == -1)
        {
            file_list.first.clear();
            file_list.second = -1;
        }
        bool success = House::getInstance().load_from_json(path);
        paint_area->update();
        ui->metric_0->setText("");
        ui->metric_1->setText("");
        ui->metric_2->setText("");
        ui->metric_3->setText("");
        ui->metric_4->setText("");
        ui->metric_5->setText("");
        if (success)
        {
            cur_json_path = path;
            QString title = QString("[%1/%2] %3").arg(idx).arg(file_list.first.size()).arg(path);
            setWindowTitle(title);
            if (functionDock->auto_calc_check->isChecked())
                on_pbt_calc_clicked();
        }
    }
}

void MainWindow::export_file(QString path)
{
    ui->statusbar->showMessage(QString("objects num: %1, write to %2").arg(House::getInstance().lay_objs.size()).arg(path));
    QString suffix = QFileInfo(path).suffix();
    if (suffix == "png" || suffix == "jpeg" || suffix == "bmp")
    {
        paint_area->save_pix(path);
        bool ret = House::getInstance().export_json(path.replace(suffix, "json"));
        if (!ret)
            QMessageBox::warning(nullptr, "warning", "导出json失败", QMessageBox::Ok);
    }
    else if (suffix == "json")
    {
        House::getInstance().export_json(path);
    }
}

void MainWindow::on_comboBox_currentTextChanged(const QString &arg1)
{
    if (arg1 == Config::getInstance().label_select_mode)
        paint_area->paint_target = PaintObject::NONE;
    else if (arg1 == Config::getInstance().label_outline)
        paint_area->paint_target = PaintObject::OUTLINE;
    else
    {
        for (const auto& pair: Config::getInstance().obj_info)
        {
            if (arg1 == pair.first) {
                paint_area->paint_target = PaintObject::LAY_OBJECT;
                paint_area->paint_lay_obj_name = pair.first;
                break;
            }
        }
    }
}

void MainWindow::on_action_import_triggered()
{
    QString path = QFileDialog::getOpenFileName(this, "Load image", QStandardPaths::writableLocation(QStandardPaths::DesktopLocation), "*.json;; Images(*.png *.jpeg *.jpg *.bmp *.gif)");
    import_file(path);
}

void MainWindow::on_action_export_triggered()
{
    QString path = QFileDialog::getSaveFileName(this, "Save Layout", QStandardPaths::writableLocation(QStandardPaths::DesktopLocation), "*.png;; *.json");
    export_file(path);
}

void MainWindow::on_action_save_json_triggered()
{
    if (cur_json_path == "")
        cur_json_path = QFileDialog::getSaveFileName(this, "Save Layout", QStandardPaths::writableLocation(QStandardPaths::DesktopLocation), "*.json");

    if (cur_json_path != "")
    {
        export_file(cur_json_path);
        setWindowTitle(cur_json_path);
    }
}

void MainWindow::on_action_unload_img_triggered()
{
    paint_area->unload_img();
}

void MainWindow::on_btn_adjust_object_clicked()
{
    auto selected_ = House::getInstance().selected.lock();
    if (!selected_) {
        return;
    }

    // 修改类型
    if (auto obj = std::dynamic_pointer_cast<LayObject>(selected_))
    {
        for (const auto& pair: Config::getInstance().obj_info)
        {
            if (functionDock->comboBox_type->currentText() == pair.first)
            {
                obj->type_name = pair.first;
                obj->setStyleSheet(pair.second.at("style_sheet"));
            }
        }
    }

    // 修改几何属性
    if (auto rect = std::dynamic_pointer_cast<BaseRectObject>(selected_))
    {
        rect->setGeometry(functionDock->spinBox_0->value(),
                          functionDock->spinBox_1->value(),
                          functionDock->spinBox_2->value(),
                          functionDock->spinBox_3->value());
        rect->setSizeF(functionDock->spinBox_2->value(), functionDock->spinBox_3->value());
    }
    else if (auto outline = std::dynamic_pointer_cast<OutlineWall>(selected_))
    {
        if (!outline->points.empty())
        {
            outline->points.back().setX(functionDock->spinBox_0->value());
            outline->points.back().setY(functionDock->spinBox_1->value());
            paint_area->update();
        }
    }
}

void MainWindow::on_btn_rm_select_clicked()
{
    House::getInstance().remove_selected();
}

void MainWindow::on_btn_global_scale_up_clicked()
{
    House::getInstance().global_scale(Config::getInstance().scale_ratio);
}

void MainWindow::on_btn_global_scale_down_clicked()
{
    House::getInstance().global_scale(1/Config::getInstance().scale_ratio);
}

void MainWindow::on_action_clear_triggered()
{
    House::getInstance().clear();
    update();
}

void MainWindow::on_btn_sameW_clicked()
{
    for (auto obj: House::getInstance().lay_objs)
    {
        if (obj->is_selected)
            obj->resize(functionDock->same_w->value(), obj->height());
    }
    update_object_info();
}

void MainWindow::on_btn_sameH_clicked()
{
    for (auto obj: House::getInstance().lay_objs)
    {
        if (obj->is_selected)
            obj->resize(obj->width(), functionDock->same_h->value());
    }
    update_object_info();
}

void MainWindow::on_btn_align_clicked()
{
    House::getInstance().align(functionDock->delta->value(), functionDock->angle->value());
}

void MainWindow::on_action_opendir_triggered()
{
    QString folderPath = QFileDialog::getExistingDirectory(this, tr("选择文件夹"), QDir::homePath(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    QDirIterator it(folderPath, QStringList() << "*.json", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        it.next();
        file_list.first.push_back(it.filePath());
    }
    file_list.second = 0;
    import_file(file_list.first[file_list.second], file_list.second);
}

void MainWindow::object_num_change()
{
    ui->statusbar->showMessage(QString("objects num:%1").arg(House::getInstance().lay_objs.size()));
}

void MainWindow::on_btn_align_2_clicked()
{
    House::getInstance().align_selected(false);
}

void MainWindow::on_btn_average_clicked()
{
    House::getInstance().align_selected(true);
}

void MainWindow::on_pbt_calc_clicked()
{
    House::getInstance().export_json("calc_used.json");
    std::string cmd = "calculation\\.venv\\Scripts\\python.exe calculation\\calc.py calc_used.json";
    std::wstring w_cmd(cmd.begin(), cmd.end());

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcess(
            NULL,
            w_cmd.data(),
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &si,
            &pi)) {
        std::cerr << "CreateProcess failed: " << GetLastError() << std::endl;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    std::remove("calc_used.json");
    std::fstream f_read("results.tmp", std::ios::in);
    if (!f_read.is_open())
    {
        QMessageBox::warning(nullptr, "计算失败", "Python计算程序未生成结果文件！", QMessageBox::Ok);
        return;
    }

    std::string metric_str;
    auto lines = std::vector<QLineEdit*>{ui->metric_0,
                                          ui->metric_1,
                                          ui->metric_2,
                                          ui->metric_3,
                                          ui->metric_4,
                                          ui->metric_5};
    try
    {
        for (int i = 0; i < 6; ++i)
        {
            f_read >> metric_str;
            lines[i]->setText(QString(metric_str.c_str()));
        }
    }
    catch (...)
    {
        QMessageBox::warning(nullptr, "计算失败", "结果文件错误", QMessageBox::Ok);
    }
    f_read.close();
    std::remove("results.tmp");
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::exitFullscreen()
{
    if (isFullScreen()) {
        showNormal();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_F11) {
        toggleFullscreen();
        event->accept();
    } else if (event->key() == Qt::Key_Escape && isFullScreen()) {
        exitFullscreen();
        event->accept();
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::adjustWindowSize()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();

    int width = screenGeometry.width() * 0.8;
    int height = screenGeometry.height() * 0.8;

    resize(width, height);
    move(screenGeometry.width() / 2 - width / 2,
         screenGeometry.height() / 2 - height / 2);
}

void MainWindow::on_control_clicked()
{
    if (functionDock) {
        functionDock->setVisible(!functionDock->isVisible());
    }
}

