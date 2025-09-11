#ifndef VIDEOTRANSFERMANAGER_H
#define VIDEOTRANSFERMANAGER_H

#include "odin/video_handler/VideoHandler.h"

using namespace odin::video_handler;

namespace odin
{
namespace video_transfer
{

class VideoTransferManager
{
public:
    VideoTransferManager();
    ~VideoTransferManager() = default;

    void runProcess();

    static bool m_run_process;
    static void signalCallbackHandler(int signum);
private:
    VideoHandler m_video_handler;
    bool m_is_camera_connected;
};

} // video_transfer
} // odin

#endif