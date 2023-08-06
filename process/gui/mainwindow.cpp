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
    m_diagnostic_board_selected(BoardSelect::GUI),
    m_chart_swap(0)
{
    m_shmem_handler = std::make_unique<shmem_wrapper::ShmemHandler<DiagnosticData>>(
        shmem_wrapper::DataTypes::DIAGNOSTIC_SHMEM_NAME, sizeof(DiagnosticData), false);
    ui->setupUi(this);

    m_diagnostic_timer = nullptr;

    draw_charts();
    draw_menu();
}

MainWindow::~MainWindow()
{
    auto * cpu_usage_chart_ptr = ui->chart_view_cpu_usage->chart();
    auto * ram_usage_chart_ptr = ui->chart_view_ram_usage->chart();

    delete ui;

    delete cpu_usage_chart_ptr;
    delete ram_usage_chart_ptr;
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
    ui->stackedWidget->setCurrentIndex(WidgetPage::MAIN);
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
    m_joypad_enabled ? show_joypad() : hide_joypad();
}

void MainWindow::on_button_diagnostic_clicked()
{
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
    m_automatic_enabled ? show_automatic() : hide_automatic();
}

void MainWindow::on_button_rpi_switch_clicked()
{
    m_diagnostic_board_selected = !m_diagnostic_board_selected;
    m_diagnostic_board_selected ? ui->button_rpi_switch->setStyleSheet(m_button_rpi_switch_arm_style_sheet) : ui->button_rpi_switch->setStyleSheet(m_button_rpi_switch_gui_style_sheet);
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
    ui->stackedWidget->setCurrentIndex(WidgetPage::JOYPAD);
    ui->button_joypad->setStyleSheet(m_enabled_joypad_style_sheet);
}

void MainWindow::hide_joypad()
{
    ui->stackedWidget->setCurrentIndex(WidgetPage::MAIN);
    ui->button_joypad->setStyleSheet(m_disabled_joypad_style_sheet);
}

void MainWindow::show_diagnostics()
{
    ui->stackedWidget->show();
    ui->stackedWidget->setCurrentIndex(WidgetPage::DIAGNOSTIC);
    ui->button_diagnostic->setStyleSheet(m_enabled_diagnostic_style_sheet);

    m_diagnostic_timer = new QTimer(this);
    m_diagnostic_timer->start(250);
    connect(m_diagnostic_timer, SIGNAL(timeout()), this, SLOT(diagnosticTimerSlot()));
}

void MainWindow::hide_diagnostics()
{
    if (m_diagnostic_timer != nullptr)
    {
        delete m_diagnostic_timer;
    }

    ui->stackedWidget->setCurrentIndex(WidgetPage::MAIN);
    ui->button_diagnostic->setStyleSheet(m_disabled_diagnostic_style_sheet);
}

void MainWindow::show_automatic()
{
    ui->stackedWidget->setCurrentIndex(WidgetPage::AUTOMATIC);
    ui->button_automatic->setStyleSheet(m_enabled_automatic_style_sheet);
}

void MainWindow::hide_automatic()
{
    ui->stackedWidget->setCurrentIndex(WidgetPage::MAIN);
    ui->button_automatic->setStyleSheet(m_disabled_automatic_style_sheet);
}

void MainWindow::diagnosticTimerSlot()
{
    if (m_shmem_handler->openShmem())
    {
        m_shmem_handler->shmemRead(&m_gui_diagnostic_data);
    }
    else
    {
        m_gui_diagnostic_data.cpu_usage = 0;
        m_gui_diagnostic_data.ram_usage = 0;
        m_gui_diagnostic_data.cpu_temp = 0;
        m_gui_diagnostic_data.latency = 0;
    }
    // Fill charts bins with data
    const auto * temp_charts_data = m_charts_data;
    for (std::uint8_t i = 0; i < CHARTS_COUNT; ++i)
    {
        for (std::uint8_t j = 0; j < CHART_BINS-1; ++j)
        {
            m_charts_data[i][j] = temp_charts_data[i][j+1];
        }
    }
    // Get latest data to last char bin
    m_charts_data[CPU_USAGE][CHART_BINS-1] = m_gui_diagnostic_data.cpu_usage;
    m_charts_data[RAM_USAGE][CHART_BINS-1] = m_gui_diagnostic_data.ram_usage;
    m_charts_data[CPU_TEMP][CHART_BINS-1] = m_gui_diagnostic_data.cpu_temp;
    m_charts_data[LATENCY][CHART_BINS-1] = m_gui_diagnostic_data.latency;
    // Draw charts
    draw_charts();
}

