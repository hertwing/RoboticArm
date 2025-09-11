#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ChartsPanel.h"
#include "CvWorker.h"
#include "InetCommData.h"
#include "ScriptedMotionWorker.h"
#include "OdinStepsModel.h"
#include "UdpMjpegReceiver.h"
#include "odin/diagnostic_handler/DataTypes.h"
#include "odin/shmem_wrapper/DataTypes.h"
#include "odin/shmem_wrapper/ShmemHandler.hpp"
#include "odin/video_handler/DataTypes.h"
#include "UdpHandler.hpp"

#include <QLineEdit>
#include <QLineSeries>
#include <QMainWindow>
#include <QSocketNotifier>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

#include <QElapsedTimer>

using namespace odin::diagnostic_handler;
using namespace odin::video_handler;

class V4L2Capture;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

enum class BoardSelect {
    GUI,
    ARM
};

enum class WidgetPage {
    MAIN,
    CAMERA,
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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    static void signalCallbackHandler(int signum);

signals:
    void stopScriptedMotionRequested();

private slots:
    void on_button_exit_clicked();
    void on_button_joypad_clicked();
    void on_button_diagnostic_clicked();
    void on_button_rpi_switch_clicked();
    void diagnosticTimerSlot();
    void on_button_automatic_clicked();
    void on_button_clear_clicked();
    void on_button_del_clicked();
    void handleDigitButtonClicked();
    void on_button_add_step_clicked();
    void on_button_remove_step_clicked();
    void on_button_save_clicked();
    void on_button_load_clicked();
    void onRadioGroupToggled(int id, bool checked);
    void on_radioButton_loop_toggled(bool checked);
    void on_button_execute_clicked();
    void on_button_table_clear_clicked();
    void on_button_stop_clicked();
    void handleMotionCompleted();
    void on_button_camera_clicked();
    void onFrame(const QImage& img);

private:
    void draw_menu();
    void disable_buttons();
    void clear_line_edits();
    void scan_automatic_files();
    void handle_num_buttons(char num);
    bool loadFaceCascadeFromResource();

private:
    Ui::MainWindow * ui;

    ChartsPanel* m_charts = nullptr;
    OdinStepsModel * m_stepsModel = nullptr;
    QThread * m_motionThread = nullptr;
    ScriptedMotionWorker * m_motionWorker = nullptr;

    enum class UIMode { MAIN, JOYPAD, CAMERA, DIAGNOSTIC, AUTOMATIC };
    UIMode m_mode = UIMode::MAIN;
    void switchMode(UIMode m);

    BoardSelect m_diagnostic_board_selected;

    bool m_run_in_loop;

    std::shared_ptr<scripted_motion_status_t> m_scripted_motion_request_status;
    std::shared_ptr<scripted_motion_status_t> m_scripted_motion_reply_status;

    std::shared_ptr<std::vector<OdinServoStep>> m_automatic_steps;

    std::filesystem::path m_scripted_motion_files_path;

    std::uint8_t m_automatic_line_edit_select;

    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<DiagnosticData>> m_gui_diagnostic_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<DiagnosticData>> m_arm_diagnostic_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<OdinControlSelection>> m_control_selection_shmem_handler;

    DiagnosticData m_diagnostic_data;

    OdinControlSelection m_control_selection;

    bool readDiagnosticOnceFor(BoardSelect src, DiagnosticData& out);
    QTimer * m_diagnostic_timer;

    UdpMjpegReceiver* m_rx = nullptr;

    uint32_t m_lastDisplayedSeq{0};

    // CV
    // detection throttling every frame
    QElapsedTimer m_uiTimer;
    int m_uiMinIntervalMs = 33;

    QThread * m_faceThread = nullptr;
    CvWorker * m_CvWorker = nullptr;
    std::atomic_bool m_faceBusy{false};
    QRect m_lastFaceQt;
    bool m_haveFaceQt = false;
    QElapsedTimer m_fullDetectTimer;
    const int m_fullDetectMs = 1200; // refresh rate full every ~1.2s
    std::string m_cascade_tmp_path = "";
    std::vector<QLineEdit*> m_line_edits;
};
#endif // MAINWINDOW_H
