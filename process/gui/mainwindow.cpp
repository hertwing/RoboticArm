#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include <QList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_joypad_enabled(false),
    m_automatic_enabled(false),
    m_diagnostic_enabled(false),
    m_diagnostic_board_selected(static_cast<bool>(BoardSelect::GUI)),
    m_chart_swap(0),
    m_automatic_line_edit_select(static_cast<std::uint8_t>(AutomaticLineEditSelect::NONE)),
    m_is_servo_num_valid(false),
    m_is_servo_pos_valid(false),
    m_is_servo_speed_valid(false),
    m_is_delay_valid(false),
    m_run_in_loop(false),
    m_automatic_file_path(std::filesystem::path(odin::shmem_wrapper::DataTypes::AUTOMATIC_FILES_PATH))
{
    ui->setupUi(this);

    scan_automatic_files();

    m_automatic_steps_count = ui->table_servo_steps->rowCount();
    // Shmem readers and writers
    m_automatic_step_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinServoStep>>(
        odin::shmem_wrapper::DataTypes::AUTOMATIC_STEP_SHMEM_NAME, sizeof(OdinServoStep), true);
    m_control_selection_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinControlSelection>>(
        odin::shmem_wrapper::DataTypes::CONTROL_SELECT_SHMEM_NAME, sizeof(OdinControlSelection), true);
    m_automatic_execute_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinAutomaticExecuteData>>(
        odin::shmem_wrapper::DataTypes::AUTOMATIC_EXECUTE_SHMEM_NAME, sizeof(OdinAutomaticExecuteData), true);
    m_gui_diagnostic_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<DiagnosticData>>(
        odin::shmem_wrapper::DataTypes::DIAGNOSTIC_SHMEM_NAME, sizeof(DiagnosticData), false);
    m_arm_diagnostic_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<DiagnosticData>>(
        odin::shmem_wrapper::DataTypes::DIAGNOSTIC_FROM_REMOTE_SHMEM_NAME, sizeof(DiagnosticData), false);
    m_automatic_execute_confirm_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinAutomaticConfirm>>(
        odin::shmem_wrapper::DataTypes::AUTOMATIC_EXECUTE_GATEWAY_CONFIRM_SHMEM_NAME, sizeof(OdinAutomaticConfirm), false);
    m_automatic_step_confirm_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinAutomaticStepConfirm>>(
        odin::shmem_wrapper::DataTypes::AUTOMATIC_EXECUTE_STEP_CONFIRM_SHMEM_NAME, sizeof(OdinAutomaticStepConfirm), false);

    // Fill shmem data with initial values
    // TODO: write a method for that
    OdinServoStep tmp_step;
    m_automatic_step_shmem_handler->shmemWrite(&tmp_step);

    m_diagnostic_timer = new QTimer(this);
    m_diagnostic_timer->start(300);
    connect(m_diagnostic_timer, SIGNAL(timeout()), this, SLOT(diagnosticTimerSlot()));

    m_control_selection.control_selection = static_cast<std::uint8_t>(ControlSelection::NONE);

    draw_charts();
    draw_menu();
}

MainWindow::~MainWindow()
{
    const auto * cpu_usage_chart_ptr = ui->chart_view_cpu_usage->chart();
    const auto * ram_usage_chart_ptr = ui->chart_view_ram_usage->chart();
    const auto * cpu_temp_chart_ptr = ui->chart_view_cpu_temp->chart();
    const auto * latency_chart_ptr = ui->chart_view_latency->chart();

    delete ui;

    delete cpu_usage_chart_ptr;
    delete ram_usage_chart_ptr;
    delete cpu_temp_chart_ptr;
    delete latency_chart_ptr;
    delete m_diagnostic_timer;
}

