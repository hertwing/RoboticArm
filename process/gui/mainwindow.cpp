#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "StyleSheets.h"

#include "InetCommData.h"
#include "odin/video_handler/DataTypes.h"

#include <endian.h> // be64toh
#include <arpa/inet.h> // ntohl/ntohs

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include <QButtonGroup>
#include <QPushButton>
#include <QList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QThread>
#include <QMessageBox>
#include <QPixmap>
#include <QResource>
#include <QStandardPaths>
#include <QPainter>
#include <QPen>
#include <QFile>

#include <opencv2/core.hpp>

using namespace odin::video_handler;

MainWindow::MainWindow(QWidget * parent):
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_diagnostic_board_selected(BoardSelect::GUI),
    m_automatic_line_edit_select(static_cast<std::uint8_t>(AutomaticLineEditSelect::NONE)),
    m_run_in_loop(false),
    m_scripted_motion_files_path(std::filesystem::path(odin::shmem_wrapper::DataTypes::SCRIPTED_MOTION_FILES_PATH))
{
    ui->setupUi(this);

    m_stepsModel = new OdinStepsModel(this);
    ui->table_servo_steps->setModel(m_stepsModel);
    ui->table_servo_steps->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->table_servo_steps->setSelectionMode(QAbstractItemView::SingleSelection);

    auto* tv = ui->table_servo_steps;
    tv->verticalHeader()->setVisible(true);
    tv->verticalHeader()->setFixedWidth(42);
    tv->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);

    auto* idLabel = new QLabel("ID", tv);
    idLabel->setAlignment(Qt::AlignCenter);
    idLabel->setStyleSheet(AUTO_MOTION_ID_COLUMN_STYLE_SHEET);
    tv->setCornerWidget(idLabel);

    auto* hh = tv->horizontalHeader();
    hh->setStretchLastSection(false);
    for (int c = 0; c < OdinStepsModel::Column::ColCount; ++c)
        hh->setSectionResizeMode(c, QHeaderView::Stretch);

    tv->setAlternatingRowColors(true);
    tv->setShowGrid(true);

    auto group = new QButtonGroup(this);
    group->addButton(ui->radioButton_servo_num,   static_cast<int>(AutomaticLineEditSelect::SERVO_NUM));
    group->addButton(ui->radioButton_servo_pos,   static_cast<int>(AutomaticLineEditSelect::SERVO_POS));
    group->addButton(ui->radioButton_servo_speed, static_cast<int>(AutomaticLineEditSelect::SERVO_SPEED));
    group->addButton(ui->radioButton_delay,       static_cast<int>(AutomaticLineEditSelect::DELAY));

    connect(group, &QButtonGroup::idToggled,
            this,  &MainWindow::onRadioGroupToggled);

    ui->lineEdit_servo_num->setValidator(new QIntValidator(1, 6, this));
    ui->lineEdit_servo_pos->setValidator(new QIntValidator(500, 2500, this));
    ui->lineEdit_servo_speed->setValidator(new QIntValidator(1, 10, this));
    ui->lineEdit_delay->setValidator(new QIntValidator(0, 10000, this));

    QMainWindow::showFullScreen();

    // cv::setNumThreads(1);
    m_uiTimer.start();
    const bool cascadeOk = loadFaceCascadeFromResource();

    m_rx = new UdpMjpegReceiver(this);
    connect(m_rx, &UdpMjpegReceiver::frameReady, this, &MainWindow::onFrame);

    // Worker
    if (cascadeOk) {
        m_faceThread = new QThread(this);
        m_CvWorker = new CvWorker(m_cascade_tmp_path);
        m_CvWorker->moveToThread(m_faceThread);
        connect(m_faceThread, &QThread::finished, m_CvWorker, &QObject::deleteLater);
        // Worker output goes back to GUI
        connect(m_CvWorker, &CvWorker::result, this, [this](bool found, const QRect& r){
            m_haveFaceQt = found;
            if (found) m_lastFaceQt = r;
            m_faceBusy = false;
        });
        m_faceThread->start();
    } else {
        std::cout << "[GUI] Face detection disabled (no cascade)" << std::endl;
    }

    m_fullDetectTimer.start();
    
    m_scripted_motion_request_status = std::make_shared<scripted_motion_status_t>(static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::IDLE));
    m_scripted_motion_reply_status = std::make_shared<scripted_motion_status_t>(static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::IDLE));
    m_automatic_steps = std::make_shared<std::vector<OdinServoStep>>();
    
    scan_automatic_files();

    // Shmem readers and writers
    std::cout << "Creating SHMEM readers and writers" << std::endl;
    std::cout << "Creating control selection SHMEM fd." << std::endl;
    m_control_selection_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinControlSelection>>(
        odin::shmem_wrapper::DataTypes::CONTROL_SELECT_SHMEM_NAME, sizeof(OdinControlSelection), true);
    std::cout << "Control selection SHMEM fd created." << std::endl;
    std::cout << "Creating GUI diagnostic data SHMEM fd." << std::endl;
    m_gui_diagnostic_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<DiagnosticData>>(
        odin::shmem_wrapper::DataTypes::DIAGNOSTIC_SHMEM_NAME, sizeof(DiagnosticData), false);
    std::cout << "GUI diagnostic data SHMEM fd created." << std::endl;
    std::cout << "Creating ARM diagnostic data SHMEM fd." << std::endl;
    m_arm_diagnostic_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<DiagnosticData>>(
        odin::shmem_wrapper::DataTypes::DIAGNOSTIC_FROM_REMOTE_SHMEM_NAME, sizeof(DiagnosticData), false);
    std::cout << "ARM diagnostic data SHMEM fd created." << std::endl;

    m_diagnostic_timer = new QTimer(this);
    m_diagnostic_timer->start(300);
    connect(m_diagnostic_timer, SIGNAL(timeout()), this, SLOT(diagnosticTimerSlot()));

    m_control_selection.control_selection = static_cast<std::uint8_t>(ControlSelection::NONE);

    m_motionThread = new QThread;
    m_motionWorker = new ScriptedMotionWorker;

    m_motionWorker->setStatusPointer(m_scripted_motion_request_status);
    m_motionWorker->setStepsVectorPtr(m_automatic_steps);
    m_motionWorker->moveToThread(m_motionThread);

    connect(m_motionWorker, &ScriptedMotionWorker::motionCompleted, this, &MainWindow::handleMotionCompleted);
    connect(this, &MainWindow::stopScriptedMotionRequested, m_motionWorker, &ScriptedMotionWorker::stopMotion);
    connect(m_motionWorker, &ScriptedMotionWorker::destroyed, m_motionThread, &QThread::quit);
    connect(m_motionThread, &QThread::finished, m_motionThread, &QThread::deleteLater);

    m_motionThread->start();
    
    m_charts = new ChartsPanel(
    ui->chart_view_cpu_usage,
    ui->chart_view_ram_usage,
    ui->chart_view_cpu_temp,
    ui->chart_view_latency,
    this);
    m_charts->setSnapshot(m_diagnostic_data);

    draw_menu();

    const QList<QPushButton*> digitBtns = {
        ui->button_0, ui->button_1, ui->button_2, ui->button_3, ui->button_4,
        ui->button_5, ui->button_6, ui->button_7, ui->button_8, ui->button_9
    };
    for (auto* b : digitBtns) connect(b, &QPushButton::clicked, this, &MainWindow::handleDigitButtonClicked);

    m_line_edits = {
        ui->lineEdit_servo_num,
        ui->lineEdit_servo_pos,
        ui->lineEdit_servo_speed,
        ui->lineEdit_delay
    };
}

