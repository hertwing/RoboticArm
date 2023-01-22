#ifndef DATATYPES_H
#define DATATYPES_H

namespace ShmemWrapper {

struct DataTypes {
    static constexpr const char * SHMEM_IDENTIFIER_PATH = "/tmp/arm_shm/";
    static constexpr const char * SHMEM_IDENTIFIER_NAME = "_shmem_identifier.txt";

    static constexpr const char * JOYPAD_SHMEM_NAME = "ControllerShmem";
    static constexpr const char * JOYPAD_SEM_NAME = "JoypadSem";
};

} // ShmemWrapper

#endif // DATATYPES_H