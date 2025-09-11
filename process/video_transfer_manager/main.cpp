#include "odin/video_transfer_manager/VideoTransferManager.h"

#include <signal.h>

using namespace odin::video_transfer;

int main()
{
    signal(SIGPIPE, SIG_IGN);
    VideoTransferManager video_transfer_manager;
    signal(SIGINT, VideoTransferManager::signalCallbackHandler);
    video_transfer_manager.runProcess();
    return 0;
}