void MainWindow::draw_menu()
{
    // Buttons
    ui->button_joypad->setStyleSheet(m_disabled_joypad_style_sheet);
    ui->button_automatic->setStyleSheet(m_disabled_automatic_style_sheet);
    ui->button_diagnostic->setStyleSheet(m_disabled_diagnostic_style_sheet);
    ui->button_exit->setStyleSheet(m_button_exit_style_sheet);
    ui->button_rpi_switch->setStyleSheet(m_button_rpi_switch_gui_style_sheet);
    // Widgets
    ui->stackedWidget->setCurrentIndex(static_cast<int>(WidgetPage::MAIN));
    ui->widget_cpu_temp->setStyleSheet(m_diagnostic_widget_style_sheet);
    ui->widget_cpu_temp_chart->setStyleSheet(m_diagnostic_chart_widget_style_sheet);
    ui->widget_cpu_usage->setStyleSheet(m_diagnostic_widget_style_sheet);
    ui->widget_cpu_usage_chart->setStyleSheet(m_diagnostic_chart_widget_style_sheet);
    ui->widget_ram_usage->setStyleSheet(m_diagnostic_widget_style_sheet);
    ui->widget_ram_usage_chart->setStyleSheet(m_diagnostic_chart_widget_style_sheet);
    ui->widget_latency->setStyleSheet(m_diagnostic_widget_style_sheet);
    ui->widget_latency_chart->setStyleSheet(m_diagnostic_chart_widget_style_sheet);
    // Lavbels
    ui->label_cpu_usage->setStyleSheet(m_diagnostic_label_style_sheet);
    ui->label_cpu_temp->setStyleSheet(m_diagnostic_label_style_sheet);
    ui->label_ram_usage->setStyleSheet(m_diagnostic_label_style_sheet);
    ui->label_latency->setStyleSheet(m_diagnostic_label_style_sheet);
    ui->label_rpi_switch->setText("GUI board diagnostic data");
    // Charts
    QMainWindow::showFullScreen();
}

void MainWindow::on_button_exit_clicked()
{
    MainWindow::close();
}

void MainWindow::on_button_joypad_clicked()
{
    // Turn off automatic
    if (m_automatic_enabled)
    {
        m_automatic_enabled = false;
        ui->button_automatic->setStyleSheet(m_disabled_automatic_style_sheet);
    }
    // Turn off diagnostic
    if (m_diagnostic_enabled)
    {
        m_diagnostic_enabled = false;
        ui->button_diagnostic->setStyleSheet(m_disabled_diagnostic_style_sheet);
    }

    m_joypad_enabled = !m_joypad_enabled;
    m_joypad_enabled ? m_control_selection.control_selection = static_cast<std::uint8_t>(ControlSelection::JOYPAD) : m_control_selection.control_selection = static_cast<std::uint8_t>(ControlSelection::NONE);
    m_control_selection_shmem_handler->shmemWrite(&m_control_selection);
    m_joypad_enabled ? show_joypad() : hide_joypad();
}

void MainWindow::on_button_diagnostic_clicked()
{
    // TODO: Rewrite to not turn off control option
    // Turn off joypad
    if (m_joypad_enabled)
    {
        m_joypad_enabled = false;
        ui->button_joypad->setStyleSheet(m_disabled_joypad_style_sheet);
    }
    // Turn off automatic
    if (m_automatic_enabled)
    {
        m_automatic_enabled = false;
        ui->button_automatic->setStyleSheet(m_disabled_automatic_style_sheet);
    }

    m_diagnostic_enabled = !m_diagnostic_enabled;
    m_diagnostic_enabled ? m_control_selection.control_selection = static_cast<std::uint8_t>(ControlSelection::DIAGNOSTIC) : m_control_selection.control_selection = static_cast<std::uint8_t>(ControlSelection::NONE);
    m_control_selection_shmem_handler->shmemWrite(&m_control_selection);
    m_diagnostic_enabled ? show_diagnostics() : hide_diagnostics();
}

