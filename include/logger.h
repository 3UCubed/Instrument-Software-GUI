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

#define ERPA_HEADER "date, time, sync, seq, SWPMON, temp1, adc"
#define PMT_HEADER "date, time, sync, seq, adc"
#define HK_HEADER "date, time, sync, seq, vsense, vrefint, temp1, temp2, temp3, temp4, busvmon, busimon, 2v5mon, 3v3mon, 5vmon, n3v3mon, n5vmon, 15vmon, 5vrefmon, n200vmon, n800vmon"
#define CONTROLS_HEADER "date, time, PMT, ERPA, HK, SDN1, SYS_ON, 3v3, 5v, n3v3, n5v, 15v, n150v, 800v"

#define PMT_PACKET_SIZE 14
#define ERPA_PACKET_SIZE 18
#define HK_PACKET_SIZE 46

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
    void copyToRawLog(char *bytes, int size);
    void closeRawLog();
    void parseRawLog();

private:
    enum Packet_t
    {
        ERPA,
        PMT,
        HK,
        UNDEFINED
    };

    struct ERPA_PKT
    {
        std::string sync;
        std::string seq;
        std::string swp;
        std::string temp1;
        std::string adc;
        std::string year;
        std::string month;
        std::string day;
        std::string hour;
        std::string minute;
        std::string second;
        std::string millisecond;
    };

    struct PMT_PKT
    {
        std::string sync;
        std::string seq;
        std::string adc;
        std::string year;
        std::string month;
        std::string day;
        std::string hour;
        std::string minute;
        std::string second;
        std::string millisecond;
    };

    struct HK_PKT
    {
        std::string sync;
        std::string seq;
        std::string vsense;
        std::string vrefint;
        std::string temp1;
        std::string temp2;
        std::string temp3;
        std::string temp4;
        std::string busvmon;
        std::string busimon;
        std::string mon2v5;
        std::string mon3v3;
        std::string mon5v;
        std::string monn3v3;
        std::string monn5v;
        std::string mon15v;
        std::string mon5vref;
        std::string monn200v;
        std::string monn800v;
        std::string year;
        std::string month;
        std::string day;
        std::string hour;
        std::string minute;
        std::string second;
        std::string millisecond;
    };

    std::string createLogTitle(std::string dir);
    std::string currentLogTitle = "";
    std::ofstream rawDataStream;
    std::ofstream erpaStream;
    std::ofstream pmtStream;
    std::ofstream hkStream;
    std::ofstream controlsStream;
    void createPacketLogs();
    int slurp(std::string path, char *&buffer);
    Packet_t determinePacketType(char MSB, char LSB);
    double intToVoltage(int value, int resolution, int ref, float mult);
    double tempsToCelsius(int val);
    void closePacketLogs();

};

#endif