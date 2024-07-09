/**
 * @file doubleBuffer.h
 * @author Jared Morrison
 * @version 2.0.0-alpha
 * @section DESCRIPTION
 *
 * Double buffer used for storing read data from serial port and displaying packet data to GUI
 */

#ifndef DOUBLE_BUFFER_H
#define DOUBLE_BUFFER_H

#include <queue>
#include <mutex>
class DoubleBuffer
{
public:
    DoubleBuffer();
    void copyToStorage(char *buffer, int size);
    int getNextBytes(char *bytes);
    volatile int MIN_STORAGE_SIZE = 128;
private:
    struct Storage
    {
        std::queue<char> fifo;
        int index;
    };

    Storage *write_storage_ptr;
    Storage *read_storage_ptr;
    Storage storage_A;
    Storage storage_B;
    std::mutex write_mutex;
    std::mutex read_mutex;
    void switchStorage();
};

#endif
