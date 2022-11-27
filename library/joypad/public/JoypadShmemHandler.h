#ifndef JOYPADSHMEMHANDLER_H
#define JOYPADSHMEMHANDLER_H

#include "JoypadData.h"

#include <string>
#include <cstdint>
#include <semaphore.h>

class JoypadShmemHandler
{
public:
    JoypadShmemHandler();
    ~JoypadShmemHandler();

    void writeJoypadData(JoypadData);
    static constexpr const char * SHMEM_NAME = "controller_shmem_";
    static constexpr const char * SHMEM_IDENTIFIER_PATH = "/tmp/arm_shm/";
    static constexpr const char * SHMEM_IDENTIFIER_NAME = "shmem_identifier.txt";

    static constexpr const char * WRITER_SEM_NAME = "controller_writer_sem_";
    static constexpr const char * READER_SEM_NAME = "controller_reader_sem_";
private:
    // sem_t *mutex_sem, *buffer_count_sem, *spool_signal_sem;
    std::int64_t m_fd_shm;
    char m_buffer [256];
    bool m_shmem_created = false;
    std::uint8_t * m_data;
    sem_t * writer_sem;
    sem_t * reader_sem;

private:
    void createJoypadShmem();
};

#endif // JOYPADSHMEMHANDLER_H