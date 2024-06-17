/**
 * @file logger.cpp
 * @author Jared Morrison
 * @version 1.0.0-beta
 * @section DESCRIPTION
 *
 * Logs packet and control data when recording is turned on
 */
#include"../include/logger.h"
Logger::Logger(){};
Logger::~Logger(){};
std::string Logger::newLogTitle()
{
    std::string newLogName = "";
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H-%M-%S");
    std::string dateTime = oss.str();
    newLogName = "logs/RawSave_" + dateTime + ".bin";
    return newLogName;
}

bool Logger::createRawLog(){
    std::string fileName = newLogTitle();
    rawDataStream.open(fileName, std::ios::app);

    if (!rawDataStream){
        return false;
    }

    return true;
}

void Logger::copyToLog(char *bytes, int size){
    for(int i = 0; i < size; i++){
        rawDataStream << bytes[i];
    }
}

void Logger::closeLog(){
    rawDataStream.close();
}