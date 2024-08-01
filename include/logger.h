/**
 * @file logger.h
 * @author Jared Morrison
 * @version 2.0.0-alpha
 * @section DESCRIPTION
 *
 * Logs packet and control data when recording is turned on
 */

#ifndef LOGGER_H
#define LOGGER_H

#define ERPA_HEADER "uptime, sync, seq, step, SWPMON, adc"
#define PMT_HEADER "uptime, sync, seq, adc"
#define HK_HEADER "date, time, uptime, sync, seq, vsense, vrefint, temp1, temp2, temp3, temp4, busvmon, busimon, 2v5mon, 3v3mon, 5vmon, n3v3mon, n5vmon, 15vmon, 5vrefmon, n200vmon, n800vmon, tmp1"
#define CONTROLS_HEADER "date, time, PMT, ERPA, HK, SDN1, SYS_ON, 3v3, 5v, n3v3, n5v, 15v, n150v, 800v"

#define PMT_PACKET_SIZE 10
#define ERPA_PACKET_SIZE 14
#define HK_PACKET_SIZE 54

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
    bool createRawLog(std::string id);
    void copyToRawLog(char *bytes, int size);
    void closeRawLog();
    void parseRawLog(std::string id);

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
        std::string step;
        std::string swp;
        std::string adc;
        std::string uptime;
    };

    struct PMT_PKT
    {
        std::string sync;
        std::string seq;
        std::string adc;
        std::string uptime;
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
        std::string tmp1;
        std::string year;
        std::string month;
        std::string day;
        std::string hour;
        std::string minute;
        std::string second;
        std::string microsecond;
        std::string uptime;
    };

    std::string createLogTitle(std::string dir, std::string id);
    std::string currentLogTitle = "";
    std::ofstream rawDataStream;
    std::ofstream erpaStream;
    std::ofstream pmtStream;
    std::ofstream hkStream;
    std::ofstream controlsStream;
    void createPacketLogs(std::string id);
    int slurp(std::string path, char *&buffer);
    Packet_t determinePacketType(char MSB, char LSB);
    double intToVoltage(int value, int resolution, double ref, float mult);
    double tempsToCelsius(int val);
    void closePacketLogs();
    float calculateTemperature(float tmpVoltage);

};

#endif