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
        m_identifier_num = std::to_string(getpid()).c_str();
        m_shmem_name_with_id = m_shmem_name + m_identifier_num;

        m_reader_blocking_shmem_name = DataTypes::SHMEM_READER_BLOCKER_NAME + m_shmem_name + m_identifier_num;

        m_writer_sem_name = m_writer_sem_prefix + m_shmem_name + m_identifier_num;
        m_reader_sem_name = m_reader_sem_prefix + m_shmem_name + m_identifier_num;
        m_blocking_sem_name = m_blocking_sem_prefix + m_shmem_name + m_identifier_num;

        //   createShmem();
        while(!createShmem() && m_run_process)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        };
    } else {
        //   openShmem();
        while(!openShmem() && m_run_process)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        };
    }
}

template <typename T>
ShmemHandler<T>::~ShmemHandler()
{
    // Close shmem fd only if owner process is closing
    if (m_is_owner)
    {
        std::cout << "Closing shmem." << std::endl;
        munmap(m_data, m_shmem_data_size);
        munmap(m_reader_blocker_crc, sizeof(std::uint8_t));
        closeShmem();
        shm_unlink(m_shmem_name_with_id.c_str());
    }
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
        return 0;
    }
    else
    {
        if (!std::filesystem::exists(DataTypes::SHMEM_IDENTIFIER_PATH))
        {
            if (!std::filesystem::create_directories(DataTypes::SHMEM_IDENTIFIER_PATH))
            {
                std::cout << "Couldn't create directory " + static_cast<std::string>(DataTypes::SHMEM_IDENTIFIER_PATH) << std::endl;
                closeShmem();
                return 0;
            }
            else
            {
                std::ofstream ofs(m_shmem_identifier_path, std::ofstream::out);
                ofs << m_identifier_num;
                ofs.close();
                ftruncate(m_shmem_fd, m_shmem_data_size);
                ftruncate(m_reader_blocker_fd, sizeof(std::uint8_t));
                m_data = (T *)mmap(0, m_shmem_data_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_shmem_fd, 0);
                m_reader_blocker_crc = (std::uint8_t *)mmap(0, sizeof(std::uint8_t), PROT_READ | PROT_WRITE, MAP_SHARED, m_reader_blocker_fd, 0);
            }
        }
        else 
        {
            std::ofstream ofs(m_shmem_identifier_path, std::ofstream::out);
            ofs << m_identifier_num;
            ofs.close();
            ftruncate(m_shmem_fd, m_shmem_data_size);
            ftruncate(m_reader_blocker_fd, sizeof(std::uint8_t));
            m_data = (T *)mmap(0, m_shmem_data_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_shmem_fd, 0);
            m_reader_blocker_crc = (std::uint8_t *)mmap(0, sizeof(std::uint8_t), PROT_READ | PROT_WRITE, MAP_SHARED, m_reader_blocker_fd, 0);
        }

        std::cout << "Opening semaphores: " << m_writer_sem_name << " " << m_reader_sem_name << " " << m_blocking_sem_name << std::endl;
        if ((m_writer_sem = sem_open(m_writer_sem_name.c_str(), O_CREAT, 0660, 0)) == SEM_FAILED)
        {
            std::cout << "Couldn't create semaphore " + m_writer_sem_name << std::endl;
            // TODO: Delete shmem if coudln't create semaphores
            sem_close(m_writer_sem);
            closeShmem();
            return 0;
        }
        if ((m_reader_sem = sem_open(m_reader_sem_name.c_str(), O_CREAT, 0660, 0)) == SEM_FAILED)
        {
            std::cout << "Couldn't create semaphore " + m_reader_sem_name << std::endl;
            sem_close(m_reader_sem);
            closeShmem();
            return 0;
        }
        if ((m_blocking_sem = sem_open(m_blocking_sem_name.c_str(), O_CREAT, 0660, 0)) == SEM_FAILED)
        {
            std::cout << "Couldn't create semaphore " + m_blocking_sem_name << std::endl;
            sem_close(m_blocking_sem);
            closeShmem();
            return 0;
        }
        std::cout << "Posting reader and writer semaphores after creation: " << m_writer_sem_name << ", " << m_reader_sem_name << std::endl;
        if (sem_post(m_writer_sem) == -1)
        {
            std::cout << "Couldn't post writer semaphore." << std::endl;
            sem_close(m_reader_sem);
            closeShmem();
            return 0;
        }
        if (sem_post(m_reader_sem) == -1)
        {
            std::cout << "Couldn't post reader semaphore." << std::endl;
            sem_close(m_reader_sem);
            closeShmem();
            return 0;
        }
    }
    return 1;
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
                std::cout << "Couldn't open shmem fd." << std::endl;
                // std::this_thread::sleep_for(std::chrono::seconds(1));
                closeShmem();
                return 0;
            }
            m_data = (T *)mmap(0, m_shmem_data_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_shmem_fd, 0);
            m_reader_blocker_crc = (std::uint8_t *)mmap(0, sizeof(std::uint8_t), PROT_READ | PROT_WRITE, MAP_SHARED, m_reader_blocker_fd, 0);

            std::cout << "Opening semaphore: " << m_blocking_sem_name << std::endl;
            if ((m_blocking_sem = sem_open(m_blocking_sem_name.c_str(), 0, 0, 0)) == SEM_FAILED)
            {
                std::cout << "Couldn't open semaphore " + m_blocking_sem_name << std::endl;
                sem_close(m_blocking_sem);
                closeShmem();
                return 0;
            }
            std::cout << "Opening semaphore: " << m_writer_sem_name << std::endl;
            if ((m_writer_sem = sem_open(m_writer_sem_name.c_str(), 0, 0, 0)) == SEM_FAILED)
            {
                std::cout << "Couldn't open semaphore " + m_writer_sem_name << std::endl;
                sem_close(m_writer_sem);
                closeShmem();
                return 0;
            }
            std::cout << "Opening semaphore: " << m_reader_sem_name << std::endl;
            if ((m_reader_sem = sem_open(m_reader_sem_name.c_str(), 0, 0, 0)) == SEM_FAILED)
            {
                std::cout << "Couldn't open semaphore " + m_reader_sem_name << std::endl;
                sem_close(m_reader_sem);
                closeShmem();
                return 0;
            }
            m_is_shmem_opened = true;
            std::cout << "Shmem opened." << std::endl;
            return 1;
        }
        return 1;
    }
    std::cout << "Error occured during opening shared memory." << std::endl;
    return 0;
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
            return 0;
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
        std::uint8_t * int_data_cast = (std::uint8_t *)data;
        for (std::uint32_t i = 0; i < m_shmem_data_size; ++i)
        {
            crc += int_data_cast[i];
        }
    }

    if (sem_wait(m_writer_sem) == -1)
    {
        std::cout << "sem_wait for writer semaphore failed." << std::endl;
        return 0;
    }
    if (sem_post(m_reader_sem) == -1)
    {
        std::cout << "sem_post post for reader semaphore failed." << std::endl;
        return 0;
    }

    std::memcpy(m_data, data, m_shmem_data_size);

    if (sem_wait(m_reader_sem) == -1)
    {
        std::cout << "sem_wait for reader semaphore failed." << std::endl;
        return 0;
    }
    if (sem_post(m_writer_sem) == -1)
    {
        std::cout << "sem_post for writer semaphore failed." << std::endl;
        return 0;
    }

    if (blocking)
    {
        bool was_read = false;
        std::int64_t timeout = 0;
        while (!was_read && timeout < 5000)
        {
            if (sem_wait(m_reader_sem) == -1)
            {
                std::cout << "sem_wait for reader semaphore failed." << std::endl;
                return 0;
            }
            if (sem_post(m_writer_sem) == -1)
            {
                std::cout << "sem_post for writer semaphore failed." << std::endl;
                return 0;
            }
            std::memcpy(&crc_confirm, m_reader_blocker_crc, sizeof(std::uint8_t));
            if (sem_wait(m_writer_sem) == -1)
            {
                std::cout << "sem_wait for writer semaphore failed." << std::endl;
                return 0;
            }
            if (sem_post(m_reader_sem) == -1)
            {
                std::cout << "sem_post for reader semaphore failed." << std::endl;
                return 0;
            }
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
            return 0;
        }
    }

    return 1;
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
            return 0;
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

    if (sem_wait(m_reader_sem) == -1)
    {
        std::cout << "sem_wait for reader semaphore failed." << std::endl;
        return 0;
    }
    if (sem_post(m_writer_sem) == -1)
    {
        std::cout << "sem_post for writer semaphore failed." << std::endl;
        return 0;
    }

    std::memcpy(data, m_data, m_shmem_data_size);

    if (blocking)
    {
        std::uint8_t * int_data_cast = (std::uint8_t *)data;
        for (std::uint32_t i = 0; i < m_shmem_data_size; ++i)
        {
            crc += int_data_cast[i];
        }
        if (sem_wait(m_writer_sem) == -1)
        {
            std::cout << "sem_wait for writer semaphore failed." << std::endl;
            return 0;
        }
        if (sem_post(m_reader_sem) == -1)
        {
            std::cout << "sem_post post for reader semaphore failed." << std::endl;
            return 0;
        }
        std::memcpy(m_reader_blocker_crc, &crc, sizeof(std::uint8_t));
        if (sem_wait(m_reader_sem) == -1)
        {
            std::cout << "sem_wait for reader semaphore failed." << std::endl;
            return 0;
        }
        if (sem_post(m_writer_sem) == -1)
        {
            std::cout << "sem_post for writer semaphore failed." << std::endl;
            return 0;
        }
    }

    if (sem_wait(m_writer_sem) == -1)
    {
        std::cout << "sem_wait for writer semaphore failed." << std::endl;
        return 0;
    }
    if (sem_post(m_reader_sem) == -1)
    {
        std::cout << "sem_post for reader semaphore failed." << std::endl;
        return 0;
    }
    return 1;
}