void MainWindow::draw_charts()
{
    m_chart_swap = !m_chart_swap;
    // Draw data on charts
    m_cpu_usage_chart[m_chart_swap] = new QChart();
    m_ram_usage_chart[m_chart_swap] = new QChart();

    m_cpu_usage_axis_x[m_chart_swap] = new QBarCategoryAxis();
    m_ram_usage_axis_x[m_chart_swap] = new QBarCategoryAxis();
    //        m_cpu_temp_axis_x[0] = new QBarCategoryAxis();
    //        m_latency_axis_x[0] = new QBarCategoryAxis();

    m_cpu_usage_axis_x[m_chart_swap]->append("%");
    m_ram_usage_axis_x[m_chart_swap]->append("%");
    //        m_cpu_temp_axis_x[0]->append("°C");
    //        m_latency_axis_x[0]->append("ms");

    for(int i=0; i<CHART_BINS; ++i)
    {
        m_cpu_usage_set[m_chart_swap][i] = new QBarSet("");
        m_ram_usage_set[m_chart_swap][i] = new QBarSet("");
        m_cpu_usage_set[m_chart_swap][i]->append(m_charts_data[CPU_USAGE][i]);
        m_ram_usage_set[m_chart_swap][i]->append(m_charts_data[RAM_USAGE][i]);
        m_cpu_usage_set[m_chart_swap][i]->setColor(QColor(239, 41, 41));
        m_ram_usage_set[m_chart_swap][i]->setColor(QColor(114, 159, 207));
    }

    m_cpu_usage_series[m_chart_swap] = new QBarSeries();
    m_ram_usage_series[m_chart_swap] = new QBarSeries();

    for (std::uint8_t i = 0; i < CHART_BINS; ++i)
    {
        m_cpu_usage_series[m_chart_swap]->append(m_cpu_usage_set[m_chart_swap][i]);
        m_ram_usage_series[m_chart_swap]->append(m_ram_usage_set[m_chart_swap][i]);
    }

    m_cpu_usage_chart[m_chart_swap]->addSeries(m_cpu_usage_series[m_chart_swap]);
    m_ram_usage_chart[m_chart_swap]->addSeries(m_ram_usage_series[m_chart_swap]);
    m_cpu_usage_chart[m_chart_swap]->createDefaultAxes();
    m_ram_usage_chart[m_chart_swap]->createDefaultAxes();
    m_cpu_usage_chart[m_chart_swap]->setAxisX(m_cpu_usage_axis_x[m_chart_swap]);
    m_ram_usage_chart[m_chart_swap]->setAxisX(m_ram_usage_axis_x[m_chart_swap]);
    m_cpu_usage_chart[m_chart_swap]->legend()->hide();
    m_ram_usage_chart[m_chart_swap]->legend()->hide();

    const auto * cpu_usage_chart_ptr = ui->chart_view_cpu_usage->chart();
    const auto * ram_usage_chart_ptr = ui->chart_view_ram_usage->chart();

    ui->chart_view_cpu_usage->setChart(m_cpu_usage_chart[m_chart_swap]);
    ui->chart_view_ram_usage->setChart(m_ram_usage_chart[m_chart_swap]);

    if (cpu_usage_chart_ptr != nullptr)
    {
        delete cpu_usage_chart_ptr;
    }
    if (cpu_usage_chart_ptr != nullptr)
    {
        delete ram_usage_chart_ptr;
    }
}


void MainWindow::on_dial_step_sliderMoved(int position)
{
    ui->label_step->setText("Step: " + QString::number(position));
}

