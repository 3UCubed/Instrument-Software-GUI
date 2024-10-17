/**
 * @file logger.cpp
 * @author Jared Morrison
 * @version 2.0.0-alpha
 * @section DESCRIPTION
 *
 * Logs packet and control data when recording is turned on
 */

#include "../include/logger.h"
Logger::Logger() {};
Logger::~Logger() {};

// ******************************************************************************************************************* PUBLIC FUNCTIONS
/**
 * @brief Creates a raw log file with the specified ID.
 *
 * Creates a raw log file with the given ID by appending ".bin" to the log title.
 * Opens the file in append mode. Updates the current log title upon successful creation.
 *
 * @param id The identifier to include in the log file name.
 * @return True if the raw log file was successfully created and opened; false otherwise.
 */
bool Logger::createRawLog(std::string id)
{
    std::string fileName = createLogTitle("RAW", id);
    fileName += ".bin";
    rawDataStream.open(fileName, std::ios::app);

    if (!rawDataStream)
    {
        return false;
    }
    currentLogTitle = fileName;
    return true;
}

/**
 * @brief Copies bytes to the raw log stream.
 *
 * Copies the specified number of bytes from the provided buffer to the raw log stream.
 *
 * @param bytes Pointer to the buffer containing the bytes to copy.
 * @param size Number of bytes to copy.
 */
void Logger::copyToRawLog(char *bytes, int size)
{
    for (int i = 0; i < size; i++)
    {
        rawDataStream << *(bytes + i);
    }
}

/**
 * @brief Closes the raw log file stream.
 *
 * Closes the currently opened raw log file stream.
 */
void Logger::closeRawLog()
{
    rawDataStream.close();
}

/**
 * @brief Parses the raw log file and extracts packet data.
 *
 * Reads the raw log file into a buffer and extracts data packets of different types (ERPA, PMT, HK).
 * Converts the extracted data into formatted strings and writes them to respective CSV files.
 *
 * @param id Identifier used in creating the CSV file names.
 */
