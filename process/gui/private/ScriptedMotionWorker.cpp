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
    m_scripted_motion_request_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<ScriptedMotionStepStatus>>(
        odin::shmem_wrapper::DataTypes::SCRIPTED_MOTION_REQUEST_STATUS_SHMEM_NAME, sizeof(ScriptedMotionStepStatus), true);
    std::cout << "Scripted motion request SHMEM fd created." << std::endl;
    std::cout << "Creating scripted motion reply SHMEM fd." << std::endl;
    m_scripted_motion_reply_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<ScriptedMotionStepStatus>>(
        odin::shmem_wrapper::DataTypes::SCRIPTED_MOTION_REPLY_STATUS_SHMEM_NAME, sizeof(ScriptedMotionStepStatus), true);
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

void ScriptedMotionWorker::processMotion()
{
    // if (!m_scripted_motion_request_status) return;
    // if (*m_scripted_motion_request_status == static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::NONE)) return;

    ScriptedMotionContext smc;

    std::uint64_t current_step = 0;
    ScriptedMotionStepStatus current_step_reply_status;
    ScriptedMotionStepStatus current_step_request_status;
    *m_scripted_motion_request_status = static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::START_REQUEST);
    current_step_request_status.step_status = *m_scripted_motion_request_status;
    current_step_request_status.step_num = current_step;
    if (!m_scripted_motion_request_shmem_handler->shmemWrite(&current_step_request_status))
    {
        std::cout << "Error while writing request info to gateway. Stopping request." << std::endl;
        m_stop_requested = true;
    }

    m_stop_requested = false;

    while (!m_stop_requested && current_step < (*m_automatic_steps).size())
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        if(!m_scripted_motion_reply_shmem_handler->shmemRead(&smc.local_reply))
        {
            std::cout << "Error while reading current step status from gateway. Stopping request." << std::endl;
            m_stop_requested = true;
        }
        if (smc.local_reply.step_num == current_step &&
            smc.local_reply.step_status == static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::WAITING) &&
            *m_scripted_motion_request_status == static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::START_REQUEST))
        {
            if (!m_scripted_motion_step_shmem_handler->shmemWrite(&((*m_automatic_steps).at(current_step))))
            {
                std::cout << "Error while writing step info to gateway. Stopping request." << std::endl;
                m_stop_requested = true;
            }
            *m_scripted_motion_request_status = static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::EXECUTE_ON_ARM);
            current_step_request_status.step_num = current_step;
            current_step_request_status.step_status = *m_scripted_motion_request_status;
            if (!m_scripted_motion_request_shmem_handler->shmemWrite(&current_step_request_status))
            {
                std::cout << "Error while writing request about step execution on ARM to gateway. Stopping request." << std::endl;
                m_stop_requested = true;
            }
        }
        if (current_step_reply_status.step_num == current_step &&
            current_step_reply_status.step_status == static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::COMPLETED))
        {
            ++current_step;
            if (current_step < (*m_automatic_steps).size())
            {
                *m_scripted_motion_request_status = static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::START_REQUEST);
                current_step_request_status.step_num = current_step;
                current_step_request_status.step_status = *m_scripted_motion_request_status;
                if (!m_scripted_motion_request_shmem_handler->shmemWrite(&current_step_request_status))
                {
                    std::cout << "Error while writing request info to gateway. Stopping request." << std::endl;
                    m_stop_requested = true;
                }
            }
            else
            {
                *m_scripted_motion_request_status = static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::REQUEST_COMPLETE);
                current_step_request_status.step_status = *m_scripted_motion_request_status;
                if (!m_scripted_motion_request_shmem_handler->shmemWrite(&current_step_request_status))
                {
                    std::cout << "Error while writing request complete info to gateway." << std::endl;
                    m_stop_requested = true;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    *m_scripted_motion_request_status = static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::NONE);
    current_step_request_status.step_status = *m_scripted_motion_request_status;
    if (!m_scripted_motion_request_shmem_handler->shmemWrite(&current_step_request_status))
    {
        std::cout << "Error while writing request info to gateway after request was done." << std::endl;
    }

    emit motionCompleted();
}

void ScriptedMotionWorker::stopMotion()
{
    m_stop_requested = true;
    // Write STOP to shmem

}