template <typename T>
bool ShmemHandler<T>::readShmemId()
{
    std::ifstream ifs(m_shmem_identifier_path);
    if (!ifs.is_open())
    {
        std::cout << "Failed to open " << m_shmem_identifier_path << std::endl;
        // std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    else
    {
        std::string obtained_pid;
        getline(ifs, obtained_pid);
        if (obtained_pid.compare(m_identifier_num) != 0)
        {
            m_is_shmem_opened = false;
            std::cout << "Reading shmem owner process PID." << std::endl;
            m_identifier_num = obtained_pid;
            std::cout << "Obtained PID: " << m_identifier_num << std::endl;
            m_writer_sem_name = m_writer_sem_prefix + m_shmem_name + m_identifier_num;
            m_reader_sem_name = m_reader_sem_prefix + m_shmem_name + m_identifier_num;
            m_blocking_sem_name = m_blocking_sem_prefix + m_shmem_name + m_identifier_num;
            m_shmem_name_with_id = m_shmem_name + m_identifier_num;
            m_reader_blocking_shmem_name = DataTypes::SHMEM_READER_BLOCKER_NAME + m_shmem_name + m_identifier_num;
            return 1;
        }
        return 1;
    }
    return 0;
}

template <typename T>
void ShmemHandler<T>::closeShmem()
{
    if (fcntl(m_shmem_fd, F_GETFD) != -1 || errno != EBADF)
    {
        close(m_shmem_fd);
        shm_unlink(m_shmem_name_with_id.c_str());
    }
    if (fcntl(m_reader_blocker_fd, F_GETFD) != -1 || errno != EBADF)
    {
        close(m_shmem_fd);
        shm_unlink(m_reader_blocking_shmem_name.c_str());
    }
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