void Logger::parseRawLog(std::string id)
{
    ERPA_PKT erpa;
    PMT_PKT pmt;
    HK_PKT hk;
    int bytesRead = 0;
    int i = 0;
    char *buffer = nullptr;
    Packet_t packetType;
    bytesRead = slurp(currentLogTitle, buffer); // Read entire file into char buffer
    createPacketLogs(id);                       // Create separate .csv for each packet type

    while (i < bytesRead)
    {
        packetType = determinePacketType(buffer[i], buffer[i + 1]);
        if (packetType == ERPA && i < bytesRead - ERPA_PACKET_SIZE)
        {
            char res[50];
            uint32_t value;
            value = ((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF);
            i += 2;
            snprintf(res, 50, "0x%X", value);
            erpa.sync = res;

            value = ((buffer[i] & 0xFF) << 24) | ((buffer[i + 1] & 0xFF) << 16) | ((buffer[i + 2] & 0xFF) << 8) | (buffer[i + 3] & 0xFF);
            i += 4;
            snprintf(res, 50, "%06d", value);
            erpa.uptime = res;

            value = ((buffer[i] & 0xFF) << 16) | ((buffer[i + 1] & 0xFF) << 8) | (buffer[i + 2] & 0xFF);
            i += 3;
            snprintf(res, 50, "%04d", value);
            erpa.seq = res;

            value = buffer[i];
            snprintf(res, 50, "%04d", value);
            erpa.step = res;
            i++;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
            erpa.swp = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%08.7f", intToVoltage(value, 16, 5, 1.0));
            erpa.adc = res;


            std::string formattedData = "";
            formattedData = erpa.sync + ", " + erpa.uptime + ", " + erpa.seq + ", " + erpa.step + ", " + erpa.swp + ", " + erpa.adc + "\n"; // Packet data
            erpaStream << formattedData;
        }
        else if (packetType == PMT && i < bytesRead - PMT_PACKET_SIZE)
        {
            char res[50];
            uint32_t value;

            value = ((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF);
            i += 2;
            snprintf(res, 50, "0x%X", value);
            pmt.sync = res;

            value = ((buffer[i] & 0xFF) << 24) | ((buffer[i + 1] & 0xFF) << 16) | ((buffer[i + 2] & 0xFF) << 8) | (buffer[i + 3] & 0xFF);
            i += 4;
            snprintf(res, 50, "%06d", value); // millisecond
            pmt.uptime = res;

            value = ((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF);
            i += 2;
            snprintf(res, 50, "%04d", value);
            pmt.seq = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%08.7f", intToVoltage(value, 16, 5, 1.0));
            pmt.adc = res;

            std::string formattedData = "";
            formattedData = pmt.sync + ", " + pmt.uptime + ", " + pmt.seq + ", " + pmt.adc + "\n";
            pmtStream << formattedData;
        }
        else if (packetType == HK && i < bytesRead - HK_PACKET_SIZE)
        {
            char res[50];
            uint32_t value;
            value = ((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF);
            i += 2;
            snprintf(res, 50, "0x%X", value);
            hk.sync = res;

            value = 
            ((buffer[i] & 0xFF) << 24) |
            ((buffer[i+1] & 0xFF) << 16) |
            ((buffer[i+2] & 0xFF) << 8)  |
            ((buffer[i+3] & 0xFF));

            uint16_t ms;
            ms = ((buffer[i+4] & 0xFF) << 8)  |
                 ((buffer[i+5] & 0xFF));

            uint64_t unix_ms;
            unix_ms = static_cast<uint64_t>(value) * 1000 + ms;
            
            i += 6;
            snprintf(res, 50, "%10d", value); 
            hk.unix = res;

            value = ((buffer[i] & 0xFF) << 24) | ((buffer[i + 1] & 0xFF) << 16) | ((buffer[i + 2] & 0xFF) << 8) | (buffer[i + 3] & 0xFF);
            i += 4;
            snprintf(res, 50, "%06d", value); // millisecond
            hk.uptime = res;

            value = ((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF);
            i += 2;
            snprintf(res, 50, "%04d", value);
            hk.seq = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", convert_VSENSE(value));
            hk.vsense = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
            hk.vrefint = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
            hk.busvmon = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
            hk.busimon = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
            hk.mon2v5 = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
            hk.mon3v3 = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
            hk.mon5v = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
            hk.monn3v3 = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
            hk.monn5v = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
            hk.mon15v = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
            hk.mon5vref = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
            hk.monn200v = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
            hk.monn800v = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", convert_ADT7410(value));
            hk.temp1 = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", convert_ADT7410(value));
            hk.temp2 = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", convert_ADT7410(value));
            hk.temp3 = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", convert_ADT7410(value));
            hk.temp4 = res;

            value = (((buffer[i] & 0xFF) << 8) | (buffer[i + 1] & 0xFF));
            i += 2;
            snprintf(res, 50, "%06.5f", convert_ADHV47021(value));
            hk.tmp1 = res;

            std::string formattedData = "";
            formattedData += hk.sync + ", " + hk.unix + ", " + hk.uptime + ", ";
            formattedData += hk.seq + ", " + hk.vsense + ", " + hk.vrefint + ", " + hk.busvmon + ", " + hk.busimon + ", ";
            formattedData += hk.mon2v5 + ", " + hk.mon3v3 + ", " + hk.mon5v + ", " + hk.monn3v3 + ", " + hk.monn5v + ", "; 
            formattedData += hk.mon15v + ", " + hk.mon5vref + ", " + hk.monn200v + ", " + hk.monn800v + ", "; 
            formattedData += hk.temp1 + ", " + hk.temp2 + ", " + hk.temp3 + ", " + hk.temp4 + ", " + hk.tmp1 + "\n";
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
/**
 * @brief Creates a log file title based on directory, identifier, and current timestamp.
 *
 * Constructs a log file name using the specified directory, identifier, and current timestamp.
 *
 * @param dir Directory name where the log file will be stored.
 * @param id Identifier to include in the log file name.
 * @return The constructed log file title as a string.
 */
std::string Logger::createLogTitle(std::string dir, std::string id)
{
    std::string newLogName = "";
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H-%M-%S");
    std::string dateTime = oss.str();
    newLogName = "logs/" + dir + "/" + dateTime + "-" + dir;
    return newLogName;
}

/**
 * @brief Creates CSV log files for different packet types.
 *
 * Creates CSV log files with headers for ERPA, PMT, HK, and controls packets
 * based on the provided identifier.
 *
 * @param id Identifier to include in the log file names.
 */
void Logger::createPacketLogs(std::string id)
{
    std::string newLogName;

    newLogName = createLogTitle("ERPA", id);
    newLogName += ".csv";
    erpaStream.open(newLogName, std::ios::app);
    erpaStream << ERPA_HEADER << "\n";

    newLogName = createLogTitle("PMT", id);
    newLogName += ".csv";
    pmtStream.open(newLogName, std::ios::app);
    pmtStream << PMT_HEADER << "\n";

    newLogName = createLogTitle("HK", id);
    newLogName += ".csv";
    hkStream.open(newLogName, std::ios::app);
    hkStream << HK_HEADER << "\n";
}

/**
 * @brief Reads the entire contents of a file into a buffer.
 *
 * Opens the specified file in binary mode and reads its entire content into a dynamically allocated buffer.
 * Returns the size of the file read and assigns the buffer pointer to the read content.
 *
 * @param path Path to the file to be read.
 * @param buffer Reference to a pointer where the read buffer will be stored.
 *               The buffer is allocated inside the function and must be freed by the caller.
 * @return Size of the file read into the buffer, or 0 if the file could not be opened or read.
 */
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

/**
 * @brief Determines the type of packet based on the MSB and LSB values.
 *
 * Determines the type of packet based on the most significant byte (MSB) and least significant byte (LSB) values.
 *
 * @param MSB Most significant byte of the packet header.
 * @param LSB Least significant byte of the packet header.
 * @return The type of packet detected (ERPA, PMT, HK) or UNDEFINED if the packet type is unknown.
 */
Logger::Packet_t Logger::determinePacketType(char MSB, char LSB)
{
    if (((MSB & 0xFF) == 0xEE) && ((LSB & 0xFF) == 0xEE))
    {
        return ERPA;
    }

    if (((MSB & 0xFF) == 0xFF) && ((LSB & 0xFF) == 0xFF))
    {
        return PMT;
    }

    if (((MSB & 0xFF) == 0xDD) && ((LSB & 0xFF) == 0xDD))
    {
        return HK;
    }

    return UNDEFINED;
}

/**
 * @brief Converts an integer value to voltage based on resolution, reference voltage, and multiplier.
 *
 * Converts an integer value representing ADC reading to voltage, considering the resolution, reference voltage,
 * and multiplier factors.
 *
 * @param value Integer value to convert to voltage.
 * @param resolution Resolution of the ADC (12 or 16 bits).
 * @param ref Reference voltage used by the ADC.
 * @param mult Multiplier factor applied to the calculated voltage.
 * @return Calculated voltage corresponding to the input value.
 */
double Logger::intToVoltage(int value, int resolution, double ref, float mult)
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

float Logger::convert_ADT7410(int16_t raw) {
    float ret = raw;
    if (raw >= 0x1000) {
        ret -= 8192;
    }

    return ret / 16.0;
}

float Logger::convert_VSENSE(int16_t raw) {
    float TS_CAL1_TEMP = 30;
    float TS_CAL1 = 0.62;

    float TS_CAL2_TEMP = 130;
    float TS_CAL2 = 0.82;

    float voltage = (raw * 3.3) / 4095;
    float temperature = ((TS_CAL2_TEMP - TS_CAL1_TEMP) / (TS_CAL2 - TS_CAL1)) * (voltage - TS_CAL1) + TS_CAL1_TEMP;

    return temperature;
}

float Logger::convert_ADHV47021(int16_t raw) {
    float voltage = (raw * 3.3) / 4095;

    float temperature = 25.0f + (voltage - 1.9f) / -0.0045f;

    return temperature;
}
/**
 * @brief Closes all open packet log streams.
 *
 * Closes the streams for ERPA, PMT, HK, and controls packet logs if they are open.
 */
void Logger::closePacketLogs()
{
    erpaStream.close();
    pmtStream.close();
    hkStream.close();
    controlsStream.close();
}