void MainWindow::on_button_automatic_clicked()
{
    // Turn off joypad
    if (m_joypad_enabled)
    {
        m_joypad_enabled = false;
        ui->button_joypad->setStyleSheet(m_disabled_joypad_style_sheet);
    }
    // Turn off diagnostic
    if (m_diagnostic_enabled)
    {
        m_diagnostic_enabled = false;
        ui->button_diagnostic->setStyleSheet(m_disabled_diagnostic_style_sheet);
    }

    m_automatic_enabled = !m_automatic_enabled;
    m_automatic_enabled ? m_control_selection.control_selection = static_cast<std::uint8_t>(ControlSelection::AUTOMATIC) : m_control_selection.control_selection = static_cast<std::uint8_t>(ControlSelection::NONE);
    m_control_selection_shmem_handler->shmemWrite(&m_control_selection);
    m_automatic_enabled ? show_automatic() : hide_automatic();
}

void MainWindow::on_button_rpi_switch_clicked()
{
    m_diagnostic_board_selected = !m_diagnostic_board_selected;
    m_diagnostic_board_selected ? ui->button_rpi_switch->setStyleSheet(m_button_rpi_switch_arm_style_sheet) : ui->button_rpi_switch->setStyleSheet(m_button_rpi_switch_gui_style_sheet);
    m_diagnostic_board_selected ? ui->label_rpi_switch->setText("ARM board diagnostic data") : ui->label_rpi_switch->setText("GUI board diagnostic data");

    // Zero chart bins
    for (std::uint8_t i = 0; i < INT_CHARTS_COUNT; ++i)
    {
        for (std::uint8_t j = 0; j < CHART_BINS; ++j)
        {
            m_charts_data[i][j] = 0;
        }
    }
}

void MainWindow::disable_buttons()
{
    ui->button_joypad->setStyleSheet(m_disabled_joypad_style_sheet);
    ui->button_diagnostic->setStyleSheet(m_disabled_diagnostic_style_sheet);
    ui->button_automatic->setStyleSheet(m_disabled_automatic_style_sheet);
}

void MainWindow::show_joypad()
{
    ui->stackedWidget->show();
    ui->stackedWidget->setCurrentIndex(static_cast<int>(WidgetPage::JOYPAD));
    ui->button_joypad->setStyleSheet(m_enabled_joypad_style_sheet);
}

void MainWindow::hide_joypad()
{
    ui->stackedWidget->setCurrentIndex(static_cast<int>(WidgetPage::MAIN));
    ui->button_joypad->setStyleSheet(m_disabled_joypad_style_sheet);
}

void MainWindow::show_diagnostics()
{
    ui->stackedWidget->show();
    ui->stackedWidget->setCurrentIndex(static_cast<int>(WidgetPage::DIAGNOSTIC));
    ui->button_diagnostic->setStyleSheet(m_enabled_diagnostic_style_sheet);
}

void MainWindow::hide_diagnostics()
{
    ui->stackedWidget->setCurrentIndex(static_cast<int>(WidgetPage::MAIN));
    ui->button_diagnostic->setStyleSheet(m_disabled_diagnostic_style_sheet);
}

void MainWindow::show_automatic()
{
    ui->stackedWidget->setCurrentIndex(static_cast<int>(WidgetPage::AUTOMATIC));
    ui->button_automatic->setStyleSheet(m_enabled_automatic_style_sheet);
}

void MainWindow::hide_automatic()
{
    ui->stackedWidget->setCurrentIndex(static_cast<int>(WidgetPage::MAIN));
    ui->button_automatic->setStyleSheet(m_disabled_automatic_style_sheet);
}

