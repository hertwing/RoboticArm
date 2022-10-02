#include "CameraRotorProcess.h"

CameraRotorProcess::CameraRotorProcess() : m_camera_rotor()
{
}

void CameraRotorProcess::TestFunProc()
{
    m_camera_rotor.TestFun();
}

int main(int argc, const char ** argv)
{
    CameraRotorProcess crp;
    crp.TestFunProc();

    return 0;
}