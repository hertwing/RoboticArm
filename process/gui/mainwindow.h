#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

enum BoardSelect {
    GUI = 0,
    ARM = 1
};

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

private:
    void draw_menu();
    void disable_buttons();
    void show_diagnostics();
    void hide_diagnostics();

private:
    Ui::MainWindow *ui;
    bool m_joypad_enabled;
    bool m_diagnostic_enabled;
    bool m_diagnostic_board_selected;

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
