#ifndef CAMERAROTORPROCESS_H
#define CAMERAROTORPROCESS_H

#include "CameraRotor.h"

class CameraRotorProcess : public CameraRotor
{
public:
    CameraRotorProcess();
    ~CameraRotorProcess() = default;
    void TestFunProc();
private:
    CameraRotor m_camera_rotor;
};

#endif // CAMERAROTORPROCESS_H