MainWindow::~MainWindow()
{    
    if (m_faceThread) 
    {
        m_faceThread->quit();
        m_faceThread->wait();
    }
    if (m_motionWorker) m_motionWorker->stopMotion();

    if (m_motionThread)
    {
        m_motionThread->quit();
        m_motionThread->wait();
    }
    delete m_diagnostic_timer;

    delete ui;
}

void MainWindow::draw_menu()
{
    // Buttons
    ui->button_joypad->setStyleSheet(DISABLED_JOYPAD_STYLE_SHEET);
    ui->button_automatic->setStyleSheet(DISABLED_AUTOMATIC_STYLE_SHEET);
    ui->button_camera->setStyleSheet(DISABLED_CAMERA_STYLE_SHEET);
    ui->button_diagnostic->setStyleSheet(DISABLED_DIAGNOSTIC_STYLE_SHEET);
    ui->button_exit->setStyleSheet(BUTTON_EXIT_STYLE_SHEET);
    ui->button_rpi_switch->setStyleSheet(BUTTON_RPI_SWITCH_GUI_STYLE_SHEET);
    // Widgets
    ui->stackedWidget->setCurrentIndex(static_cast<int>(WidgetPage::MAIN));
    ui->widget_cpu_temp->setStyleSheet(DIAGNOSTIC_WIDGET_STYLE_SHEET);
    ui->widget_cpu_temp_chart->setStyleSheet(DIAGNOSTIC_CHART_WIDGET_STYLE_SHEET);
    ui->widget_cpu_usage->setStyleSheet(DIAGNOSTIC_WIDGET_STYLE_SHEET);
    ui->widget_cpu_usage_chart->setStyleSheet(DIAGNOSTIC_CHART_WIDGET_STYLE_SHEET);
    ui->widget_ram_usage->setStyleSheet(DIAGNOSTIC_WIDGET_STYLE_SHEET);
    ui->widget_ram_usage_chart->setStyleSheet(DIAGNOSTIC_CHART_WIDGET_STYLE_SHEET);
    ui->widget_latency->setStyleSheet(DIAGNOSTIC_WIDGET_STYLE_SHEET);
    ui->widget_latency_chart->setStyleSheet(DIAGNOSTIC_CHART_WIDGET_STYLE_SHEET);
    // Labels
    ui->label_cpu_usage->setStyleSheet(DIAGNOSTIC_LABEL_STYLE_SHEET);
    ui->label_cpu_temp->setStyleSheet(DIAGNOSTIC_LABEL_STYLE_SHEET);
    ui->label_ram_usage->setStyleSheet(DIAGNOSTIC_LABEL_STYLE_SHEET);
    ui->label_latency->setStyleSheet(DIAGNOSTIC_LABEL_STYLE_SHEET);
    ui->label_rpi_switch->setText("GUI board diagnostic data");
}

