#ifndef SHMEMHANDLER_H
#define SHMEMHANDLER_H

#include "DataTypes.h"
#include <semaphore.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <chrono>

// TODO: Create config files for communication types. Not all processes needs to have both reader and writer rights.

namespace odin
{
namespace shmem_wrapper
{

template <typename T>
class ShmemHandler {
public:
    ShmemHandler(const char * shmem_name, std::uint32_t data_size, bool is_owner);
    ~ShmemHandler();

    bool createShmem();
    bool openShmem();
    bool shmemWrite(const T * data, const bool blocking = false);
    bool shmemRead(T * data, const bool blocking = false);
    bool readShmemId();

    static void signalCallbackHandler(int signum);

private:
    bool readOperation(T *, const bool blocking);
    bool writeOperation(const T *, const bool blocking);
    void closeShmem();

    inline static bool safeSemWait(sem_t* sem, const std::string& sem_name);
    inline static bool safeSemPost(sem_t* sem, const std::string& sem_name);

private:
    bool m_is_owner;
    bool m_is_shmem_opened;

    std::string m_shmem_name;
    std::string m_shmem_name_with_id;
    std::string m_writer_sem_name;
    std::string m_reader_sem_name;
    std::string m_blocking_sem_name;
    std::string m_shmem_identifier_path;
    std::string m_identifier_num;
    std::string m_reader_blocking_shmem_name;

    const std::string m_writer_sem_prefix = "writer_sem_";
    const std::string m_reader_sem_prefix = "reader_sem_";
    const std::string m_blocking_sem_prefix = "blocking_sem_";

    std::int64_t m_shmem_fd;
    std::int64_t m_reader_blocker_fd;
    
    T * m_data;
    std::uint8_t * m_reader_blocker_crc;

    std::uint32_t m_shmem_data_size;

    sem_t * m_writer_sem;
    sem_t * m_reader_sem;
    sem_t * m_blocking_sem;

    static bool m_run_process;
};

template <typename T>
bool ShmemHandler<T>::m_run_process = true;

template <typename T>
ShmemHandler<T>::ShmemHandler(const char * shmem_name, std::uint32_t data_size, bool is_owner) :
    m_shmem_data_size(data_size),
    m_is_owner(is_owner),
    m_identifier_num(""),
    m_is_shmem_opened(false)
{
    m_shmem_name = static_cast<std::string>(shmem_name);

    m_shmem_identifier_path = static_cast<std::string>(DataTypes::SHMEM_IDENTIFIER_PATH) + m_shmem_name + DataTypes::SHMEM_IDENTIFIER_NAME;

    if (m_is_owner)
    {
        m_identifier_num = std::to_string(getpid());
        m_shmem_name_with_id = m_shmem_name + m_identifier_num;

        m_reader_blocking_shmem_name = DataTypes::SHMEM_READER_BLOCKER_NAME + m_shmem_name + m_identifier_num;

        m_writer_sem_name = m_writer_sem_prefix + m_shmem_name + m_identifier_num;
        m_reader_sem_name = m_reader_sem_prefix + m_shmem_name + m_identifier_num;
        m_blocking_sem_name = m_blocking_sem_prefix + m_shmem_name + m_identifier_num;

        while(!createShmem() && m_run_process)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        };
    }
    else
    {
        while(!openShmem() && m_run_process)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        };
    }
}

template <typename T>
ShmemHandler<T>::~ShmemHandler()
{
    closeShmem();
}

