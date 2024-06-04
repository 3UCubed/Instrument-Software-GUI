/**
 * @file logger.cpp
 * @author Jared Morrison
 * @version 1.0.0-beta
 * @section DESCRIPTION
 *
 * Logs packet and control data when recording is turned on
 */

#include <iomanip>
#include <string>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <sstream>
#include <iostream>

using namespace std;

#define ERPA_HEADER "date, time, sync, seq, SWPMON, temp1, adc"
#define PMT_HEADER "date, time, sync, seq, adc"
#define HK_HEADER "date, time, sync, seq, vsense, vrefint, temp1, temp2, temp3, temp4, busvmon, busimon, 2v5mov, 3v3mon, 5vmon, n3v3mon, n5vmon, 15vmon, 5refmon, n200vmon, n800vmon"
#define CONTROLS_HEADER "date, time, c_pmtOn, c_erpaOn, c_hkOn, c_sysOn, c_3v3, c_5v, c_n3v3, c_n5v, c_15v, c_n150v, c_800v, c_sdn1"

ofstream erpaStream;
ofstream pmtStream;
ofstream hkStream;
ofstream controlsStream;

string createLogName(string logType);

/**
 * @brief Enumeration for log types.
 */
enum Log
{
    ERPA,
    PMT,
    HK,
    CONTROLS
};

/**
 * @brief Structure for enabled controls.
 */
struct ControlsEnabled
{
    bool c_pmtOn = false;
    bool c_erpaOn = false;
    bool c_hkOn = false;
    bool c_sysOn = false;
    bool c_3v3 = false;
    bool c_5v = false;
    bool c_n3v3 = false;
    bool c_n5v = false;
    bool c_15v = false;
    bool c_n150v = false;
    bool c_800v = false;
    bool c_sdn1 = false;
} Controls;

/**
 * @brief Create new log files.
 *
 * Creates new log files for ERPA, PMT, HK, and Controls data.
 */
void createNewLogs()
{
    string newLogName;

    newLogName = createLogName("ERPA");
    erpaStream.open(newLogName, ios::app);
    erpaStream << ERPA_HEADER << "\n";

    newLogName = createLogName("PMT");
    pmtStream.open(newLogName, ios::app);
    pmtStream << PMT_HEADER << "\n";

    newLogName = createLogName("HK");
    hkStream.open(newLogName, ios::app);
    hkStream << HK_HEADER << "\n";

    newLogName = createLogName("CONTROLS");
    controlsStream.open(newLogName, ios::app);
    controlsStream << CONTROLS_HEADER << "\n";
}

/**
 * @brief Create a log file name.
 *
 * Generates a file name for a log based on the log type and current date and time.
 *
 * @param logType The type of log (e.g., "ERPA", "PMT", "HK", "CONTROLS").
 * @return A string containing the generated log file name.
 */
string createLogName(string logType)
{
    string newLogName = "";
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    ostringstream oss;
    oss << put_time(&tm, "%Y-%m-%d %H-%M-%S");
    string dateTime = oss.str();
    newLogName = "logs/" + logType + "/" + logType + " " + dateTime + ".csv";
    return newLogName;
}

/**
 * @brief Write control state to log.
 *
 * Writes the state of control variables to the controls log file along with timestamp information.
 *
 * @param controls The structure containing the control states.
 */
void writeToLog(ControlsEnabled &controls)
{
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);

    auto now = chrono::system_clock::now();
    auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
    controlsStream << put_time(&tm, "%m-%d-%Y, %H:%M:%S:") << ms.count();

    controlsStream << ", " << controls.c_pmtOn;
    controlsStream << ", " << controls.c_erpaOn;
    controlsStream << ", " << controls.c_hkOn;
    controlsStream << ", " << controls.c_sysOn;
    controlsStream << ", " << controls.c_3v3;
    controlsStream << ", " << controls.c_5v;
    controlsStream << ", " << controls.c_n3v3;
    controlsStream << ", " << controls.c_n5v;
    controlsStream << ", " << controls.c_15v;
    controlsStream << ", " << controls.c_n150v;
    controlsStream << ", " << controls.c_800v;
    controlsStream << ", " << controls.c_sdn1;
    controlsStream << "\n";
}

/**
 * @brief Write data to specified log file.
 *
 * Writes the given data to the specified log file according to the type of log.
 *
 * @param log The type of log (ERPA, PMT, HK, CONTROLS).
 * @param data An array containing the data to be written to the log file.
 */
void writeToLog(Log log, string data[])
{
    switch (log)
    {
    case ERPA:
    {
        string date = data[5].substr(0, 8);
        string time = data[5].substr(8, 14);
        erpaStream << date << ", " << time;
        for (int i = 0; i < 5; i++)
        {
            erpaStream << ", " << data[i];
        }
        erpaStream << "\n";
        break;
    }
    case PMT:
    {
        string date = data[3].substr(0, 8);
        string time = data[3].substr(8, 14);
        pmtStream << date << ", " << time;

        for (int i = 0; i < 3; i++)
        {
            pmtStream << ", " << data[i];
        }
        pmtStream << "\n";
        break;
    }
    case HK:
    {
        string date = data[19].substr(0, 8);
        string time = data[19].substr(8, 14);
        hkStream << date << ", " << time;

        for (int i = 0; i < 19; i++)
        {
            hkStream << ", " << data[i];
        }
        hkStream << "\n";
        break;
    }
    default:
    {
        break;
    }
    }
}

/**
 * @brief Close all log files.
 *
 * Closes the ERPA, PMT, HK, and CONTROLS log files.
 */
void closeLogs()
{
    erpaStream.close();
    pmtStream.close();
    hkStream.close();
    controlsStream.close();
}