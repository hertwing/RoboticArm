#include "ScriptedMotionWorker.h"
#include <QCoreApplication>
#include <QThread>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

ScriptedMotionWorker::ScriptedMotionWorker(QObject* parent)
    : QObject(parent), m_stop_requested(false)
{
    std::cout << "Creating scripted motion request SHMEM fd." << std::endl;
    m_scripted_motion_request_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<ScriptedMotionStepData>>(
        odin::shmem_wrapper::DataTypes::SCRIPTED_MOTION_REQUEST_STATUS_SHMEM_NAME, sizeof(ScriptedMotionStepData), true);
    std::cout << "Scripted motion request SHMEM fd created." << std::endl;
    std::cout << "Creating scripted motion reply SHMEM fd." << std::endl;
    m_scripted_motion_reply_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<ScriptedMotionStepData>>(
        odin::shmem_wrapper::DataTypes::SCRIPTED_MOTION_REPLY_STATUS_SHMEM_NAME, sizeof(ScriptedMotionStepData), true);
    std::cout << "Scripted motion reply SHMEM fd created." << std::endl;
    std::cout << "Creating scripted motion servo step info SHMEM fd." << std::endl;
    m_scripted_motion_step_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinServoStep>>(
        odin::shmem_wrapper::DataTypes::SCRIPTED_MOTION_SERVO_STEP_SHMEM_NAME, sizeof(OdinServoStep), true);
    std::cout << "Scripted motion servo step SHMEM fd created." << std::endl;
}

void ScriptedMotionWorker::setStatusPointer(std::shared_ptr<scripted_motion_status_t> statusPtr)
{
    m_scripted_motion_request_status = std::move(statusPtr);
}

void ScriptedMotionWorker::setStepsVectorPtr(std::shared_ptr<std::vector<OdinServoStep>> stepsVectorPtr)
{
    m_automatic_steps = std::move(stepsVectorPtr);
}

void ScriptedMotionWorker::setRunInLoop(bool run_in_loop)
{
    m_run_in_loop = run_in_loop;
}

void ScriptedMotionWorker::processMotion()
{
    m_stop_requested = false;
    enum class Phase { StartReq, HandleReq, WaitStepCompleted, EndReq, StopReq };
    Phase phase = Phase::StartReq;
    // Save current step index for GUI table
    m_current_step_index = 0;

    auto req_status = ScriptedMotionStatus::IDLE;
    auto rep_status = ScriptedMotionStatus::IDLE;

    ScriptedMotionStepData req_data{m_current_step_index, req_status};
    ScriptedMotionStepData rep_data{m_current_step_index, rep_status};

    while (!m_stop_requested)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        switch(phase)
        {
            case Phase::StartReq:
            {
                std::cout << "Start req" << std::endl;
                m_current_step_index = 0;
                phase = Phase::HandleReq;
                break;
            }
            case Phase::HandleReq:
            {
                req_data.step_num = m_current_step_index;
                req_data.step_status = ScriptedMotionStatus::START_REQUEST;
                m_scripted_motion_request_shmem_handler->shmemWrite(&req_data);
                std::cout << "Handling step num: " << m_current_step_index << std::endl;
                if (m_current_step_index < m_automatic_steps->size())
                {
                    m_scripted_motion_step_shmem_handler->shmemWrite(&(m_automatic_steps->at(m_current_step_index)));
                    auto s = m_automatic_steps->at(m_current_step_index);
                    std::cout << +s.step_num << std::endl;
                    std::cout << +s.servo_num << std::endl;
                    std::cout << +s.position << std::endl;
                    std::cout << +s.speed << std::endl;
                    std::cout << +s.delay << std::endl;
                    phase = Phase::WaitStepCompleted;
                }
                else
                {
                    phase = Phase::EndReq;
                }
                break;
            }
            case Phase::WaitStepCompleted:
            {
                if (!m_scripted_motion_reply_shmem_handler->shmemRead(&rep_data))
                {
                    QThread::msleep(5);
                    break;
                }
                if (rep_data.step_num == m_current_step_index && rep_data.step_status == ScriptedMotionStatus::REQUEST_COMPLETED)
                {
                    ++m_current_step_index;
                    req_data.step_num = m_current_step_index;
                    phase = Phase::HandleReq;
                }
                break;
            }
            case Phase::StopReq:
            {
                break;
            }
            case Phase::EndReq:
            {
                m_current_step_index = 0;
                req_data.step_num = m_current_step_index;
                req_data.step_status = ScriptedMotionStatus::IDLE;
                m_scripted_motion_request_shmem_handler->shmemWrite(&req_data);
                if (!m_run_in_loop)
                {
                    m_stop_requested = true;
                    break;
                }
                phase = Phase::StartReq;
                break;
            }
            default:
                break;
        }
    }

    emit motionCompleted();
}

void ScriptedMotionWorker::stopMotion()
{
    m_stop_requested = true;
    m_current_step_index = 0;
    ScriptedMotionStepData req_data{m_current_step_index, ScriptedMotionStatus::IDLE};
    m_scripted_motion_request_shmem_handler->shmemWrite(&req_data);
}