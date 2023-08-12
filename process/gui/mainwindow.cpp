#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <iostream>
#include <string>
#include <QTimer>

#include <QList>

#include <chrono>
#include <thread>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_joypad_enabled(false),
    m_automatic_enabled(false),
    m_diagnostic_enabled(false),
    m_diagnostic_board_selected(static_cast<bool>(BoardSelect::GUI)),
    m_chart_swap(0)
{
    m_gui_diagnostic_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<DiagnosticData>>(
        odin::shmem_wrapper::DataTypes::DIAGNOSTIC_SHMEM_NAME, sizeof(DiagnosticData), false);
    m_arm_diagnostic_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<DiagnosticData>>(
        odin::shmem_wrapper::DataTypes::DIAGNOSTIC_FROM_REMOTE_SHMEM_NAME, sizeof(DiagnosticData), false);
    m_control_selection_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinControlSelection>>(
        odin::shmem_wrapper::DataTypes::CONTROL_SELECT_SHMEM_NAME, sizeof(OdinControlSelection), true);

    ui->setupUi(this);

    m_diagnostic_timer = nullptr;
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

    m_diagnostic_timer = new QTimer(this);
    m_diagnostic_timer->start(300);
    connect(m_diagnostic_timer, SIGNAL(timeout()), this, SLOT(diagnosticTimerSlot()));
}

void MainWindow::hide_diagnostics()
{
    if (m_diagnostic_timer != nullptr)
    {
        delete m_diagnostic_timer;
    }

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


void MainWindow::on_dial_step_sliderMoved(int position)
{
    ui->label_step->setText("Step: " + QString::number(position));
}

