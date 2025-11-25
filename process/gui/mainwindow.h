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
#include "odin/odin_steps_io_handler/OdinStepsIOHandler.h"
#include "UdpHandler.hpp"

#include "ServoController.h"

#include <QLineEdit>
#include <QLineSeries>
#include <QMainWindow>
#include <QSocketNotifier>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

#include <QElapsedTimer>

using namespace odin::diagnostic_handler;
using namespace odin::video_handler;
using namespace odin::io;


// MOVE LATER:
static QRect makeAdaptiveTargetRect(int W, int H, const QRect& face_bbox)
{
    const int    minWH    = std::min(W, H);
    const int    MIN_SIDE = int(0.12 * minWH);  // lower limit of rectangle size
    const int    MAX_SIDE = int(0.60 * minWH);  // upper limit of rectangle size
    const double SCALE    = 1.25;               // enlarge relative to detection bbox size
    const double PAD_FRAC = 0.08;               // additional margin around
    const double EMA_A    = 0.25;               // smooth size (0..1)

    // keep the size smooth between frames
    static int side_ema = -1;

    // Determine the raw side of the rectangle from the face (or default)
    int side_raw;
    if (face_bbox.isValid() && !face_bbox.isEmpty())
    {
        const int face_side = std::max(face_bbox.width(), face_bbox.height());
        side_raw = std::clamp(int(std::lround(face_side * SCALE)), MIN_SIDE, MAX_SIDE);
    }
    else
    {
        // no face: keep the last size, and if there is no face keep a reasonable default
        side_raw = (side_ema > 0) ? side_ema : int(0.30 * minWH);
    }

    // EMA size so it doesn't jump around
    if (side_ema < 0) side_ema = side_raw;
    else              side_ema = int(std::lround((1.0 - EMA_A) * side_ema + EMA_A * side_raw));

    int side = std::clamp(side_ema, MIN_SIDE, MAX_SIDE);

    // Add margin proportional to the side
    const int pad = int(std::lround(PAD_FRAC * side));
    side = std::min({ side + 2 * pad, W, H });

    // Centered rectangle
    const int cx = W / 2;
    const int cy = H / 2;
    QRect r(cx - side / 2, cy - side / 2, side, side);

    // Frame clamp
    if (r.width() > W)  r.setWidth(W);
    if (r.height() > H) r.setHeight(H);
    r.moveTo(std::clamp(r.x(), 0, W - r.width()),
             std::clamp(r.y(), 0, H - r.height()));
    return r;
}

