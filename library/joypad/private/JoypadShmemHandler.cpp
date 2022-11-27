#include "JoypadHandler.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <signal.h>
#include <filesystem>
#include <fstream>

JoypadShmemHandler::JoypadShmemHandler()
{
    createJoypadShmem();
}

JoypadShmemHandler::~JoypadShmemHandler()
{
    std::cout << "Closing shmem." << std::endl;
    munmap(m_data, JoypadHandler::CONTROL_DATA_BINS);
    close(m_fd_shm);
}

void JoypadShmemHandler::createJoypadShmem()
{
    const std::string pid = std::to_string(getpid());
    const std::string shmem_fd_name = std::string(SHMEM_NAME) + pid;
    const std::string writer_sem_name = std::string(WRITER_SEM_NAME) + pid;
    const std::string reader_sem_name = std::string(READER_SEM_NAME) + pid;
    m_fd_shm = shm_open(shmem_fd_name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
    if (m_fd_shm < 0)
    {
        std::cerr << "Couldn't open " + std::string(shmem_fd_name) << " errno: " << m_fd_shm << std::endl;
        m_shmem_created = false;
    } else {
        if (!std::filesystem::exists(SHMEM_IDENTIFIER_PATH))
        {
            if (!std::filesystem::create_directories(SHMEM_IDENTIFIER_PATH))
            {
                std::cerr << "Couldn't create directory " + std::string(SHMEM_IDENTIFIER_PATH) << std::endl;
            }
            else 
            {
                std::ofstream ofs(std::string(SHMEM_IDENTIFIER_PATH) + std::string(SHMEM_IDENTIFIER_NAME), std::ofstream::out);
                ofs << pid;
                ofs.close();
                m_shmem_created = true;
                ftruncate(m_fd_shm, JoypadHandler::CONTROL_DATA_BINS);
                m_data = (std::uint8_t *)mmap(0, JoypadHandler::CONTROL_DATA_BINS, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd_shm, 0);
            }
        }
        else 
        {
            std::ofstream ofs(std::string(SHMEM_IDENTIFIER_PATH) + std::string(SHMEM_IDENTIFIER_NAME), std::ofstream::out);
            ofs << pid;
            ofs.close();
            m_shmem_created = true;
            ftruncate(m_fd_shm, JoypadHandler::CONTROL_DATA_BINS);
            m_data = (std::uint8_t *)mmap(0, JoypadHandler::CONTROL_DATA_BINS, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd_shm, 0);
        }

        if ((writer_sem = sem_open(writer_sem_name.c_str(), O_CREAT, 0660, 0)) == SEM_FAILED)
        {
            std::cerr << "Couldn't create semaphore " + writer_sem_name << std::endl;
        }
        if ((reader_sem = sem_open(reader_sem_name.c_str(), O_CREAT, 0660, 0)) == SEM_FAILED)
        {
            std::cerr << "Couldn't create semaphore " + reader_sem_name << std::endl;
        }
        if (sem_post(writer_sem) == -1)
        {
            std::cerr << "Couldn't post writer semaphore." << std::endl;
        }
        if (sem_post(reader_sem) == -1)
        {
            std::cerr << "Couldn't post reader semaphore." << std::endl;
        }
    }
}

void JoypadShmemHandler::writeJoypadData(JoypadData data)
{
    if (m_shmem_created)
    {
        if (sem_wait(writer_sem) == -1)
        {
            std::cerr << "Couldn't wait writer semaphore." << std::endl;
        }
        if (sem_post(reader_sem) == -1)
        {
            std::cerr << "Couldn't post reader semaphore." << std::endl;
        }
        for (int i = 0; i < JoypadHandler::CONTROL_DATA_BINS; ++i)
        {
            m_data[i] = data.data[i];
        }
        if (sem_wait(reader_sem) == -1)
        {
            std::cerr << "Couldn't wait reader semaphore." << std::endl;
        }
        if (sem_post(writer_sem) == -1)
        {
            std::cerr << "Couldn't post writer semaphore." << std::endl;
        }
    }
}