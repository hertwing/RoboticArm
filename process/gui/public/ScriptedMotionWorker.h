#ifndef SCRIPTEDMOTIONWORKTER_H
#define SCRIPTEDMOTIONWORKTER_H

#include <QObject>

#include "InetCommData.h"
#include "odin/shmem_wrapper/ShmemHandler.hpp"

#include <vector>
#include <memory>

class ScriptedMotionWorker : public QObject
{
    Q_OBJECT

public:
    explicit ScriptedMotionWorker(QObject* parent = nullptr);
    void setStatusPointer(std::shared_ptr<scripted_motion_status_t> statusPtr);
    void setStepsVectorPtr(std::shared_ptr<std::vector<OdinServoStep>> stepsVectorPtr);
    void setRunInLoop(bool loop);
    std::uint64_t m_current_step_index = 0;
signals:
    void motionCompleted();

public slots:
    void processMotion();
    void stopMotion();

private:
    bool m_run_in_loop = false;
    bool m_stop_requested;
    std::shared_ptr<scripted_motion_status_t> m_scripted_motion_request_status;
    std::shared_ptr<std::vector<OdinServoStep>> m_automatic_steps;

    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<ScriptedMotionStepData>> m_scripted_motion_request_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<ScriptedMotionStepData>> m_scripted_motion_reply_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<OdinServoStep>> m_scripted_motion_step_shmem_handler;
};

#endif