static CameraPosData computeCameraPosData(
    int frame_w,
    int frame_h,
    const QRect & face_bbox,
    int pan_cur_pwm,
    int tilt_cur_pwm)
{
    CameraPosData out{};
    out.pan_pos.position  = pan_cur_pwm;
    out.pan_pos.servo_num = SERVO_CAMERA_PAN;
    out.pan_pos.speed = 10;
    out.tilt_pos.position = tilt_cur_pwm;
    out.tilt_pos.servo_num = SERVO_CAMERA_TILT;
    out.tilt_pos.speed = 10;

    // ---- Configuration ----
    constexpr int  PAN_HOME  = 1500;
    constexpr int  TILT_HOME = 1500;

    // Logitech C920 - 640x480 view degrees
    constexpr double FOV_H_DEG = 70.0;
    constexpr double FOV_V_DEG = 43.0;

    // Gain and angle mapping - PWM
    constexpr double PWM_PER_DEG = 2000.0 / 180.0; // ~11.11 us/° for 500..2500
    constexpr double K_PAN  = 0.6;   // level sensitivity
    constexpr double K_TILT = 0.6;   // vertical sensitivity

    // Directions
    constexpr int S_PAN  = -1;
    constexpr int S_TILT = -1;

    // Movement stabilization
    constexpr int DEADZONE_PX = 15;
    constexpr int MIN_STEP_PWM = 2; // minimal step for servo deadband
    constexpr int MAX_DELTA_PAN_PWM = 25;  // maximum change per call
    constexpr int MAX_DELTA_TILT_PWM = 25;

    // Detection loss timeout (before moving to home position)
    constexpr int LOSS_TIMEOUT_MS = 800;

    // ---- Helpers ----
    auto clamp_int = [](int v, int lo, int hi) {
        return (v < lo) ? lo : (v > hi ? hi : v);
    };
    constexpr double kPi = 3.14159265358979323846;
    auto rad = [](double deg){ return deg * kPi / 180.0; };

    // Calculation of the centered target window
    const QRect target_rect = makeAdaptiveTargetRect(frame_w, frame_h, face_bbox);

    // Effective focal length in pixels (pinhole model)
    const double fpx_h = frame_w / (2.0 * std::tan(rad(FOV_H_DEG) * 0.5));
    const double fpx_v = frame_h / (2.0 * std::tan(rad(FOV_V_DEG) * 0.5));

    // Clock for no detection logic
    using clock = std::chrono::steady_clock;
    const auto now = clock::now();
    static bool inited_time = false;
    static clock::time_point last_face_tp;
    if (!inited_time) { last_face_tp = now; inited_time = true; }

    const bool have_face = face_bbox.isValid() && !face_bbox.isEmpty();

    // --------- CASE 1: target detected ---------
    if (have_face)
    {
        last_face_tp = now;

        const QPoint fc = face_bbox.center();
        const QPoint tc = target_rect.center();

        int dx_px = 0;
        int dy_px = 0;

        // Check if detection bbox is larger than target area bbox
        const bool face_bigger =
            (face_bbox.width()  > target_rect.width()) ||
            (face_bbox.height() > target_rect.height());

        if (face_bigger)
        {
            dx_px = fc.x() - tc.x();
            dy_px = fc.y() - tc.y();
        }
        else
        {
            /// minimum inner margin - keeps the detection bbox not right on the edge
            constexpr int CONTAIN_MARGIN_PX = 2;

            const int L = target_rect.left()   + CONTAIN_MARGIN_PX;
            const int R = target_rect.right()  - CONTAIN_MARGIN_PX;
            const int T = target_rect.top()    + CONTAIN_MARGIN_PX;
            const int B = target_rect.bottom() - CONTAIN_MARGIN_PX;

            // error only if detection bbox gout out of target area bbox
            dx_px = 0;
            if      (face_bbox.left()  < L) dx_px = face_bbox.left()  - L;   // too much left
            else if (face_bbox.right() > R) dx_px = face_bbox.right() - R;   // too much right

            dy_px = 0;
            if      (face_bbox.top()    < T) dy_px = face_bbox.top()    - T; // too much up
            else if (face_bbox.bottom() > B) dy_px = face_bbox.bottom() - B; // too much down
        }

        // Deadzone in pixels
        if (std::abs(dx_px) <= DEADZONE_PX) dx_px = 0;
        if (std::abs(dy_px) <= DEADZONE_PX) dy_px = 0;

        // No movement needed
        if (dx_px == 0 && dy_px == 0)
            return out;

        // Pixels to radians
        const double d_yaw_rad   = std::atan(static_cast<double>(dx_px) / fpx_h);
        const double d_pitch_rad = std::atan(static_cast<double>(dy_px) / fpx_v);

        // Angle to PWM
        const double pan_pwm_delta  = S_PAN  * (K_PAN  * PWM_PER_DEG) * (d_yaw_rad   * 180.0 / kPi);
        const double tilt_pwm_delta = S_TILT * (K_TILT * PWM_PER_DEG) * (d_pitch_rad * 180.0 / kPi);

        out.pan_pos.position  = pan_cur_pwm + pan_pwm_delta;
        out.tilt_pos.position = tilt_cur_pwm + tilt_pwm_delta;
        return out;
    }

    // --------- CASE 2: no detection ---------
    {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_face_tp).count();
        if (ms > LOSS_TIMEOUT_MS)
        {
            // Slowly return to home position
            const int dpan = clamp_int(PAN_HOME - pan_cur_pwm, -MAX_DELTA_PAN_PWM, +MAX_DELTA_PAN_PWM);
            const int dtilt = clamp_int(TILT_HOME - tilt_cur_pwm, -MAX_DELTA_TILT_PWM, +MAX_DELTA_TILT_PWM);

            // If almost at home position calculate last step
            auto snap_if_tiny = [&](int cur, int home, int d)
            {
                if (std::abs(home - cur) < MIN_STEP_PWM) return (home - cur);
                return d;
            };

            out.pan_pos.position = pan_cur_pwm + snap_if_tiny(pan_cur_pwm, PAN_HOME, dpan);
            out.tilt_pos.position = tilt_cur_pwm + snap_if_tiny(tilt_cur_pwm, TILT_HOME, dtilt);
            return out;
        }
        return out;
    }
}
////

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
    void on_button_draw_targets_clicked();
    void onFrame(const QImage& img);

private:
    void draw_menu();
    void disable_buttons();
    void clear_line_edits();
    void scan_automatic_files();
    void handle_num_buttons(char num);

private:
    Ui::MainWindow * ui;

    ChartsPanel* m_charts = nullptr;
    OdinStepsModel * m_stepsModel = nullptr;
    QThread * m_motionThread = nullptr;
    QThread * m_rxThread = nullptr;
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
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<CameraPosData>> m_camera_pos_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<CameraPosReadyData>> m_camera_pos_ready_shmem_handler;

    DiagnosticData m_diagnostic_data;

    OdinControlSelection m_control_selection;

    OdinStepsIOHandler m_odin_steps_io_handler;

    CameraPosData m_camera_pos_data;
    CameraPosReadyData m_camera_pos_ready_data;

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
    QRect m_face_bbox;
    QRect m_roi_hint;
    bool m_haveFaceQt = false;
    double m_smile_conf = 0.0;
    double m_smile_counter = 0.0;
    bool m_smile_detected = false;
    std::vector<QLineEdit*> m_line_edits;

    bool m_needFreshDetection = false;
    bool m_camera_move_done = true;
    bool m_afterTargetBlock = false;
    int  m_afterTargetDelayMs = 200;
    bool m_draw_targets = false;
};
#endif // MAINWINDOW_H
