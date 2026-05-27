#include "odin/odin_comm/OdinComm.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>

using namespace std::chrono_literals;
using namespace odin::odin_comm;

int main()
{
    OdinCommReader<ControlSelection, Udp> reader;

    while (true) {
        auto result = reader.read(1000ms);

        if (!result) {
            if (result.error() == CommError::Timeout) {
                continue;
            }

            std::cout << "Read failed: "
                      << odin::odin_comm::toString(result.error())
                      << '\n';

            continue;
        }

        std::cout << "Read control mode: "
                  << static_cast<std::uint32_t>(result.value().selectedMode)
                  << '\n';
    }

    return 0;
}