void MainWindow::switchMode(UIMode m)
{
    if (m == m_mode) return;

    if (m == UIMode::DIAGNOSTIC) {
        if (m_diagnostic_timer) m_diagnostic_timer->start(300);
    } else {
        if (m_diagnostic_timer) m_diagnostic_timer->stop();
    }

    switch (m) {
        case UIMode::MAIN:       ui->stackedWidget->setCurrentIndex(static_cast<int>(WidgetPage::MAIN)); break;
        case UIMode::JOYPAD:     ui->stackedWidget->setCurrentIndex(static_cast<int>(WidgetPage::JOYPAD)); break;
        case UIMode::CAMERA:     ui->stackedWidget->setCurrentIndex(static_cast<int>(WidgetPage::CAMERA)); break;
        case UIMode::DIAGNOSTIC: ui->stackedWidget->setCurrentIndex(static_cast<int>(WidgetPage::DIAGNOSTIC)); break;
        case UIMode::AUTOMATIC:  ui->stackedWidget->setCurrentIndex(static_cast<int>(WidgetPage::AUTOMATIC)); break;
    }

    auto setOn  = [](QPushButton* b, const QString& on){ b->setStyleSheet(on);  };
    auto setOff = [](QPushButton* b, const QString& off){ b->setStyleSheet(off);};

    setOff(ui->button_joypad,     DISABLED_JOYPAD_STYLE_SHEET);
    setOff(ui->button_camera,     DISABLED_CAMERA_STYLE_SHEET);
    setOff(ui->button_diagnostic, DISABLED_DIAGNOSTIC_STYLE_SHEET);
    setOff(ui->button_automatic,  DISABLED_AUTOMATIC_STYLE_SHEET);

    switch (m) {
        case UIMode::JOYPAD:     setOn(ui->button_joypad,     ENABLED_JOYPAD_STYLE_SHEET);     break;
        case UIMode::CAMERA:     setOn(ui->button_camera,     ENABLED_CAMERA_STYLE_SHEET);     break;
        case UIMode::DIAGNOSTIC: setOn(ui->button_diagnostic, ENABLED_DIAGNOSTIC_STYLE_SHEET); break;
        case UIMode::AUTOMATIC:  setOn(ui->button_automatic,  ENABLED_AUTOMATIC_STYLE_SHEET);  break;
        default: break;
    }

    switch (m) {
        case UIMode::JOYPAD:     m_control_selection.control_selection = static_cast<std::uint8_t>(ControlSelection::JOYPAD); break;
        case UIMode::CAMERA:     m_control_selection.control_selection = static_cast<std::uint8_t>(ControlSelection::CAMERA); break;
        case UIMode::DIAGNOSTIC: m_control_selection.control_selection = static_cast<std::uint8_t>(ControlSelection::DIAGNOSTIC); break;
        case UIMode::AUTOMATIC:  m_control_selection.control_selection = static_cast<std::uint8_t>(ControlSelection::AUTOMATIC); break;
        case UIMode::MAIN:       m_control_selection.control_selection = static_cast<std::uint8_t>(ControlSelection::NONE); break;
    }
    m_control_selection_shmem_handler->shmemWrite(&m_control_selection);

    m_mode = m;
}