template <typename T>
bool ShmemHandler<T>::createShmem()
{
    std::cout << "Creating shmem fd: " << m_shmem_name_with_id << std::endl;
    m_shmem_fd = shm_open(m_shmem_name_with_id.c_str(), O_CREAT | O_EXCL | O_RDWR, 0660);
    std::cout << "m_shmem_fd: " << m_shmem_fd << std::endl;

    std::cout << "Creating blocking reader fd: " << m_reader_blocking_shmem_name << std::endl;
    m_reader_blocker_fd = shm_open(m_reader_blocking_shmem_name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0660);
    std::cout << "m_reader_blocker_fd: " << m_reader_blocker_fd << std::endl;

    if ((m_shmem_fd < 0) || (m_reader_blocker_fd < 0))
    {
        std::cout << "Couldn't open " + m_shmem_name_with_id << " errno: " << m_shmem_fd << std::endl;
        closeShmem();
        return false;
    }
    else
    {
        if (!std::filesystem::exists(DataTypes::SHMEM_IDENTIFIER_PATH))
        {
            if (!std::filesystem::create_directories(DataTypes::SHMEM_IDENTIFIER_PATH))
            {
                std::cout << "Couldn't create directory " + static_cast<std::string>(DataTypes::SHMEM_IDENTIFIER_PATH) << std::endl;
                closeShmem();
                return false;
            }
        }
        std::ofstream ofs(m_shmem_identifier_path, std::ofstream::out);
        ofs << m_identifier_num;
        ofs.close();
        if (ftruncate(m_shmem_fd, m_shmem_data_size) == -1)
        { 
            std::cout << "Error during first ftruncate operation." << std::endl;
            closeShmem();
            return false; 
        }
        if (ftruncate(m_reader_blocker_fd, sizeof(std::uint8_t)) == -1)
        {
            std::cout << "Error during second ftruncate operation." << std::endl;
            closeShmem();
            return false; 
        }
        m_data = (T *)mmap(0, m_shmem_data_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_shmem_fd, 0);
        m_reader_blocker_crc = (std::uint8_t *)mmap(0, sizeof(std::uint8_t), PROT_READ | PROT_WRITE, MAP_SHARED, m_reader_blocker_fd, 0);

        if (m_data == MAP_FAILED || m_reader_blocker_crc == MAP_FAILED)
        {
            std::cout << "Failed to mmap memory." << std::endl;
            closeShmem();
            return false;
        }

        std::cout << "Opening semaphores: " << m_writer_sem_name << " " << m_reader_sem_name << " " << m_blocking_sem_name << std::endl;
        if ((m_writer_sem = sem_open(m_writer_sem_name.c_str(), O_CREAT, 0660, 0)) == SEM_FAILED)
        {
            std::cout << "Couldn't create semaphore " + m_writer_sem_name << std::endl;
            closeShmem();
            return false;
        }
        if ((m_reader_sem = sem_open(m_reader_sem_name.c_str(), O_CREAT, 0660, 0)) == SEM_FAILED)
        {
            std::cout << "Couldn't create semaphore " + m_reader_sem_name << std::endl;
            closeShmem();
            return false;
        }
        if ((m_blocking_sem = sem_open(m_blocking_sem_name.c_str(), O_CREAT, 0660, 0)) == SEM_FAILED)
        {
            std::cout << "Couldn't create semaphore " + m_blocking_sem_name << std::endl;
            closeShmem();
            return false;
        }
        // Set initial state of semaphores to "unlocked" (ready for operation)
        std::cout << "Posting reader and writer semaphores after creation: " << m_writer_sem_name << ", " << m_reader_sem_name << std::endl;
        if (sem_post(m_writer_sem) == -1)
        {
            std::cout << "Couldn't post writer semaphore." << std::endl;
            sem_close(m_reader_sem);
            closeShmem();
            return false;
        }
        if (sem_post(m_reader_sem) == -1)
        {
            std::cout << "Couldn't post reader semaphore." << std::endl;
            sem_close(m_reader_sem);
            closeShmem();
            return false;
        }
    }
    return true;
}

