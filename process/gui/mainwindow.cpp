#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <iostream>
#include <string>
#include <QTimer>
#include <QChar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_joypad_enabled(false),
    m_diagnostic_enabled(false),
    m_diagnostic_board_selected(BoardSelect::GUI)
{
    m_shmem_handler = std::make_unique<shmem_wrapper::ShmemHandler<DiagnosticData>>(
        shmem_wrapper::DataTypes::DIAGNOSTIC_SHMEM_NAME, sizeof(DiagnosticData), false);
    ui->setupUi(this);
    draw_menu();
}

MainWindow::~MainWindow()
{
    delete m_diagnostic_timer;
    delete ui;
}

void MainWindow::draw_menu()
{
    ui->button_joypad->setStyleSheet(m_disabled_joypad_style_sheet);
    ui->button_diagnostic->setStyleSheet(m_disabled_diagnostic_style_sheet);
    ui->button_exit->setStyleSheet(m_button_exit_style_sheet);
    ui->button_rpi_switch->setStyleSheet(m_button_rpi_switch_gui_style_sheet);
    ui->widget_cpu_temp->setStyleSheet(m_diagnostic_widget_style_sheet);
    ui->widget_cpu_temp_chart->setStyleSheet(m_diagnostic_chart_widget_style_sheet);
    ui->widget_cpu_usage->setStyleSheet(m_diagnostic_widget_style_sheet);
    ui->widget_cpu_usage_chart->setStyleSheet(m_diagnostic_chart_widget_style_sheet);
    ui->widget_ram_usage->setStyleSheet(m_diagnostic_widget_style_sheet);
    ui->widget_ram_usage_chart->setStyleSheet(m_diagnostic_chart_widget_style_sheet);
    ui->widget_latency->setStyleSheet(m_diagnostic_widget_style_sheet);
    ui->widget_latency_chart->setStyleSheet(m_diagnostic_chart_widget_style_sheet);
    ui->label_cpu_usage->setStyleSheet(m_diagnostic_label_style_sheet);
    ui->label_cpu_temp->setStyleSheet(m_diagnostic_label_style_sheet);
    ui->label_ram_usage->setStyleSheet(m_diagnostic_label_style_sheet);
    ui->label_latency->setStyleSheet(m_diagnostic_label_style_sheet);
    ui->widget_diagnostic->hide();
    QMainWindow::showFullScreen();
}

void MainWindow::on_button_exit_clicked()
{
    MainWindow::close();
}

void MainWindow::on_button_joypad_clicked()
{
    m_joypad_enabled = !m_joypad_enabled;
    m_joypad_enabled ? ui->button_joypad->setStyleSheet(m_enabled_joypad_style_sheet) : ui->button_joypad->setStyleSheet(m_disabled_joypad_style_sheet);
}

void MainWindow::on_button_diagnostic_clicked()
{
    m_diagnostic_enabled = !m_diagnostic_enabled;
    m_diagnostic_enabled ? show_diagnostics() : hide_diagnostics();
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
}

void MainWindow::show_diagnostics()
{
    ui->widget_diagnostic->show();
    ui->button_diagnostic->setStyleSheet(m_enabled_diagnostic_style_sheet);

    m_diagnostic_timer = new QTimer(this);
    m_diagnostic_timer->start(250);
    connect(m_diagnostic_timer, SIGNAL(timeout()), this, SLOT(diagnosticTimerSlot()));
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
        m_gui_diagnostic_data.cpu_temp = 0;
        m_gui_diagnostic_data.ram_usage = 0;
        m_gui_diagnostic_data.latency = 0;
    }
    ui->label_cpu_usage_val->setText(QString::fromStdString(std::to_string(m_gui_diagnostic_data.cpu_usage) + "%"));
    ui->label_ram_usage_val->setText(QString::fromStdString(std::to_string(m_gui_diagnostic_data.ram_usage) + "%"));
    ui->label_cpu_temp_val->setText(QString::fromStdString(std::to_string(m_gui_diagnostic_data.cpu_temp) + "°C"));
    ui->label_latency_val->setText(QString::fromStdString(std::to_string(m_gui_diagnostic_data.latency) + "ms"));
}

void MainWindow::hide_diagnostics()
{
    delete m_diagnostic_timer;
    ui->widget_diagnostic->hide();
    ui->button_diagnostic->setStyleSheet(m_disabled_diagnostic_style_sheet);
}