void MainWindow::diagnosticTimerSlot()
{
    if (!m_diagnostic_board_selected)
    {
        if (m_gui_diagnostic_shmem_handler->openShmem())
        {
            m_gui_diagnostic_shmem_handler->shmemRead(&m_gui_diagnostic_data);
        }
        else
        {
            m_gui_diagnostic_data.cpu_usage = 0;
            m_gui_diagnostic_data.ram_usage = 0;
            m_gui_diagnostic_data.cpu_temp = 0;
            m_gui_diagnostic_data.latency = 0;
        }
    }
    else
    {
        if (m_arm_diagnostic_shmem_handler->openShmem())
        {
            m_arm_diagnostic_shmem_handler->shmemRead(&m_gui_diagnostic_data);
        }
        else
        {
            m_gui_diagnostic_data.cpu_usage = 0;
            m_gui_diagnostic_data.ram_usage = 0;
            m_gui_diagnostic_data.cpu_temp = 0;
            m_gui_diagnostic_data.latency = 0;
        }
    }

    // Fill charts bins with data
    const auto * temp_charts_data = m_charts_data;
    for (std::uint8_t i = 0; i < INT_CHARTS_COUNT; ++i)
    {
        for (std::uint8_t j = 0; j < CHART_BINS-1; ++j)
        {
            m_charts_data[i][j] = temp_charts_data[i][j+1];
        }
    }
    // Get latest data to last char bin
    m_charts_data[static_cast<std::uint32_t>(ChartSelect::CPU_USAGE)][CHART_BINS-1] = m_gui_diagnostic_data.cpu_usage;
    m_charts_data[static_cast<std::uint32_t>(ChartSelect::RAM_USAGE)][CHART_BINS-1] = m_gui_diagnostic_data.ram_usage;
    m_charts_data[static_cast<std::uint32_t>(ChartSelect::CPU_TEMP)][CHART_BINS-1] = m_gui_diagnostic_data.cpu_temp;
    m_latency_chart_data = m_gui_diagnostic_data.latency;

    // Draw charts
    draw_charts();
}

