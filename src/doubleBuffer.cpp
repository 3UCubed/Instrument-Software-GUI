/**
 * @file doubleBuffer.cpp
 * @author Jared Morrison
 * @version 1.0.0-beta
 * @section DESCRIPTION
 *
 * Double buffer used for storing read data from serial port and displaying packet data to GUI
 */

#include "../include/doubleBuffer.h"
#include <iostream>
using namespace std;

/**
 * @brief Constructs a DoubleBuffer object with initialized storage buffers.
 * 
 * Initializes two storage buffers (storage_A and storage_B) and sets their indices to 0.
 * Sets the write pointer to point to storage_A and the read pointer to point to storage_B.
 */
DoubleBuffer::DoubleBuffer()
    : storage_A(), storage_B()
{
    storage_A.index = 0;
    storage_B.index = 0;
    write_storage_ptr = &storage_A;
    read_storage_ptr = &storage_B;
}

/**
 * @brief Copies data from a buffer to the active storage.
 * 
 * Copies data from the provided buffer to the active storage buffer (write_storage_ptr).
 * If the buffer or size is invalid, an error message is printed and no action is taken.
 * After copying, checks if the active storage buffer needs to switch with the read storage buffer.
 * 
 * @param buffer Pointer to the buffer containing data to copy.
 * @param size Size of the data to copy.
 */
void DoubleBuffer::copyToStorage(char *buffer, int size)
{
    if (buffer == nullptr || size <= 0)
    {
        std::cerr << "Invalid buffer or size\n";
        return;
    }
    write_mutex.lock();
    for (int i = 0; i < size; i++)
    {
        write_storage_ptr->fifo.push(buffer[i]);
        write_storage_ptr->index++;
    }
    write_mutex.unlock();
    if (read_storage_ptr->index == 0 && write_storage_ptr->index >= MIN_STORAGE_SIZE)
    {
        switchStorage();
    }
}

/**
 * @brief Switches the active and read storage buffers.
 * 
 * Switches the pointers of the write and read storage buffers atomically 
 * to enable seamless data reading and writing.
 * Uses mutexes to ensure thread safety during the switching process.
 */
void DoubleBuffer::switchStorage()
{
    read_mutex.lock();
    write_mutex.lock();
    Storage *temp = write_storage_ptr;
    write_storage_ptr = read_storage_ptr;
    read_storage_ptr = temp;
    read_mutex.unlock();
    write_mutex.unlock();
}

/**
 * @brief Retrieves the next set of bytes from the read storage.
 * 
 * Retrieves bytes from the read storage buffer (read_storage_ptr) and copies them 
 * into the provided buffer. Clears the read storage buffer after retrieval.
 * 
 * @param bytes Pointer to the buffer where bytes will be copied.
 * @return Number of bytes retrieved and copied.
 *         Returns 0 if the bytes pointer is invalid or if no bytes are available.
 */
int DoubleBuffer::getNextBytes(char *bytes)
{
    if (bytes == nullptr)
    {
        std::cerr << "Invalid bytes pointer\n";
        return 0;
    }
    read_mutex.lock();
    int i = 0;
    while (read_storage_ptr->index > 0)
    {
        bytes[i++] = read_storage_ptr->fifo.front();
        read_storage_ptr->fifo.pop();
        read_storage_ptr->index--;
    }
    read_mutex.unlock();
    return i;
}
