#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "odin/diagnostic_handler/DataTypes.h"
#include "odin/shmem_wrapper/ShmemHandler.hpp"
#include "odin/shmem_wrapper/DataTypes.h"

#include <QMainWindow>
#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>

#include <memory>
#include <cstdint>

using namespace odin::diagnostic_handler;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

enum BoardSelect {
    GUI,
    ARM
};

enum WidgetPage {
    MAIN,
    JOYPAD,
    DIAGNOSTIC,
    AUTOMATIC
};

enum ChartSelect {
    CPU_USAGE,
    RAM_USAGE,
    CPU_TEMP,
    LATENCY
};

static constexpr std::uint8_t CHARTS_COUNT = 4;
static constexpr std::uint8_t CHART_BINS = 10;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_button_exit_clicked();

    void on_button_joypad_clicked();

    void on_button_diagnostic_clicked();

    void on_button_rpi_switch_clicked();

    void diagnosticTimerSlot();

    void on_button_automatic_clicked();

    void on_dial_step_sliderMoved(int position);

private:
    void draw_menu();
    void disable_buttons();
    void show_joypad();
    void hide_joypad();
    void show_diagnostics();
    void hide_diagnostics();
    void draw_charts();
    void show_automatic();
    void hide_automatic();

private:
    Ui::MainWindow * ui;
    bool m_joypad_enabled;
    bool m_automatic_enabled;
    bool m_diagnostic_enabled;
    bool m_diagnostic_board_selected;
    bool m_chart_swap;

    std::unique_ptr<shmem_wrapper::ShmemHandler<DiagnosticData>> m_shmem_handler;
    DiagnosticData m_gui_diagnostic_data;
    DiagnosticData m_rak_diagnostic_data;

    QTimer * m_diagnostic_timer;

    std::uint32_t m_charts_data[CHARTS_COUNT][CHART_BINS] = { 0 };
    QBarSet * m_cpu_usage_set[2][CHART_BINS];
    QBarSet * m_ram_usage_set[2][CHART_BINS];
//    QBarSet * cpu_temp_set;
//    QBarSet * latency_set;

    QBarSeries * m_cpu_usage_series[2];
    QBarSeries * m_ram_usage_series[2];
//    QBarSeries * cpu_temp_series;
//    QBarSeries * latency_series;

    QChart * m_cpu_usage_chart[2];
    QChart * m_ram_usage_chart[2];
    QChart * m_cpu_temp_chart[2];
    QChart * m_latency_chart[2];

    QBarCategoryAxis * m_cpu_usage_axis_x[2];
    QBarCategoryAxis * m_ram_usage_axis_x[2];
    QBarCategoryAxis * m_cpu_temp_axis_x[2];
    QBarCategoryAxis * m_latency_axis_x[2];
private:
    const QString m_button_rpi_switch_gui_style_sheet =
        "padding: 1; \
        image: url(:/icons/resources/rpi_icon.png); \
        background-color: rgb(252, 175, 62);";
    const QString m_button_rpi_switch_arm_style_sheet =
        "padding: 1; \
        image: url(:/icons/resources/rpi_icon.png); \
        background-color: rgb(173, 127, 168);";
    const QString m_button_exit_style_sheet =
        "padding: 10; \
        image: url(:/icons/resources/power_off_white.png); \
        background-color: rgb(46, 52, 54);";
    const QString m_enabled_joypad_style_sheet =
        "padding: 3; \
        background-color: rgb(255, 155, 0); \
        image: url(:/icons/resources/joypad.png);";
    const QString m_disabled_joypad_style_sheet =
        "padding: 3; \
        background-color: rgb(204, 0, 0); \
        image: url(:/icons/resources/joypad.png);";
    const QString m_enabled_automatic_style_sheet =
        "padding: 3; \
        background-color: rgb(255, 155, 0); \
        image: url(:/icons/resources/automatic_icon.png);";
        const QString m_disabled_automatic_style_sheet =
        "padding: 3; \
        background-color: rgb(204, 0, 0); \
        image: url(:/icons/resources/automatic_icon.png);";
    const QString m_enabled_diagnostic_style_sheet =
        "padding: 3; \
        image: url(:/icons/resources/magnifying_glass.png); \
        background-color: rgb(255, 155, 0);";
    const QString m_disabled_diagnostic_style_sheet =
        "padding: 3; \
        image: url(:/icons/resources/magnifying_glass.png); \
        background-color: rgb(204, 0, 0);";
    const QString m_diagnostic_widget_style_sheet =
        "border: 2 solid rgb(114, 159, 207);";
    const QString m_diagnostic_chart_widget_style_sheet =
        "border-top: 1 solid rgb(114, 159, 207);";
    const QString m_diagnostic_label_style_sheet =
        "border: none;";
};
#endif // MAINWINDOW_H
