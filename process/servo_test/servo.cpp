#include "ServoController.h"

#include "JoypadData.h"
#include "JoypadHandler.h"
#include "JoypadShmemHandler.h"

#include <thread>
#include <chrono>
#include <iostream>
#include <fstream>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>

// TODO: write checker for reading pid from file

int main()
{
    JoypadData joypad_data;
    JoypadData joypad_data_previous;
    JoypadDataTypes joypad_data_types;
    ServoController sc;
    std::string joypad_manager_pid;
    std::string shmem_idenrifier = std::string(JoypadShmemHandler::SHMEM_IDENTIFIER_PATH) + std::string(JoypadShmemHandler::SHMEM_IDENTIFIER_NAME);
    
    sem_t * writer_sem;
    sem_t * reader_sem;

    int current_servo_l = 1;
    int current_servo_r = 0;


    std::ifstream ifs(shmem_idenrifier);
    if (!ifs.is_open()) {
        // std::cout << "failed to open " << shmem_idenrifier << '\n';
    } else {
        // TODO: write check for line
        getline(ifs, joypad_manager_pid);
        // std::cout << joypad_manager_pid << std::endl;
    }


    const std::string writer_sem_name = std::string(JoypadShmemHandler::WRITER_SEM_NAME) + joypad_manager_pid;
    const std::string reader_sem_name = std::string(JoypadShmemHandler::READER_SEM_NAME) + joypad_manager_pid;

    if ((writer_sem = sem_open(writer_sem_name.c_str(), 0, 0, 0)) == SEM_FAILED)
    {
        std::cerr << "Couldn't create semaphore " + writer_sem_name << std::endl;
    }
    if ((reader_sem = sem_open(reader_sem_name.c_str(), 0, 0, 0)) == SEM_FAILED)
    {
        std::cerr << "Couldn't create semaphore " + reader_sem_name << std::endl;
    }

    const std::string shmem_name = std::string(JoypadShmemHandler::SHMEM_NAME) + joypad_manager_pid;

    int fd = shm_open(shmem_name.c_str(), O_RDONLY, 0666);
    if (fd < 0)
    {
        std::cerr << "ERROR OPENING SHMEM!" << std::endl;
        return EXIT_FAILURE;
    }

    std::uint8_t * data = (std::uint8_t *)mmap(0, JoypadHandler::CONTROL_DATA_BINS, PROT_READ, MAP_SHARED, fd, 0);

    // int a = 0;
    while(true)
    {
        // std::cout << "Semaphore wait" << std::endl;
        if (sem_wait(reader_sem) == -1)
        {
            std::cerr << "Couldn't wait reader semaphore." << std::endl;
        }
        if (sem_post(writer_sem) == -1)
        {
            std::cerr << "Couldn't post writer semaphore." << std::endl;
        }
        
        for (int i = 0; i < JoypadHandler::CONTROL_DATA_BINS; ++i)
        {
            joypad_data.data[i] = data[i];
        }
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
            if (joypad_data_previous.data[i] != joypad_data.data[i])
            {
                joypad_data_types.parseJoypadData(joypad_data);
                // joypad_data_types.printJoypadData();

                if (joypad_data_types.leftTrigger)
                {
                    --current_servo_l;
                    if (current_servo_l < 1)
                    {
                        ++current_servo_l;
                    }
                }

                if (joypad_data_types.leftBumper)
                {
                    ++current_servo_l;
                    if (current_servo_l > 5)
                    {
                        --current_servo_l;
                    }
                }
                break;
            }
        }
        if (joypad_data_types.leftStickX < 125)
        {
            // std::cout << "joypad_data_types.leftStickX " << +joypad_data_types.leftStickX << std::endl;
            sc.moveLeft(current_servo_l);
        } else if(joypad_data_types.leftStickX > 129) {
            // std::cout << "joypad_data_types.leftStickX " << +joypad_data_types.leftStickX << std::endl;
            sc.moveRight(current_servo_l);
        }

        if (joypad_data_types.rightStickX < 125)
        {
            // std::cout << "joypad_data_types.rightStickX " << +joypad_data_types.rightStickX << std::endl;
            sc.moveLeft(0);
        } else if(joypad_data_types.rightStickX > 129) {
            // std::cout << "joypad_data_types.rightStickX " << +joypad_data_types.rightStickX << std::endl;
            sc.moveRight(0);
        }
        for (int i = 0; i < JoypadHandler::CONTROL_DATA_BINS; ++i)
        {
            joypad_data_previous.data[i] = joypad_data.data[i];
        }

        // std::cout << "Semaphore post" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        // // std::cout << "--- a: " << a << std::endl;
        // ++a;

    }
    munmap(data, JoypadHandler::CONTROL_DATA_BINS);
    close(fd);
    shm_unlink(JoypadShmemHandler::SHMEM_NAME);
    // while(true)
    // {
        // std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        // sc.setAbsolutePosition(MIDDLE_POS_SERVO_0, 0);
        // std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        // sc.setAbsolutePosition(MIDDLE_POS_SERVO_2, 2);
        // std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        // sc.setAbsolutePosition(MIDDLE_POS_SERVO_1, 1);
        // std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        // sc.setAbsolutePosition(MIDDLE_POS_SERVO_3, 3);
        // std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        // sc.setAbsolutePosition(MIDDLE_POS_SERVO_4, 4);
        // std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        // sc.setAbsolutePosition(MIDDLE_POS_SERVO_5, 5);
        // std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        // sc.setAbsolutePosition(STARTUP_POS_SERVO_0, 0);
        // std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        // sc.setAbsolutePosition(STARTUP_POS_SERVO_2, 2);
        // std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        // sc.setAbsolutePosition(STARTUP_POS_SERVO_1, 1);
        // std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        // sc.setAbsolutePosition(STARTUP_POS_SERVO_3, 3);
        // std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        // sc.setAbsolutePosition(STARTUP_POS_SERVO_4, 4);
        // std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        // sc.setAbsolutePosition(STARTUP_POS_SERVO_5, 5);
        // sc.setAbsolutePosition(MIN_POS_VAL, 0);
        // sc.setAbsolutePosition(MAX_POS_VAL, 0);
        // sc.setAbsolutePosition(MIDDLE_POS_SERVO_0, 0);
        // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        // sc.setAbsolutePosition(500, 3);
        // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        // sc.setAbsolutePosition(1500, 3);
    // }
    // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // sc.setAbsolutePosition(1500, 0);
    // sc.setAbsolutePosition(1500, 0);
    // // sc.setAbsolutePosition(2500, 1);
    // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // sc.setAbsolutePosition(1200, 0);
    // // sc.setAbsolutePosition(500, 1);
    // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // sc.setAbsolutePosition(900, 0);
    // // sc.setAbsolutePosition(900, 1);
    // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // sc.setAbsolutePosition(1200, 0);
    // // sc.setAbsolutePosition(2200, 1);
    // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // sc.setAbsolutePosition(1500, 0);

    return 0;
}