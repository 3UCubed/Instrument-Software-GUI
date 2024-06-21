/**
 * @file logger.cpp
 * @author Jared Morrison
 * @version 1.0.0-beta
 * @section DESCRIPTION
 *
 * Logs packet and control data when recording is turned on
 */

#include "../include/logger.h"

Logger::Logger(){};
Logger::~Logger(){};



// ******************************************************************************************************************* PUBLIC FUNCTIONS
bool Logger::createRawLog()
{
    std::string fileName = createLogTitle("RAW");
    rawDataStream.open(fileName, std::ios::app);

    if (!rawDataStream)
    {
        return false;
    }
    currentLogTitle = fileName;
    return true;
}

void Logger::copyToRawLog(char *bytes, int size)
{
    for (int i = 0; i < size; i++)
    {
        rawDataStream << bytes[i];
    }
}

void Logger::closeRawLog()
{
    rawDataStream.close();
}

void Logger::parseRawLog()
{
    ERPA_PKT erpa;
    PMT_PKT pmt;
    HK_PKT hk;
    int bytesRead = 0;
    int i = 0;
    char *buffer = nullptr;
    Packet_t packetType;
    bytesRead = slurp(currentLogTitle, buffer); // Read entire file into char buffer
    createPacketLogs();                  // Create separate .csv for each packet type

    while (i < bytesRead)
    {
        packetType = determinePacketType(buffer[i], buffer[i + 1]);
        if (packetType == ERPA && i < bytesRead - ERPA_PACKET_SIZE)
        {
            char res[50];
            int value;
            value = ((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF);
            i += 2;
            snprintf(res, 50, "0x%X", value);
            erpa.sync = res;

            value = ((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF);
            i += 2;
            snprintf(res, 50, "%04d", value);
            erpa.seq = res;

            value = intToVoltage(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF), 12, 3.3, 1.0);
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            erpa.swp = res;

            value = intToVoltage(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF), 12, 3.3, 1.0);
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            erpa.temp1 = res;

            value = intToVoltage(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF), 16, 5, 1.0);
            i += 2;
            snprintf(res, 50, "%08.7f", value);
            erpa.adc = res;

            snprintf(res, 50, "%02d", buffer[i++]); // year
            erpa.year = res;
            snprintf(res, 50, "%02d", buffer[i++]); // month
            erpa.month = res;
            snprintf(res, 50, "%02d", buffer[i++]); // day
            erpa.day = res;
            snprintf(res, 50, "%02d", buffer[i++]); // hour
            erpa.hour = res;
            snprintf(res, 50, "%02d", buffer[i++]); // minute
            erpa.minute = res;
            snprintf(res, 50, "%02d", buffer[i++]); // second
            erpa.second = res;
            value = ((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF);
            i += 2;
            snprintf(res, 50, "%003d", value); // millisecond
            erpa.millisecond = res;

            std::string formattedData = "";
            formattedData += erpa.year + "-" + erpa.month + "-" + erpa.day + ", ";                                       // Date
            formattedData += "T" + erpa.hour + ":" + erpa.minute + ":" + erpa.second + "." + erpa.millisecond + "Z, ";   // Time
            formattedData += erpa.sync + ", " + erpa.seq + ", " + erpa.swp + ", " + erpa.temp1 + ", " + erpa.adc + "\n"; // Packet data
            erpaStream << formattedData;
        }
        else if (packetType == PMT && i < bytesRead - PMT_PACKET_SIZE)
        {
            char res[50];
            int value;

            value = ((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF);
            i += 2;
            snprintf(res, 50, "0x%X", value);
            pmt.sync = res;

            value = ((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF);
            i += 2;
            snprintf(res, 50, "%04d", value);
            pmt.seq = res;

            value = intToVoltage(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF), 16, 5, 1.0);
            i += 2;
            snprintf(res, 50, "%08.7f", value);
            pmt.adc = res;

            snprintf(res, 50, "%02d", buffer[i++]); // year
            pmt.year = res;
            snprintf(res, 50, "%02d", buffer[i++]); // month
            pmt.month = res;
            snprintf(res, 50, "%02d", buffer[i++]); // day
            pmt.day = res;
            snprintf(res, 50, "%02d", buffer[i++]); // hour
            pmt.hour = res;
            snprintf(res, 50, "%02d", buffer[i++]); // minute
            pmt.minute = res;
            snprintf(res, 50, "%02d", buffer[i++]); // second
            pmt.second = res;

            value = ((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF);
            i += 2;
            snprintf(res, 50, "%003d", value); // millisecond
            pmt.millisecond = res;
            std::string formattedData = "";
            formattedData += pmt.year + "-" + pmt.month + "-" + pmt.day + ", ";                                    // Date
            formattedData += "T" + pmt.hour + ":" + pmt.minute + ":" + pmt.second + "." + pmt.millisecond + "Z, "; // Time
            formattedData += pmt.sync + ", " + pmt.seq + ", " + pmt.adc + "\n";
            pmtStream << formattedData;
        }
        else if (packetType == HK && i < bytesRead - HK_PACKET_SIZE)
        {
            char res[50];
            int value;
            value = ((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF);
            i += 2;
            snprintf(res, 50, "0x%X", value);
            hk.sync = res;

            value = ((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF);
            i += 2;
            snprintf(res, 50, "%04d", value);
            hk.seq = res;

            value = intToVoltage(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF), 12, 3.3, 1.0);
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            hk.vsense = res;

            value = intToVoltage(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF), 12, 3.3, 1.0);
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            hk.vrefint = res;

            value = tempsToCelsius(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            hk.temp1 = res;

            value = tempsToCelsius(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            hk.temp2 = res;

            value = tempsToCelsius(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            hk.temp3 = res;

            value = tempsToCelsius(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            hk.temp4 = res;

            value = intToVoltage(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF), 12, 3.3, 1.0);
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            hk.busvmon = res;

            value = intToVoltage(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF), 12, 3.3, 1.0);
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            hk.busimon = res;

            value = intToVoltage(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF), 12, 3.3, 1.0);
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            hk.mon2v5 = res;

            value = intToVoltage(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF), 12, 3.3, 1.0);
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            hk.mon3v3 = res;

            value = intToVoltage(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF), 12, 3.3, 1.0);
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            hk.mon5v = res;

            value = intToVoltage(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF), 12, 3.3, 1.0);
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            hk.monn3v3 = res;

            value = intToVoltage(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF), 12, 3.3, 1.0);
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            hk.monn5v = res;

            value = intToVoltage(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF), 12, 3.3, 1.0);
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            hk.mon15v = res;

            value = intToVoltage(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF), 12, 3.3, 1.0);
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            hk.mon5vref = res;

            value = intToVoltage(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF), 12, 3.3, 1.0);
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            hk.monn200v = res;

            value = intToVoltage(((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF), 12, 3.3, 1.0);
            i += 2;
            snprintf(res, 50, "%06.5f", value);
            hk.monn800v = res;

            snprintf(res, 50, "%02d", buffer[i++]); // year
            hk.year = res;
            snprintf(res, 50, "%02d", buffer[i++]); // month
            hk.month = res;
            snprintf(res, 50, "%02d", buffer[i++]); // day
            hk.day = res;
            snprintf(res, 50, "%02d", buffer[i++]); // hour
            hk.hour = res;
            snprintf(res, 50, "%02d", buffer[i++]); // minute
            hk.minute = res;
            snprintf(res, 50, "%02d", buffer[i++]); // second
            hk.second = res;

            value = ((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF);
            i += 2;
            snprintf(res, 50, "%003d", value); // millisecond
            hk.millisecond = res;

            std::string formattedData = "";
            formattedData += hk.year + "-" + hk.month + "-" + hk.day + ", ";                                   // Date
            formattedData += "T" + hk.hour + ":" + hk.minute + ":" + hk.second + "." + hk.millisecond + "Z, "; // Time
            formattedData += hk.sync + ", " + hk.seq + ", " + hk.vsense + ", " + hk.vrefint + ", " + hk.temp1 + ", " + hk.temp2 + ", " + hk.temp3 + ", " + hk.temp4 + ", ";
            formattedData += hk.busvmon + ", " + hk.busimon + ", " + hk.mon2v5 + ", " + hk.mon3v3 + ", " + hk.mon5v + ", " + hk.monn3v3 + ", " + hk.monn5v + ", " + hk.mon15v + ", " + hk.mon5vref + ", " + hk.monn200v + ", " + hk.monn800v + "\n";
            hkStream << formattedData;
        }
        else
        {
            i++;
        }
    }
    closePacketLogs();
}

