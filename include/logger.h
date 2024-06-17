/**
 * @file logger.h
 * @author Jared Morrison
 * @version 1.0.0-beta
 * @section DESCRIPTION
 *
 * Logs packet and control data when recording is turned on
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <iomanip>
#include <string>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <sstream>
#include <iostream>
class Logger
{
public:
    Logger();
    ~Logger();
    bool createRawLog();
    void copyToLog(char *bytes, int size);
    void closeLog();
private:
    std::string newLogTitle();
    std::ofstream rawDataStream;
};

#endif