void MainWindow::on_button_exit_clicked()
{
    MainWindow::close();
}

void MainWindow::on_button_joypad_clicked()     { switchMode(m_mode == UIMode::JOYPAD     ? UIMode::MAIN : UIMode::JOYPAD); }
void MainWindow::on_button_camera_clicked()     { switchMode(m_mode == UIMode::CAMERA     ? UIMode::MAIN : UIMode::CAMERA); }
void MainWindow::on_button_diagnostic_clicked() { switchMode(m_mode == UIMode::DIAGNOSTIC ? UIMode::MAIN : UIMode::DIAGNOSTIC); }
void MainWindow::on_button_automatic_clicked()  { switchMode(m_mode == UIMode::AUTOMATIC  ? UIMode::MAIN : UIMode::AUTOMATIC); }

void MainWindow::on_button_rpi_switch_clicked()
{
    m_diagnostic_board_selected =
        (m_diagnostic_board_selected == BoardSelect::GUI) ? BoardSelect::ARM : BoardSelect::GUI;

    ui->button_rpi_switch->setStyleSheet(
        (m_diagnostic_board_selected == BoardSelect::ARM) ? BUTTON_RPI_SWITCH_ARM_STYLE_SHEET
                                                          : BUTTON_RPI_SWITCH_GUI_STYLE_SHEET);
    ui->label_rpi_switch->setText(
        (m_diagnostic_board_selected == BoardSelect::ARM) ? "ARM board diagnostic data"
                                                          : "GUI board diagnostic data");

    DiagnosticData snap{};
    if (readDiagnosticOnceFor(m_diagnostic_board_selected, snap))
    {
        if (m_charts) m_charts->setSnapshot(snap);
    } 
    else 
    {
        if (m_charts) m_charts->setSnapshot(DiagnosticData{});
    }
}

void MainWindow::disable_buttons()
{
    ui->button_joypad->setStyleSheet(DISABLED_JOYPAD_STYLE_SHEET);
    ui->button_diagnostic->setStyleSheet(DISABLED_DIAGNOSTIC_STYLE_SHEET);
    ui->button_automatic->setStyleSheet(DISABLED_AUTOMATIC_STYLE_SHEET);
}

