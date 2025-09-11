#include "odin/video_transfer_manager/VideoTransferManager.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace odin
{
namespace video_transfer
{

bool VideoTransferManager::m_run_process = true;

VideoTransferManager::VideoTransferManager() :
    m_video_handler(),
    m_is_camera_connected(false)
{
}

void VideoTransferManager::runProcess() {
    m_video_handler.requestRun(true);
    while (m_run_process) 
    {
        m_video_handler.tick();
        m_video_handler.run();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void VideoTransferManager::signalCallbackHandler(int signum)
{
    std::cout << "VideoTransferManager received signal: " << signum << std::endl;
    VideoHandler::signalCallbackHandler(signum);
    m_run_process = false;
}

} // video_transfer
} // odin