// ******************************************************************************************************************* PRIVATE FUNCTIONS
std::string Logger::createLogTitle(std::string dir)
{
    std::string newLogName = "";
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H-%M-%S");
    std::string dateTime = oss.str();
    newLogName = "logs/" + dir + "/" + dir + "_" + dateTime + ".csv";
    return newLogName;
}

void Logger::createPacketLogs()
{
    std::string newLogName;

    newLogName = createLogTitle("ERPA");
    erpaStream.open(newLogName, std::ios::app);
    erpaStream << ERPA_HEADER << "\n";

    newLogName = createLogTitle("PMT");
    pmtStream.open(newLogName, std::ios::app);
    pmtStream << PMT_HEADER << "\n";

    newLogName = createLogTitle("HK");
    hkStream.open(newLogName, std::ios::app);
    hkStream << HK_HEADER << "\n";

    newLogName = createLogTitle("CONTROLS");
    controlsStream.open(newLogName, std::ios::app);
    controlsStream << CONTROLS_HEADER << "\n";
}

int Logger::slurp(std::string path, char *&buffer)
{
    // Open the file in binary mode
    std::ifstream file(path, std::ios::binary);

    // Check if the file was opened successfully
    if (!file)
    {
        std::cerr << "Failed to open the file: " << path << std::endl;
        return 0;
    }

    // Seek to the end of the file to determine its size
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    buffer = new char[fileSize];

    // Read the entire file into the buffer
    if (file.read(buffer, fileSize))
    {
        // Successfully read the file
        std::cout << "File read successfully.\n";

        // Use the buffer (for demonstration purposes, we'll just print the size)
        // In a real application, you would process the buffer contents here
    }
    else
    {
        std::cerr << "Failed to read the file" << std::endl;
        return 0;
    }

    // Close the file
    file.close();
    return fileSize;
}

