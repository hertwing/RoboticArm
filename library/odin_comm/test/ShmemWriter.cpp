#include "odin/odin_comm/OdinComm.hpp"

#include <iostream>

using namespace odin::odin_comm;

int main()
{
    OdinCommWriter<JoypadData, Shmem> writer;

    JoypadData data{};
    data.data[0] = 42;

    auto status = writer.write(data);

    if (!status) {
        std::cout << "Write failed: "
                  << odin::odin_comm::toString(status.error())
                  << '\n';

        return 1;
    }

    std::cout << "Written\n";
    std::cout << "Press Enter to exit...\n";
    std::cin.get();

    return 0;
}