void MainWindow::diagnosticTimerSlot()
{
    if (m_diagnostic_board_selected == BoardSelect::GUI)
    {
        if (m_gui_diagnostic_shmem_handler->openShmem())
        {
            if (!m_gui_diagnostic_shmem_handler->shmemRead(&m_diagnostic_data))
            {
                std::cout << "Error while reading from gui diagnostic SHMEM" << std::endl;
            }
        }
        else
        {
            m_diagnostic_data.cpu_usage = 0;
            m_diagnostic_data.ram_usage = 0;
            m_diagnostic_data.cpu_temp = 0;
            m_diagnostic_data.latency = 0;
        }
    }
    else
    {
        if (m_arm_diagnostic_shmem_handler->openShmem())
        {
            if (!m_arm_diagnostic_shmem_handler->shmemRead(&m_diagnostic_data))
            {
                std::cout << "Error while reading from arm diagnostic SHMEM" << std::endl;
            }
        }
        else
        {
            m_diagnostic_data.cpu_usage = 0;
            m_diagnostic_data.ram_usage = 0;
            m_diagnostic_data.cpu_temp = 0;
            m_diagnostic_data.latency = 0;
        }
    }

    // Draw charts
    if (m_charts) m_charts->push(m_diagnostic_data);
}

bool MainWindow::readDiagnosticOnceFor(BoardSelect src, DiagnosticData& out)
{
    auto* h = (src == BoardSelect::GUI) ? m_gui_diagnostic_shmem_handler.get()
                                        : m_arm_diagnostic_shmem_handler.get();
    if (!h->openShmem())
    {
        out = {}; return false;
    }
    if (!h->shmemRead(&out))
    {
        out = {}; return false;
    }
    return true;
}

void MainWindow::on_button_clear_clicked()
{
    switch(m_automatic_line_edit_select)
    {
    case static_cast<std::uint8_t>(AutomaticLineEditSelect::SERVO_NUM):
        ui->lineEdit_servo_num->clear();
        break;
    case static_cast<std::uint8_t>(AutomaticLineEditSelect::SERVO_POS):
        ui->lineEdit_servo_pos->clear();
        break;
    case static_cast<std::uint8_t>(AutomaticLineEditSelect::SERVO_SPEED):
        ui->lineEdit_servo_speed->clear();
        break;
    case static_cast<std::uint8_t>(AutomaticLineEditSelect::DELAY):
        ui->lineEdit_delay->clear();
        break;
    default:
        break;
    }
}

void MainWindow::on_button_del_clicked()
{
    switch(m_automatic_line_edit_select)
    {
    case static_cast<std::uint8_t>(AutomaticLineEditSelect::SERVO_NUM):
    {
        QString current_text = ui->lineEdit_servo_num->text();
        current_text.chop(1);
        ui->lineEdit_servo_num->setText(current_text);
        break;
    }
    case static_cast<std::uint8_t>(AutomaticLineEditSelect::SERVO_POS):
    {
        QString current_text = ui->lineEdit_servo_pos->text();
        current_text.chop(1);
        ui->lineEdit_servo_pos->setText(current_text);
        break;
    }
    case static_cast<std::uint8_t>(AutomaticLineEditSelect::SERVO_SPEED):
    {
        QString current_text = ui->lineEdit_servo_speed->text();
        current_text.chop(1);
        ui->lineEdit_servo_speed->setText(current_text);
        break;
    }
    case static_cast<std::uint8_t>(AutomaticLineEditSelect::DELAY):
    {
        QString current_text = ui->lineEdit_delay->text();
        current_text.chop(1);
        ui->lineEdit_delay->setText(current_text);
        break;
    }
    default:
        break;
    }
}

void MainWindow::handleDigitButtonClicked()
{
    auto* b = qobject_cast<QPushButton*>(sender());
    if (!b) return;
    const QChar ch = b->text().isEmpty() ? QChar() : b->text().at(0);
    if (ch.isNull()) return;
    handle_num_buttons(ch.toLatin1());
}

void MainWindow::handle_num_buttons(char num)
{
    switch(m_automatic_line_edit_select)
    {
    case static_cast<std::uint8_t>(AutomaticLineEditSelect::SERVO_NUM):
    {
        QString current_text = ui->lineEdit_servo_num->text();
        current_text.append(num);
        ui->lineEdit_servo_num->setText(current_text);
        break;
    }
    case static_cast<std::uint8_t>(AutomaticLineEditSelect::SERVO_POS):
    {
        QString current_text = ui->lineEdit_servo_pos->text();
        current_text.append(num);
        ui->lineEdit_servo_pos->setText(current_text);
        break;
    }
    case static_cast<std::uint8_t>(AutomaticLineEditSelect::SERVO_SPEED):
    {
        QString current_text = ui->lineEdit_servo_speed->text();
        current_text.append(num);
        ui->lineEdit_servo_speed->setText(current_text);
        break;
    }
    case static_cast<std::uint8_t>(AutomaticLineEditSelect::DELAY):
    {
        QString current_text = ui->lineEdit_delay->text();
        current_text.append(num);
        ui->lineEdit_delay->setText(current_text);
        break;
    }
    default:
        break;
    }
}

