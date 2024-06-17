#include "../include/doubleBuffer.h"
#include <iostream>
using namespace std;
DoubleBuffer::DoubleBuffer()
    : storage_A(), storage_B()
{
    storage_A.index = 0;
    storage_B.index = 0;
    write_storage_ptr = &storage_A;
    read_storage_ptr = &storage_B;
}

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
