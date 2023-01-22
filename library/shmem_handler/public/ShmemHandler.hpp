#ifndef SHMEMHANDLER_H
#define SHMEMHANDLER_H

#include <semaphore.h>
#include <cstdint>


#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <chrono>

template <typename T>
class ShmemHandler {
public:
    ShmemHandler(const char * shmem_name, int data_bins, const char * pid = '\0',bool use_semaphores = false, const char * sem_name = '\0', bool is_reader = false);
    ~ShmemHandler();

    bool createShmem();
    bool openShmem();

    bool shmemWrite(const T *);

    bool shmemRead(T *);

    bool readShmemId();

public:
    static constexpr const char * SHMEM_IDENTIFIER_PATH = "/tmp/arm_shm/";
    static constexpr const char * SHMEM_IDENTIFIER_NAME = "_shmem_identifier.txt";

private:
    bool m_use_semaphores;
    bool m_is_reader;
    std::string m_shmem_name;
    std::string m_shmem_name_with_id;
    std::string m_sem_name;
    std::int64_t m_shmem_fd;
    T * m_data;
    std::string m_identifier_num;
    int m_shmem_data_bins;
    sem_t * m_writer_sem;
    sem_t * m_reader_sem;
    std::string m_writer_sem_name;
    std::string m_reader_sem_name;
    std::string m_shmem_identifier_path;

    bool m_is_shmem_opened = false;
};

template <typename T>
ShmemHandler<T>::ShmemHandler(const char * shmem_name, int data_bins, const char * pid, bool use_semaphores, const char * sem_name, bool is_reader) :
    m_shmem_data_bins(data_bins),
    m_identifier_num(pid),
    m_sem_name(sem_name),
    m_use_semaphores(use_semaphores),
    m_is_reader(is_reader)
{
    m_shmem_name = std::string(shmem_name);
    m_shmem_name_with_id = m_shmem_name + m_identifier_num;
    m_writer_sem_name = std::string(m_sem_name) + "_writer_" + m_identifier_num;
    m_reader_sem_name = std::string(m_sem_name) + "_reader_" + m_identifier_num;
    std::cout << m_writer_sem_name << std::endl;
    std::cout << m_reader_sem_name << std::endl;

    m_shmem_identifier_path = std::string(SHMEM_IDENTIFIER_PATH) + std::string(m_shmem_name) + std::string(SHMEM_IDENTIFIER_NAME);
}

template <typename T>
ShmemHandler<T>::~ShmemHandler()
{
    if (!m_is_reader)
    {
        std::cout << "Closing shmem." << std::endl;
        munmap(m_data, m_shmem_data_bins);
        close(m_shmem_fd);
        shm_unlink(m_shmem_name_with_id.c_str());
    }
}

template <typename T>
bool ShmemHandler<T>::createShmem()
{
    std::cout << "Creating shmem fd: " << m_shmem_name_with_id << std::endl;
    m_shmem_fd = shm_open(m_shmem_name_with_id.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);

    if (m_shmem_fd < 0)
    {
        std::cerr << "Couldn't open " + m_shmem_name_with_id << " errno: " << m_shmem_fd << std::endl;
        return 0;
    }
    else
    {
        if (!std::filesystem::exists(SHMEM_IDENTIFIER_PATH))
        {
            if (!std::filesystem::create_directories(SHMEM_IDENTIFIER_PATH))
            {
                std::cerr << "Couldn't create directory " + std::string(SHMEM_IDENTIFIER_PATH) << std::endl;
                return 0;
            }
            else 
            {
                std::ofstream ofs(m_shmem_identifier_path, std::ofstream::out);
                ofs << m_identifier_num;
                ofs.close();
                ftruncate(m_shmem_fd, m_shmem_data_bins);
                m_data = (T *)mmap(0, m_shmem_data_bins, PROT_READ | PROT_WRITE, MAP_SHARED, m_shmem_fd, 0);
            }
        }
        else 
        {
            std::ofstream ofs(m_shmem_identifier_path, std::ofstream::out);
            ofs << m_identifier_num;
            ofs.close();
            ftruncate(m_shmem_fd, m_shmem_data_bins);
            m_data = (T *)mmap(0, m_shmem_data_bins, PROT_READ | PROT_WRITE, MAP_SHARED, m_shmem_fd, 0);
        }

        // Fill shmem with neutral values
        for (int i = 0; i < m_shmem_data_bins; ++i)
        {
            if (i < 3)
            {
                m_data[i] = 0;
            }
            else
            {
                m_data[i] = 127;
            }
        }

        if (m_use_semaphores)
        {
            if ((m_writer_sem = sem_open(m_writer_sem_name.c_str(), O_CREAT, 0660, 0)) == SEM_FAILED)
            {
                std::cerr << "Couldn't create semaphore " + m_writer_sem_name << std::endl;
                return 0;
            }
            if ((m_reader_sem = sem_open(m_reader_sem_name.c_str(), O_CREAT, 0660, 0)) == SEM_FAILED)
            {
                std::cerr << "Couldn't create semaphore " + m_reader_sem_name << std::endl;
                return 0;
            }
            if (sem_post(m_writer_sem) == -1)
            {
                std::cerr << "Couldn't post writer semaphore." << std::endl;
                return 0;
            }
            if (sem_post(m_reader_sem) == -1)
            {
                std::cerr << "Couldn't post reader semaphore." << std::endl;
                return 0;
            }
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
            m_shmem_fd = shm_open(m_shmem_name_with_id.c_str(), O_RDONLY, 0666);
            std::cout << "FD is: " << m_shmem_fd << std::endl;
            if (m_shmem_fd < 0)
            {
                std::cerr << "Couldn't open shmem fd." << std::endl;
                return 0;
            }
            m_data = (T *)mmap(0, m_shmem_data_bins, PROT_READ, MAP_SHARED, m_shmem_fd, 0);

            std::cout << "Opening semaphores: " << m_writer_sem_name << " " << m_reader_sem_name << std::endl;
            if ((m_writer_sem = sem_open(m_writer_sem_name.c_str(), 0, 0, 0)) == SEM_FAILED)
            {
                std::cerr << "Couldn't create semaphore " + m_writer_sem_name << std::endl;
                return 0;
            }
            if ((m_reader_sem = sem_open(m_reader_sem_name.c_str(), 0, 0, 0)) == SEM_FAILED)
            {
                std::cerr << "Couldn't create semaphore " + m_reader_sem_name << std::endl;
                return 0;
            }
            m_is_shmem_opened = true;
            return 1;
        }
        return 1;
    }
    std::cout << "Returning 0" << std::endl;
    return 0;
}

