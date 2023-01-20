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

template <typename T>
class ShmemHandler {
public:
    ShmemHandler(const char * shmem_name, const char * pid, const char * p_name, int data_bins, bool use_semaphores = false, const char * sem_name = '\0', bool is_reader = false);
    ~ShmemHandler();

    bool createShmem();
    bool openShmem();

    bool shmemWrite(const T *);

    bool shmemRead(T *);

public:
    static constexpr const char * SHMEM_IDENTIFIER_PATH = "/tmp/arm_shm/";
    static constexpr const char * SHMEM_IDENTIFIER_NAME = "_shmem_identifier.txt";

private:
    bool m_use_semaphores;
    bool m_is_reader;
    std::string m_shmem_name;
    std::string m_sem_name;
    std::int64_t m_writer_shm_fd;
    std::int64_t m_reader_shm_fd;
    T * m_data;
    const char * m_identifier_num;
    const char * m_identifier_prefix;
    int m_shmem_data_bins;
    sem_t * m_writer_sem;
    sem_t * m_reader_sem;
    std::string m_sem_writer_name;
    std::string m_sem_reader_name;
};



template <typename T>
ShmemHandler<T>::ShmemHandler(const char * shmem_name, const char * pid, const char * p_name, int data_bins, bool use_semaphores, const char * sem_name, bool is_reader) :
    m_identifier_num(pid),
    m_identifier_prefix(p_name),
    m_shmem_data_bins(data_bins),
    m_sem_name(sem_name),
    m_use_semaphores(use_semaphores),
    m_is_reader(is_reader)
{
    m_shmem_name = std::string(shmem_name) + m_identifier_num;
    m_sem_writer_name = std::string(m_sem_name) + "_writer_" + m_identifier_num;
    m_sem_reader_name = std::string(m_sem_name) + "_reader_" + m_identifier_num;
    std::cout << m_sem_writer_name << std::endl;
    std::cout << m_sem_reader_name << std::endl;

    if (m_is_reader)
    {
        std::cout << "Opening semaphores." << std::endl;
        if ((m_writer_sem = sem_open(m_sem_writer_name.c_str(), 0, 0, 0)) == SEM_FAILED)
        {
            std::cerr << "Couldn't create semaphore " + m_sem_writer_name << std::endl;
        }
        if ((m_reader_sem = sem_open(m_sem_reader_name.c_str(), 0, 0, 0)) == SEM_FAILED)
        {
            std::cerr << "Couldn't create semaphore " + m_sem_reader_name << std::endl;
        }
    }
}

template <typename T>
ShmemHandler<T>::~ShmemHandler()
{
    std::cout << "Closing shmem." << std::endl;
    munmap(m_data, m_shmem_data_bins);
    close(m_reader_shm_fd);
    close(m_writer_shm_fd);
    shm_unlink(m_shmem_name.c_str());
}

template <typename T>
bool ShmemHandler<T>::createShmem()
{
    std::cout << "Creating shmem fd: " << m_shmem_name << std::endl;
    m_writer_shm_fd = shm_open(m_shmem_name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);

    if (m_writer_shm_fd < 0)
    {
        std::cerr << "Couldn't open " + m_shmem_name << " errno: " << m_writer_shm_fd << std::endl;
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
                std::ofstream ofs(std::string(SHMEM_IDENTIFIER_PATH) + std::string(m_identifier_prefix) + std::string(SHMEM_IDENTIFIER_NAME), std::ofstream::out);
                ofs << m_identifier_num;
                ofs.close();
                ftruncate(m_writer_shm_fd, m_shmem_data_bins);
                m_data = (T *)mmap(0, m_shmem_data_bins, PROT_READ | PROT_WRITE, MAP_SHARED, m_writer_shm_fd, 0);
            }
        }
        else 
        {
            std::ofstream ofs(std::string(SHMEM_IDENTIFIER_PATH) + std::string(m_identifier_prefix) + std::string(SHMEM_IDENTIFIER_NAME), std::ofstream::out);
            ofs << m_identifier_num;
            ofs.close();
            ftruncate(m_writer_shm_fd, m_shmem_data_bins);
            m_data = (T *)mmap(0, m_shmem_data_bins, PROT_READ | PROT_WRITE, MAP_SHARED, m_writer_shm_fd, 0);
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
            if ((m_writer_sem = sem_open(m_sem_writer_name.c_str(), O_CREAT, 0660, 0)) == SEM_FAILED)
            {
                std::cerr << "Couldn't create semaphore " + m_sem_writer_name << std::endl;
                return 0;
            }
            if ((m_reader_sem = sem_open(m_sem_reader_name.c_str(), O_CREAT, 0660, 0)) == SEM_FAILED)
            {
                std::cerr << "Couldn't create semaphore " + m_sem_reader_name << std::endl;
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
    std::cout << "Opening shmem fd: " << m_shmem_name << std::endl;
    m_reader_shm_fd = shm_open(m_shmem_name.c_str(), O_RDONLY, 0666);
    std::cout << "FD is: " << m_reader_shm_fd << std::endl;
    if (m_reader_shm_fd < 0)
    {
        std::cerr << "Couldn't open shmem fd." << std::endl;
        return 0;
    }
    m_data = (T *)mmap(0, m_shmem_data_bins, PROT_READ, MAP_SHARED, m_reader_shm_fd, 0);

    return 1;
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
    }
    if (sem_post(m_writer_sem) == -1)
    {
        std::cerr << "Couldn't post writer semaphore." << std::endl;
    }
    for (int i = 0; i < m_shmem_data_bins; ++i)
    {
        data[i] = m_data[i];
    }
    if (sem_wait(m_writer_sem) == -1)
    {
        std::cerr << "Couldn't wait writer semaphore." << std::endl;
    }
    if (sem_post(m_reader_sem) == -1)
    {
        std::cerr << "Couldn't post reader semaphore." << std::endl;
    }
    return 1;
}

#endif //SHMEMHANDLER_H