void MainWindow::draw_charts()
{
    m_chart_swap = !m_chart_swap;

    m_cpu_usage_chart[m_chart_swap] = new QChart();
    m_ram_usage_chart[m_chart_swap] = new QChart();
    m_cpu_temp_chart[m_chart_swap] = new QChart();
    m_latency_chart[m_chart_swap] = new QChart();
    m_latency_chart[m_chart_swap]->setTitle(QString::number(m_latency_chart_data, 'f', 2) + "ms");

    m_cpu_usage_axis_x[m_chart_swap] = new QBarCategoryAxis();
    m_ram_usage_axis_x[m_chart_swap] = new QBarCategoryAxis();
    m_cpu_temp_axis_x[m_chart_swap] = new QBarCategoryAxis();

    m_cpu_usage_axis_x[m_chart_swap]->append(QString::number(+m_charts_data[static_cast<std::uint32_t>(ChartSelect::CPU_USAGE)][CHART_BINS-1]) + "%");
    m_ram_usage_axis_x[m_chart_swap]->append(QString::number(+m_charts_data[static_cast<std::uint32_t>(ChartSelect::RAM_USAGE)][CHART_BINS-1])+ "%");
    m_cpu_temp_axis_x[m_chart_swap]->append(QString::number(+m_charts_data[static_cast<std::uint32_t>(ChartSelect::CPU_TEMP)][CHART_BINS-1]) + "°C");

    for(int i=0; i<CHART_BINS; ++i)
    {
        m_cpu_usage_set[m_chart_swap][i] = new QBarSet("");
        m_ram_usage_set[m_chart_swap][i] = new QBarSet("");
        m_cpu_usage_set[m_chart_swap][i]->append(m_charts_data[static_cast<std::uint32_t>(ChartSelect::CPU_USAGE)][i]);
        m_ram_usage_set[m_chart_swap][i]->append(m_charts_data[static_cast<std::uint32_t>(ChartSelect::RAM_USAGE)][i]);
        m_cpu_usage_set[m_chart_swap][i]->setColor(QColor(239, 41, 41));
        m_ram_usage_set[m_chart_swap][i]->setColor(QColor(114, 159, 207));
    }

    m_cpu_usage_series[m_chart_swap] = new QBarSeries();

    m_ram_usage_series[m_chart_swap] = new QBarSeries();

    m_cpu_temp_series[m_chart_swap] = new QLineSeries();
    m_cpu_temp_series[m_chart_swap]->setColor(QColor(245, 121, 0));
    auto pen = m_cpu_temp_series[m_chart_swap]->pen();
    pen.setWidth(2);
    m_cpu_temp_series[m_chart_swap]->setPen(pen);

    m_latency_series[m_chart_swap] = new QPieSeries();

    for (int i = 0; i < 2; ++i)
    {
        m_latency_slice[m_chart_swap][i] = new QPieSlice();
    }

    // Calculate latency percent
    double latency_percent = (100.0 * m_latency_chart_data) / 20.0;
    if (latency_percent > 100)
    {
        latency_percent = 100;
    }

    m_latency_slice[m_chart_swap][0]->setValue(latency_percent);
    m_latency_slice[m_chart_swap][1]->setValue(100.0 - latency_percent);

    latency_percent < 50 ? m_latency_slice[m_chart_swap][0]->setColor(QColor(138, 226, 52)) : m_latency_slice[m_chart_swap][0]->setColor(QColor(239, 41, 41));

    m_latency_series[m_chart_swap]->append(m_latency_slice[m_chart_swap][0]);
    m_latency_series[m_chart_swap]->append(m_latency_slice[m_chart_swap][1]);

    m_latency_series[m_chart_swap]->setHoleSize(0.5);

    for (std::uint8_t i = 0; i < CHART_BINS; ++i)
    {
        m_cpu_usage_series[m_chart_swap]->append(m_cpu_usage_set[m_chart_swap][i]);
        m_ram_usage_series[m_chart_swap]->append(m_ram_usage_set[m_chart_swap][i]);
        m_cpu_temp_series[m_chart_swap]->append(i, m_charts_data[static_cast<std::uint32_t>(ChartSelect::CPU_TEMP)][i]);
    }

    m_cpu_usage_chart[m_chart_swap]->addSeries(m_cpu_usage_series[m_chart_swap]);
    m_ram_usage_chart[m_chart_swap]->addSeries(m_ram_usage_series[m_chart_swap]);
    m_cpu_temp_chart[m_chart_swap]->addSeries(m_cpu_temp_series[m_chart_swap]);
    m_latency_chart[m_chart_swap]->addSeries(m_latency_series[m_chart_swap]);

    m_cpu_usage_chart[m_chart_swap]->createDefaultAxes();
    m_ram_usage_chart[m_chart_swap]->createDefaultAxes();
    m_cpu_temp_chart[m_chart_swap]->createDefaultAxes();

    m_cpu_usage_chart[m_chart_swap]->setAxisX(m_cpu_usage_axis_x[m_chart_swap]);
    m_cpu_usage_chart[m_chart_swap]->axisY()->setRange(0, 100);
    m_ram_usage_chart[m_chart_swap]->setAxisX(m_ram_usage_axis_x[m_chart_swap]);
    m_ram_usage_chart[m_chart_swap]->axisY()->setRange(0, 100);
    m_cpu_temp_chart[m_chart_swap]->setAxisX(m_cpu_temp_axis_x[m_chart_swap]);
    m_cpu_temp_chart[m_chart_swap]->axisY()->setRange(0, 100);

    m_cpu_usage_chart[m_chart_swap]->legend()->hide();
    m_ram_usage_chart[m_chart_swap]->legend()->hide();
    m_cpu_temp_chart[m_chart_swap]->legend()->hide();
    m_latency_chart[m_chart_swap]->legend()->hide();

    const auto * cpu_usage_chart_ptr = ui->chart_view_cpu_usage->chart();
    const auto * ram_usage_chart_ptr = ui->chart_view_ram_usage->chart();
    const auto * cpu_temp_chart_ptr = ui->chart_view_cpu_temp->chart();
    const auto * cpu_latency_ptr = ui->chart_view_latency->chart();

    ui->chart_view_cpu_usage->setChart(m_cpu_usage_chart[m_chart_swap]);
    ui->chart_view_ram_usage->setChart(m_ram_usage_chart[m_chart_swap]);
    ui->chart_view_cpu_temp->setChart(m_cpu_temp_chart[m_chart_swap]);
    ui->chart_view_latency->setChart(m_latency_chart[m_chart_swap]);

    if (cpu_usage_chart_ptr != nullptr)
    {
        delete cpu_usage_chart_ptr;
    }
    if (cpu_usage_chart_ptr != nullptr)
    {
        delete ram_usage_chart_ptr;
    }
    if (cpu_temp_chart_ptr != nullptr)
    {
        delete cpu_temp_chart_ptr;
    }
    if (cpu_latency_ptr != nullptr)
    {
        delete cpu_latency_ptr;
    }
}

