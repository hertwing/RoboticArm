#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_joypad_enabled(false),
    m_diagnostic_enabled(false),
    m_diagnostic_board_selected(BoardSelect::GUI)
{
    ui->setupUi(this);
    draw_menu();
}

MainWindow::~MainWindow()
{
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
}

void MainWindow::hide_diagnostics()
{
    ui->widget_diagnostic->hide();
    ui->button_diagnostic->setStyleSheet(m_disabled_diagnostic_style_sheet);
}


