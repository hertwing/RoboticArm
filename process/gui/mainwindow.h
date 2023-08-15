#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QLineEdit>
#include <QLineSeries>
#include <QMainWindow>
#include <QPieSeries>
#include <QPieSlice>
#include <QValueAxis>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <list>

#include "InetCommData.h"
#include "odin/diagnostic_handler/DataTypes.h"
#include "odin/shmem_wrapper/DataTypes.h"
#include "odin/shmem_wrapper/ShmemHandler.hpp"

using namespace odin::diagnostic_handler;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

enum class BoardSelect {
    GUI,
    ARM
};

enum class WidgetPage {
    MAIN,
    JOYPAD,
    DIAGNOSTIC,
    AUTOMATIC
};

enum class ChartSelect {
    CPU_USAGE,
    RAM_USAGE,
    CPU_TEMP,
    LATENCY
};

enum class AutomaticLineEditSelect
{
    SERVO_POS,
    SERVO_NUM,
    SERVO_SPEED,
    DELAY,
    NONE
};

static constexpr std::uint8_t INT_CHARTS_COUNT = 3;
static constexpr std::uint8_t CHART_BINS = 10;
static constexpr double LATENCY_MAX = 20.0;

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
    void on_radioButton_servo_num_toggled(bool checked);
    void on_radioButton_servo_pos_toggled(bool checked);
    void on_radioButton_servo_speed_toggled(bool checked);
    void on_radioButton_delay_toggled(bool checked);
    void on_button_clear_clicked();
    void on_button_del_clicked();
    void on_button_0_clicked();
    void on_button_1_clicked();
    void on_button_2_clicked();
    void on_button_3_clicked();
    void on_button_4_clicked();
    void on_button_5_clicked();
    void on_button_6_clicked();
    void on_button_7_clicked();
    void on_button_8_clicked();
    void on_button_9_clicked();
    void on_buton_add_step_clicked();
    void on_button_remove_step_clicked();
    void on_button_save_clicked();
    void on_button_load_clicked();
    void on_radioButton_loop_toggled(bool checked);

    void on_button_execute_clicked();

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
    void check_edit_line_servo_num();
    void check_edit_line_servo_pos();
    void check_edit_line_servo_speed();
    void check_edit_line_delay();
    void clear_line_edits();
    void scan_automatic_files();

private:
    Ui::MainWindow * ui;
    bool m_joypad_enabled;
    bool m_automatic_enabled;
    bool m_diagnostic_enabled;
    bool m_diagnostic_board_selected;
    bool m_chart_swap;

    bool m_is_servo_num_valid;
    bool m_is_servo_pos_valid;
    bool m_is_servo_speed_valid;
    bool m_is_delay_valid;
    bool m_run_in_loop;

    std::list<OdinServoStep> m_automatic_steps;

    std::filesystem::path m_automatic_file_path;

    std::uint64_t m_automatic_steps_count;

    std::uint8_t m_automatic_line_edit_select;

    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<DiagnosticData>> m_gui_diagnostic_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<DiagnosticData>> m_arm_diagnostic_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<OdinControlSelection>> m_control_selection_shmem_handler;
    DiagnosticData m_gui_diagnostic_data;
    DiagnosticData m_rak_diagnostic_data;

    OdinControlSelection m_control_selection;

    QTimer * m_diagnostic_timer;

    std::uint32_t m_charts_data[INT_CHARTS_COUNT][CHART_BINS] = { 0 };
    double m_latency_chart_data;
    QBarSet * m_cpu_usage_set[2][CHART_BINS];
    QBarSet * m_ram_usage_set[2][CHART_BINS];

    QBarSeries * m_cpu_usage_series[2];
    QBarSeries * m_ram_usage_series[2];
    QLineSeries * m_cpu_temp_series[2];
    QPieSeries * m_latency_series[2];
    QPieSlice * m_latency_slice[2][2];

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
    const QString m_line_edit_error_style_sheet =
        "background-color: rgb(239, 41, 41);";
    const QString m_line_edit_success_style_sheet =
        "background-color: rgb(115, 210, 22);";
};
#endif // MAINWINDOW_H