template <typename T>
bool ShmemHandler<T>::openShmem()
{
    if (readShmemId())
    {
        if (!m_is_shmem_opened)
        {
            std::cout << "Opening shmem fd: " << m_shmem_name_with_id << std::endl;
            m_shmem_fd = shm_open(m_shmem_name_with_id.c_str(), O_RDWR, 0666);
            std::cout << "Opening blocking reader fd: " << m_reader_blocking_shmem_name << std::endl;
            m_reader_blocker_fd = shm_open(m_reader_blocking_shmem_name.c_str(), O_RDWR, 0666);
            if (m_shmem_fd < 0)
            {
                std::cout << "Couldn't open shmem fd: " + m_shmem_name_with_id << std::endl;
                closeShmem();
                return false;
            }
            m_data = (T *)mmap(0, m_shmem_data_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_shmem_fd, 0);
            m_reader_blocker_crc = (std::uint8_t *)mmap(0, sizeof(std::uint8_t), PROT_READ | PROT_WRITE, MAP_SHARED, m_reader_blocker_fd, 0);

            if (m_data == MAP_FAILED || m_reader_blocker_crc == MAP_FAILED)
            {
                std::cout << "mmap failed for shared memory." << std::endl;
                closeShmem();
                return false;
            }

            std::cout << "Opening semaphore: " << m_blocking_sem_name << std::endl;
            if ((m_blocking_sem = sem_open(m_blocking_sem_name.c_str(), 0)) == SEM_FAILED)
            {
                std::cout << "Couldn't open semaphore " + m_blocking_sem_name << std::endl;
                closeShmem();
                return false;
            }
            std::cout << "Opening semaphore: " << m_writer_sem_name << std::endl;
            if ((m_writer_sem = sem_open(m_writer_sem_name.c_str(), 0)) == SEM_FAILED)
            {
                std::cout << "Couldn't open semaphore " + m_writer_sem_name << std::endl;
                closeShmem();
                return false;
            }
            std::cout << "Opening semaphore: " << m_reader_sem_name << std::endl;
            if ((m_reader_sem = sem_open(m_reader_sem_name.c_str(), 0)) == SEM_FAILED)
            {
                std::cout << "Couldn't open semaphore " + m_reader_sem_name << std::endl;
                closeShmem();
                return false;
            }
            m_is_shmem_opened = true;
            std::cout << "Shmem opened." << std::endl;
            return true;
        }
        return true;
    }
    std::cout << "Error occured during opening shared memory." << std::endl;
    return false;
}

template <typename T>
bool ShmemHandler<T>::shmemWrite(const T * data, const bool blocking)
{
    if (!m_is_owner)
    {
        if (openShmem())
        {
            return writeOperation(data, blocking);
        }
        else
        {
            return false;
        }
    }
    else
    {
        return writeOperation(data, blocking);
    }
}