template <typename T>
bool ShmemHandler<T>::shmemWrite(const T * data)
{
    std::cout << "Writing to shmem fd." << std::endl;
    if (sem_wait(m_writer_sem) == -1)
    {
        std::cerr << "Couldn't wait writer semaphore." << std::endl;
        return 0;
    }
    if (sem_post(m_reader_sem) == -1)
    {
        std::cerr << "Couldn't post reader semaphore." << std::endl;
        return 0;
    }
    for (int i = 0; i < m_shmem_data_bins; ++i)
    {
        m_data[i] = data[i];
    }
    if (sem_wait(m_reader_sem) == -1)
    {
        std::cerr << "Couldn't wait reader semaphore." << std::endl;
        return 0;
    }
    if (sem_post(m_writer_sem) == -1)
    {
        std::cerr << "Couldn't post writer semaphore." << std::endl;
        return 0;
    }
    return 1;
}

template <typename T>
bool ShmemHandler<T>::shmemRead(T * data)
{
    if (sem_wait(m_reader_sem) == -1)
    {
        std::cerr << "Couldn't wait reader semaphore." << std::endl;
        return 0;
    }
    if (sem_post(m_writer_sem) == -1)
    {
        std::cerr << "Couldn't post writer semaphore." << std::endl;
        return 0;
    }
    for (int i = 0; i < m_shmem_data_bins; ++i)
    {
        data[i] = m_data[i];
    }
    if (sem_wait(m_writer_sem) == -1)
    {
        std::cerr << "Couldn't wait writer semaphore." << std::endl;
        return 0;
    }
    if (sem_post(m_reader_sem) == -1)
    {
        std::cerr << "Couldn't post reader semaphore." << std::endl;
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
        std::cerr << "failed to open " << m_shmem_identifier_path << '\n';
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    else
    {
        std::string obtained_pid;
        getline(ifs, obtained_pid);
        if (obtained_pid.compare(m_identifier_num) != 0)
        {
            std::cout << "m_identifier_num: " << m_identifier_num << std::endl;
            std::cout << "Obtained pid: " << obtained_pid << std::endl;
            // if previous shmem fd was opened, close it before opening another
            if (m_is_shmem_opened)
            {
                std::cout << "Shmem fd changed, closing previous one." << std::endl;
                munmap(m_data, m_shmem_data_bins);
                close(m_shmem_fd);
                shm_unlink(m_shmem_name_with_id.c_str());
            }
            m_is_shmem_opened = false;
            std::cout << "Reading Joypad Manager PID" << std::endl;
            m_identifier_num = obtained_pid;
            std::cout << "m_identifier_num: " << m_identifier_num << std::endl;
            std::cout << "Obtained pid: " << obtained_pid << std::endl;
            m_writer_sem_name = m_sem_name + "_writer_" + m_identifier_num;
            m_reader_sem_name = m_sem_name + "_reader_" + m_identifier_num;
            m_shmem_name_with_id = m_shmem_name + m_identifier_num;
            return 1;
        }
        return 1;
    }
    return 0;
}

#endif //SHMEMHANDLER_H