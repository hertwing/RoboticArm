#include "odin/odin_comm/OdinComm.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>

using namespace std::chrono_literals;
using namespace odin::odin_comm;

int main()
{

    OdinCommReader<JoypadData, Shmem> reader;

    while (true) {
        auto result = reader.read();

        if (!result) {
            if (result.error() == odin::odin_comm::CommError::Timeout) {
                continue;
            }

            std::cout << "Read failed: "
                      << odin::odin_comm::toString(result.error())
                      << '\n';

            continue;
        }

        std::cout << "Read Joypad Data: "
                  << static_cast<std::uint32_t>(result.value().data[0])
                  << '\n';
    }

    return 0;
}