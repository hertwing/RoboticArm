#include "odin/odin_comm/OdinComm.hpp"

#include <iostream>

using namespace odin::odin_comm;

int main()
{
    OdinCommWriter<ControlSelection, Udp> writer;

    const auto status = writer.write(
        ControlSelection{
            .selectedMode = ControlMode::ScriptedMotion
        }
    );

    if (!status) {
        std::cout << "Write failed: "
                  << toString(status.error())
                  << '\n';

        return 1;
    }

    std::cout << "Written\n";

    return 0;
}