void MainWindow::on_button_add_step_clicked()
{
    bool all_ok = true;
    for (const auto line_edit : m_line_edits)
    {
        if (!line_edit->hasAcceptableInput())
        {
            line_edit->setStyleSheet(LINE_EDIT_ERROR_STYLE_SHEET);
            all_ok = false;
        }
        else
        {
            line_edit->setStyleSheet(LINE_EDIT_SUCCESS_STYLE_SHEET);
        }
    }

    if (!all_ok) return;

    OdinServoStep s;
    s.step_num = (uint64_t)m_stepsModel->rowCount();
    s.servo_num = ui->lineEdit_servo_num->text().toInt();
    s.position  = ui->lineEdit_servo_pos->text().toInt();
    s.speed     = ui->lineEdit_servo_speed->text().toInt();
    s.delay     = ui->lineEdit_delay->text().toInt();

    m_stepsModel->addStep(s);
    m_automatic_steps->push_back(s);
    clear_line_edits();

    for (auto s = m_automatic_steps->begin(); s != m_automatic_steps->end(); ++s)
    {
        std::cout << +s->step_num << std::endl;
        std::cout << +s->servo_num << std::endl;
        std::cout << +s->position << std::endl;
        std::cout << +s->speed << std::endl;
        std::cout << +s->delay << std::endl;
    }
    std::cout << "---" << std::endl;
}

void MainWindow::clear_line_edits()
{
    for (const auto line_edit : m_line_edits)
    {
        line_edit->clear();
        line_edit->setStyleSheet("");
    }
}

void MainWindow::on_button_remove_step_clicked() {
    const int row = ui->lineEdit_step_number->text().toInt() - 1;
    if (row >= 0 && row < m_stepsModel->rowCount())
    {
        m_stepsModel->removeRowAt(row);
        if (row < (int)m_automatic_steps->size())
            m_automatic_steps->erase(m_automatic_steps->begin() + row);
        ui->lineEdit_step_number->clear();
    }
}

void MainWindow::on_button_table_clear_clicked() {
    m_stepsModel->clear();
    m_automatic_steps->clear();
}

void MainWindow::on_button_save_clicked() {
    try {
        if (!std::filesystem::exists(m_scripted_motion_files_path))
            std::filesystem::create_directories(m_scripted_motion_files_path);

        const std::string file_name = ui->lineEdit_file_name->text().toStdString();
        if (file_name.empty()) return;

        std::ofstream file((m_scripted_motion_files_path / file_name).string());
        for (const auto& s : *m_automatic_steps) {
            file << s.servo_num << "\n"
                 << s.position  << "\n"
                 << s.speed     << "\n"
                 << s.delay     << "\n";
        }
        ui->lineEdit_file_name->clear();
        scan_automatic_files();
    } catch (const std::exception& e) { std::cout << e.what() << std::endl; }
}

void MainWindow::on_button_load_clicked() {
    try {
        m_stepsModel->clear();
        m_automatic_steps->clear();

        if (!std::filesystem::exists(m_scripted_motion_files_path)) return;

        const auto* item = ui->list_automatic_files->currentItem();
        if (!item) return;

        std::ifstream file((m_scripted_motion_files_path / item->text().toStdString()).string());
        for (;;) {
            OdinServoStep s{};
            if (!(file >> s.servo_num)) break;
            if (!(file >> s.position))  break;
            if (!(file >> s.speed))     break;
            if (!(file >> s.delay))     break;
            s.step_num = (uint64_t)m_stepsModel->rowCount();
            m_stepsModel->addStep(s);
            m_automatic_steps->push_back(s);
        }
    } catch (const std::exception& e) { std::cout << e.what() << std::endl; }
}