void MainWindow::on_radioButton_servo_num_toggled(bool checked)
{
    if (checked)
    {
        ui->radioButton_servo_pos->setChecked(false);
        ui->radioButton_servo_speed->setChecked(false);
        ui->radioButton_delay->setChecked(false);
        m_automatic_line_edit_select = static_cast<std::uint8_t>(AutomaticLineEditSelect::SERVO_NUM);
    }
}

void MainWindow::on_radioButton_servo_pos_toggled(bool checked)
{
    if (checked)
    {
        ui->radioButton_servo_num->setChecked(false);
        ui->radioButton_servo_speed->setChecked(false);
        ui->radioButton_delay->setChecked(false);
        m_automatic_line_edit_select = static_cast<std::uint8_t>(AutomaticLineEditSelect::SERVO_POS);
    }
}

void MainWindow::on_radioButton_servo_speed_toggled(bool checked)
{
    if (checked)
    {
        ui->radioButton_servo_pos->setChecked(false);
        ui->radioButton_servo_num->setChecked(false);
        ui->radioButton_delay->setChecked(false);
        m_automatic_line_edit_select = static_cast<std::uint8_t>(AutomaticLineEditSelect::SERVO_SPEED);
    }
}

void MainWindow::on_radioButton_delay_toggled(bool checked)
{
    if (checked)
    {
        ui->radioButton_servo_pos->setChecked(false);
        ui->radioButton_servo_speed->setChecked(false);
        ui->radioButton_servo_num->setChecked(false);
        m_automatic_line_edit_select = static_cast<std::uint8_t>(AutomaticLineEditSelect::DELAY);
    }
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

void MainWindow::on_button_0_clicked()
{
    handle_num_buttons('0');
}

void MainWindow::on_button_1_clicked()
{
    handle_num_buttons('1');
}

void MainWindow::on_button_2_clicked()
{
    handle_num_buttons('2');
}

void MainWindow::on_button_3_clicked()
{
    handle_num_buttons('3');
}

void MainWindow::on_button_4_clicked()
{
    handle_num_buttons('4');
}

void MainWindow::on_button_5_clicked()
{
    handle_num_buttons('5');
}

void MainWindow::on_button_6_clicked()
{
    handle_num_buttons('6');
}

void MainWindow::on_button_7_clicked()
{
    handle_num_buttons('7');
}

void MainWindow::on_button_8_clicked()
{
    handle_num_buttons('8');
}

void MainWindow::on_button_9_clicked()
{
    handle_num_buttons('9');
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

void MainWindow::check_edit_line_servo_num()
{
    int value = ui->lineEdit_servo_num->text().toInt();
    if ((value < 1) || (value > 6))
    {
        ui->lineEdit_servo_num->clear();
        ui->lineEdit_servo_num->setStyleSheet(m_line_edit_error_style_sheet);
        m_is_servo_num_valid = false;
    }
    else
    {
        ui->lineEdit_servo_num->setStyleSheet(m_line_edit_success_style_sheet);
        m_is_servo_num_valid = true;
    }
}

void MainWindow::check_edit_line_servo_pos()
{
    int value = ui->lineEdit_servo_pos->text().toInt();
    if ((value < 500) || (value > 2500))
    {
        ui->lineEdit_servo_pos->clear();
        ui->lineEdit_servo_pos->setStyleSheet(m_line_edit_error_style_sheet);
        m_is_servo_pos_valid = false;
    }
    else
    {
        ui->lineEdit_servo_pos->setStyleSheet(m_line_edit_success_style_sheet);
        m_is_servo_pos_valid = true;
    }
}

void MainWindow::check_edit_line_servo_speed()
{
    int value = ui->lineEdit_servo_speed->text().toInt();
    if ((value < 1) || (value > 10))
    {
        ui->lineEdit_servo_speed->clear();
        ui->lineEdit_servo_speed->setStyleSheet(m_line_edit_error_style_sheet);
        m_is_servo_speed_valid = false;
    }
    else
    {
        ui->lineEdit_servo_speed->setStyleSheet(m_line_edit_success_style_sheet);
        m_is_servo_speed_valid = true;
    }
}

void MainWindow::check_edit_line_delay()
{
    int value = ui->lineEdit_delay->text().toInt();
    if ((ui->lineEdit_delay->text() == "") || (value < 0) || (value > 10000))
    {
        ui->lineEdit_delay->clear();
        ui->lineEdit_delay->setStyleSheet(m_line_edit_error_style_sheet);
        m_is_delay_valid = false;
    }
    else
    {
        ui->lineEdit_delay->setStyleSheet(m_line_edit_success_style_sheet);
        m_is_delay_valid = true;
    }
}

void MainWindow::on_buton_add_step_clicked()
{
    check_edit_line_servo_num();
    check_edit_line_servo_pos();
    check_edit_line_servo_speed();
    check_edit_line_delay();

    if (m_is_servo_num_valid && m_is_servo_pos_valid && m_is_servo_speed_valid && m_is_delay_valid)
    {

        if (m_automatic_steps_count < 1000)
        {
            ++m_automatic_steps_count;
            ui->table_servo_steps->setRowCount(m_automatic_steps_count);
            QTableWidgetItem * item = new QTableWidgetItem();
            item->setText(ui->lineEdit_servo_num->text());
            ui->table_servo_steps->setItem(m_automatic_steps_count-1, 0, item);
            item = new QTableWidgetItem();
            item->setText(ui->lineEdit_servo_pos->text());
            ui->table_servo_steps->setItem(m_automatic_steps_count-1, 1, item);
            item = new QTableWidgetItem();
            item->setText(ui->lineEdit_servo_speed->text());
            ui->table_servo_steps->setItem(m_automatic_steps_count-1, 2, item);
            item = new QTableWidgetItem();
            item->setText(ui->lineEdit_delay->text());
            ui->table_servo_steps->setItem(m_automatic_steps_count-1, 3, item);

            clear_line_edits();
        }
    }
}

void MainWindow::clear_line_edits()
{
    ui->lineEdit_servo_num->clear();
    ui->lineEdit_servo_num->setStyleSheet("");
    ui->lineEdit_servo_pos->clear();
    ui->lineEdit_servo_pos->setStyleSheet("");
    ui->lineEdit_servo_speed->clear();
    ui->lineEdit_servo_speed->setStyleSheet("");
    ui->lineEdit_delay->clear();
    ui->lineEdit_delay->setStyleSheet("");
}

void MainWindow::on_button_remove_step_clicked()
{
    if (ui->lineEdit_step_number->text() != "")
    {
        int row = ui->lineEdit_step_number->text().toInt()-1;
        if (row >= 0 && row < ui->table_servo_steps->rowCount())
        {
            --m_automatic_steps_count;
            ui->table_servo_steps->removeRow(row);
            ui->lineEdit_step_number->clear();
        }
    }
}

void MainWindow::on_button_save_clicked()
{
    try
    {
        if(!std::filesystem::exists(m_automatic_file_path))
        {
            std::filesystem::create_directories(m_automatic_file_path);
        }

        const std::string file_name = ui->lineEdit_file_name->text().toStdString();

        std::filesystem::path file_path = m_automatic_file_path / file_name;
        std::ofstream file(file_path.string());

        for (int i = 0; i < ui->table_servo_steps->rowCount(); ++i)
        {
            for (int j = 0; j < ui->table_servo_steps->columnCount(); ++j)
            {
                file << ui->table_servo_steps->item(i, j)->text().toStdString() << "\n";
            }
        }

        ui->lineEdit_file_name->clear();
        scan_automatic_files();
        file.close();
    }
    catch (const std::exception & error)
    {
        std::cout << error.what() << std::endl;
    }
}

void MainWindow::on_button_load_clicked()
{
    try
    {
        if(std::filesystem::exists(m_automatic_file_path))
        {
            m_automatic_steps_count = 0;
            ui->table_servo_steps->setRowCount(m_automatic_steps_count);
            const std::string file_name = ui->list_automatic_files->currentItem()->text().toStdString();

            std::cout << file_name << std::endl;

            std::filesystem::path file_path = m_automatic_file_path / file_name;
            std::ifstream file(file_path.string());

            std::string data;

            bool work = true;
            while(work)
            {
                ++m_automatic_steps_count;
                for (int i = 0; i < ui->table_servo_steps->columnCount(); ++i)
                {
                    file >> data;
                    if (file.eof())
                    {
                        work = false;
                        --m_automatic_steps_count;
                        break;
                    }
                    else
                    {
                        ui->table_servo_steps->setRowCount(m_automatic_steps_count);
                        QTableWidgetItem * item = new QTableWidgetItem;
                        item->setText(QString::fromStdString(data));
                        ui->table_servo_steps->setItem(m_automatic_steps_count-1, i, item);
                    }
                }
            }
            file.close();
        }
    }
    catch (const std::exception & error)
    {
        std::cout << error.what() << std::endl;
    }
}

void MainWindow::scan_automatic_files()
{
    try
    {
        if (std::filesystem::exists(m_automatic_file_path))
        {
            ui->list_automatic_files->clear();
            for (const auto & entry : std::filesystem::directory_iterator(m_automatic_file_path))
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

void MainWindow::on_radioButton_loop_toggled(bool checked)
{
    std::cout << "TOGGLE!" << std::endl;
    checked ? m_run_in_loop = true : m_run_in_loop = false;
}

void MainWindow::on_button_execute_clicked()
{
    OdinServoStep servo_step;
    OdinAutomaticStepConfirm servo_step_confirm;
    OdinAutomaticConfirm automatic_confirm;
    OdinAutomaticExecuteData automatic_data;
    automatic_data.data_collection_status = 1;
    automatic_data.run_in_loop = m_run_in_loop;
    // TODO: Write error handling
    while (!m_automatic_execute_shmem_handler->shmemWrite(&automatic_data)){};
    bool confirm = false;
    while (!confirm)
    {
        if (m_automatic_execute_confirm_shmem_handler->openShmem())
        {
            if (m_automatic_execute_confirm_shmem_handler->shmemRead(&automatic_confirm))
            {
                if (automatic_confirm.confirm == true)
                {
                    confirm = true;
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    bool stop_sending_steps = false;
    while (!stop_sending_steps)
    {
        for (int i = 0; i < ui->table_servo_steps->rowCount(); ++i)
        {
            std::cout << "Writing data" << std::endl;
            servo_step.step_num = i;
            servo_step.servo_num = ui->table_servo_steps->item(i, 0)->text().toInt();
            servo_step.position = ui->table_servo_steps->item(i, 1)->text().toInt();
            servo_step.speed = ui->table_servo_steps->item(i, 2)->text().toInt();
            servo_step.delay = ui->table_servo_steps->item(i, 3)->text().toInt();
            while (!m_automatic_step_shmem_handler->shmemWrite(&servo_step));
            bool confirm_step = false;
            while (!confirm_step)
            {
                if (m_automatic_step_confirm_shmem_handler->openShmem())
                {
                    if (m_automatic_step_confirm_shmem_handler->shmemRead(&servo_step_confirm))
                    {
                        if (servo_step_confirm.step_num == i)
                        {
                            std::cout << "Confirm read" << std::endl;
                            confirm_step = true;
                        }
                        else
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                    }
                    else
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        }
        stop_sending_steps = true;
        std::cout << "Sending steps finished." << std::endl;
    }
    automatic_data.data_collection_status = 2;
    while (!m_automatic_execute_shmem_handler->shmemWrite(&automatic_data)){};
    confirm = false;
    while (!confirm)
    {
        if (m_automatic_execute_confirm_shmem_handler->openShmem())
        {
            if (m_automatic_execute_confirm_shmem_handler->shmemRead(&automatic_confirm))
            {
                if (automatic_confirm.confirm == true)
                {
                    confirm = true;
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void MainWindow::on_button_table_clear_clicked()
{
    m_automatic_steps_count = 0;
    ui->table_servo_steps->setRowCount(m_automatic_steps_count);
}