Logger::Packet_t Logger::determinePacketType(char MSB, char LSB)
{
    if (((MSB & 0xFF) == 0xAA) && ((LSB & 0xFF) == 0xAA))
    {
        return ERPA;
    }

    if (((MSB & 0xFF) == 0xBB) && ((LSB & 0xFF) == 0xBB))
    {
        return PMT;
    }

    if (((MSB & 0xFF) == 0xCC) && ((LSB & 0xFF) == 0xCC))
    {
        return HK;
    }

    return UNDEFINED;
}

double Logger::intToVoltage(int value, int resolution, int ref, float mult)
{
    double voltage;
    if (resolution == 12)
    {
        voltage = (double)(value * ref) / 4095 * mult;
    }

    if (resolution == 16)
    {
        voltage = (double)(value * ref) / 65535 * mult;
    }
    return voltage;
}

double Logger::tempsToCelsius(int val)
{
    char convertedChar[16];
    double convertedTemp;

    // Convert to 2's complement, since temperature can be negative
    if (val > 0x7FF)
    {
        val |= 0xF000;
    }

    // Convert to float temperature value (Celsius)
    float temp_c = val * 0.0625;

    // Convert temperature to decimal value
    temp_c *= 100;

    sprintf(convertedChar, "%u.%u",
            ((unsigned int)temp_c / 100),
            ((unsigned int)temp_c % 100));

    convertedTemp = std::stod(convertedChar);

    return convertedTemp;
}

void Logger::closePacketLogs(){
    erpaStream.close();
    pmtStream.close();
    hkStream.close();
    controlsStream.close();
}