void MainWindow::scan_automatic_files()
{
    try
    {
        if (std::filesystem::exists(m_scripted_motion_files_path))
        {
            ui->list_automatic_files->clear();
            for (const auto & entry : std::filesystem::directory_iterator(m_scripted_motion_files_path))
            {
                ui->list_automatic_files->addItem(QString::fromStdString(entry.path().filename().string()));
            }
        }
    }
    catch (const std::exception & error)
    {
        std::cout << error.what() << std::endl;
    }
}

void MainWindow::onRadioGroupToggled(int id, bool checked)
{
    if (!checked) return;
    m_automatic_line_edit_select = static_cast<std::uint8_t>(id);
}

void MainWindow::on_radioButton_loop_toggled(bool checked)
{
    checked ? m_run_in_loop = true : m_run_in_loop = false;

    m_motionWorker->setRunInLoop(m_run_in_loop);

    std::cout << "run in loop: " << m_run_in_loop << std::endl;
}

void MainWindow::on_button_execute_clicked()
{
    ui->button_execute->setEnabled(false);

    *m_scripted_motion_request_status = static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::START_REQUEST);

    QMetaObject::invokeMethod(m_motionWorker, "processMotion", Qt::QueuedConnection);
}

void MainWindow::handleMotionCompleted()
{
    std::cout << "[OdinGui] Motion request completed." << std::endl;
    *m_scripted_motion_request_status = static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::IDLE);
    ui->button_execute->setEnabled(true);
}

void MainWindow::onFrame(const QImage& img)
{
    if (m_uiTimer.elapsed() < m_uiMinIntervalMs) return;
    m_uiTimer.restart();

    if (img.isNull()) return;

    // display clean frame immediately
    QImage frame = (img.format() == QImage::Format_RGB888)
                     ? img
                     : img.convertToFormat(QImage::Format_RGB888);

    // paint last known bbox
    if (m_haveFaceQt)
    {
        QPainter p(&frame);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(QColor(0, 200, 0)); pen.setWidth(3);
        p.setPen(pen);
        p.drawRect(m_lastFaceQt);
    }

    // async detection, only when worker is free
    if (m_CvWorker && !m_faceBusy)
    {
        m_faceBusy = true;
        const bool forceFull = (m_fullDetectTimer.elapsed() >= m_fullDetectMs) || !m_haveFaceQt;
        if (forceFull) m_fullDetectTimer.restart();

        // process in worker thread
        QMetaObject::invokeMethod(m_CvWorker, "process",
                                  Qt::QueuedConnection,
                                  Q_ARG(QImage, frame),
                                  Q_ARG(bool, forceFull));
    }

    // send to label
    const QPixmap pm = QPixmap::fromImage(frame).scaled(
        ui->camera_label->size(),
        Qt::KeepAspectRatio,
        Qt::FastTransformation);
    ui->camera_label->setPixmap(pm);
}

bool MainWindow::loadFaceCascadeFromResource()
{
    const QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + "/haarcascade_frontalface_default.xml";
    m_cascade_tmp_path = tmp.toStdString();

    QFile f(":/data/haarcascade_frontalface_default.xml");
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "[GUI] No cascade in resources (:/data/haarcascade_frontalface_default.xml)";
        return false;
    }
    const QByteArray xml = f.readAll();
    f.close();

    std::ofstream ofs(m_cascade_tmp_path, std::ios::binary | std::ios::trunc);
    if (!ofs) {
        qWarning() << "[GUI] Tmp cascade write failed:" << tmp;
        return false;
    }
    ofs.write(xml.constData(), static_cast<std::streamsize>(xml.size()));
    ofs.close();

    // sanity check
    if (!std::filesystem::exists(m_cascade_tmp_path) ||
        std::filesystem::file_size(m_cascade_tmp_path) == 0) {
        qWarning() << "[GUI] Cascade file missing/empty:" << tmp;
        return false;
    }
    return true;
}

void MainWindow::on_button_stop_clicked()
{
    *m_scripted_motion_request_status = static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::STOP_REQUESTED);
    emit stopScriptedMotionRequested();
}