template <typename T>
bool ShmemHandler<T>::writeOperation(const T * data, const bool blocking)
{
    uint8_t crc = 1;
    uint8_t crc_confirm = 0;
    if (blocking)
    {
        const std::uint8_t * int_data_cast = reinterpret_cast<const std::uint8_t *>(data);
        for (std::uint32_t i = 0; i < m_shmem_data_size; ++i)
        {
            crc += int_data_cast[i];
        }
    }

    if (!safeSemWait<T>(m_writer_sem, m_writer_sem_name)) return false;
    if (!safeSemPost<T>(m_reader_sem, m_reader_sem_name)) return false;

    std::memcpy(m_data, data, m_shmem_data_size);

    if (!safeSemWait<T>(m_reader_sem, m_reader_sem_name)) return false;
    if (!safeSemPost<T>(m_writer_sem, m_writer_sem_name)) return false;

    if (blocking)
    {
        bool was_read = false;
        std::int64_t timeout = 0;
        while (!was_read && timeout < 5000)
        {
            if (!safeSemWait<T>(m_reader_sem, m_reader_sem_name)) return false;
            if (!safeSemPost<T>(m_writer_sem, m_writer_sem_name)) return false;

            std::memcpy(&crc_confirm, m_reader_blocker_crc, sizeof(std::uint8_t));

            if (!safeSemWait<T>(m_writer_sem, m_writer_sem_name)) return false;
            if (!safeSemPost<T>(m_reader_sem, m_reader_sem_name)) return false;

            if (crc == crc_confirm)
            {
                std::cout << "Blocked message read." << std::endl;
                was_read = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            timeout+=10;
        }
        if (timeout == 5000)
        {
            std::cout << "Timeout reached." << std::endl;
            return false;
        }
    }
    return true;
}

template <typename T>
bool ShmemHandler<T>::shmemRead(T * data, const bool blocking)
{
    if (!m_is_owner)
    {
        if (openShmem())
        {
            return readOperation(data, blocking);
        }
        else
        {
            return false;
        }
    }
    else
    {
        return readOperation(data, blocking);
    }
}

template <typename T>
bool ShmemHandler<T>::readOperation(T * data, const bool blocking)
{
    uint8_t crc = 1;

    if (!safeSemWait<T>(m_reader_sem, m_reader_sem_name)) return false;
    if (!safeSemPost<T>(m_writer_sem, m_writer_sem_name)) return false;

    std::memcpy(data, m_data, m_shmem_data_size);

    if (blocking)
    {
        const std::uint8_t * int_data_cast = reinterpret_cast<const std::uint8_t *>(data);
        for (std::uint32_t i = 0; i < m_shmem_data_size; ++i)
        {
            crc += int_data_cast[i];
        }

        if (!safeSemWait<T>(m_writer_sem, m_writer_sem_name)) return false;
        if (!safeSemPost<T>(m_reader_sem, m_reader_sem_name)) return false;

        std::memcpy(m_reader_blocker_crc, &crc, sizeof(std::uint8_t));

        if (!safeSemWait<T>(m_reader_sem, m_reader_sem_name)) return false;
        if (!safeSemPost<T>(m_writer_sem, m_writer_sem_name)) return false;
    }

    if (!safeSemWait<T>(m_writer_sem, m_writer_sem_name)) return false;
    if (!safeSemPost<T>(m_reader_sem, m_reader_sem_name)) return false;

    return true;
}

template <typename T>
bool ShmemHandler<T>::readShmemId()
{
    std::ifstream ifs(m_shmem_identifier_path);
    if (!ifs.is_open())
    {
        std::cout << "Failed to open " << m_shmem_identifier_path << std::endl;
        return false;
    }

    std::string obtained_pid;
    getline(ifs, obtained_pid);
    if (obtained_pid != m_identifier_num)
    {
        m_is_shmem_opened = false;
        m_identifier_num = obtained_pid;

        std::cout << "Reading shmem owner process PID: " << m_identifier_num << std::endl;

        m_writer_sem_name = m_writer_sem_prefix + m_shmem_name + m_identifier_num;
        m_reader_sem_name = m_reader_sem_prefix + m_shmem_name + m_identifier_num;
        m_blocking_sem_name = m_blocking_sem_prefix + m_shmem_name + m_identifier_num;
        m_shmem_name_with_id = m_shmem_name + m_identifier_num;
        m_reader_blocking_shmem_name = DataTypes::SHMEM_READER_BLOCKER_NAME + m_shmem_name + m_identifier_num;
    }

    return true;
}

template <typename T>
bool ShmemHandler<T>::safeSemWait(sem_t* sem, const std::string& sem_name)
{
    if (sem_wait(sem) == -1)
    {
        std::cerr << "sem_wait failed on [" << sem_name << "]: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

template <typename T>
bool ShmemHandler<T>::safeSemPost(sem_t* sem, const std::string& sem_name)
{
    if (sem_post(sem) == -1)
    {
        std::cerr << "sem_post failed on [" << sem_name << "]: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

template <typename T>
void ShmemHandler<T>::closeShmem()
{
    // Unmap shared memory
    if (m_data && m_data != MAP_FAILED)
    {
        munmap(m_data, m_shmem_data_size);
        m_data = nullptr;
    }
    if (m_reader_blocker_crc && m_reader_blocker_crc != MAP_FAILED)
    {
        munmap(m_reader_blocker_crc, sizeof(std::uint8_t));
        m_reader_blocker_crc = nullptr;
    }

    // Close semaphores
    if (m_writer_sem) { sem_close(m_writer_sem); m_writer_sem = nullptr; }
    if (m_reader_sem) { sem_close(m_reader_sem); m_reader_sem = nullptr; }
    if (m_blocking_sem) { sem_close(m_blocking_sem); m_blocking_sem = nullptr; }

    // Close and unlink SHMEM
    if (m_shmem_fd >= 0)
    {
        close(m_shmem_fd);
        if (m_is_owner) shm_unlink(m_shmem_name_with_id.c_str());
        m_shmem_fd = -1;
    }
    if (m_reader_blocker_fd >= 0)
    {
        close(m_reader_blocker_fd);
        if (m_is_owner) shm_unlink(m_reader_blocking_shmem_name.c_str());
        m_reader_blocker_fd = -1;
    }

    m_is_shmem_opened = false;
}

template <typename T>
void ShmemHandler<T>::signalCallbackHandler(int signum)
{
    std::cout << "ShmemHandler received signal: " << signum << std::endl;
    m_run_process = false;
}

} // shmem_wrapper
} // odin

#endif //SHMEMHANDLER_H
