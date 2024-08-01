/**
 * @file instrumentGUI.cpp
 * @author Jared Morrison, Jared King, Shane Woods
 * @version 2.0.0-alpha
 * @section DESCRIPTION
 *
 * GUI that connects to H7-Instrument-Software and shows packet data in real time
 */

// #define GUI_LOG
#include "../include/instrumentGUI.h"

// ******************************************************************************************************************* HELPER FUNCTIONS
/**
 * @brief Finds the first serial port device matching the specified prefix in /dev.
 *
 * Searches the /dev directory for a serial port device that starts with the given prefix.
 * Returns the path of the first matching device found.
 *
 * @return const char* The path to the serial port device, or nullptr if not found.
 *         The caller is responsible for managing the memory of the returned string.
 */
const char *findSerialPort()
{
    const char *devPath = "/dev/";
    const char *prefix = "cu.usbserial-"; // Your prefix here
    DIR *dir = opendir(devPath);
    if (dir == nullptr)
    {
        std::cerr << "Error opening directory" << std::endl;
        return nullptr;
    }

    dirent *entry;
    char *portName = nullptr;

    while ((entry = readdir(dir)) != nullptr)
    {
        const char *filename = entry->d_name;
        if (strstr(filename, prefix) != nullptr)
        {
            size_t pathLength = strlen(devPath) + strlen(filename) + 1;
            portName = (char *)malloc(pathLength);
            if (portName != nullptr)
            {
                snprintf(portName, pathLength, "%s%s", devPath, filename);
                std::cout << "Using port: " << portName << std::endl;
            }
            break; // Use the first matching serial port found
        }
    }
    closedir(dir);
    return portName;
}

/**
 * @brief Writes a single byte of data to the specified serial port.
 *
 * This function sends the provided data byte to the given serial port and logs an error if the write operation fails.
 *
 * @param serialPort The file descriptor for the open serial port.
 * @param data The byte of data to write to the serial port.
 */
void writeSerialData(const int &serialPort, const unsigned char data)
{
    const unsigned char *ptr = &data;
    ssize_t bytesWritten = write(serialPort, ptr, sizeof(unsigned char));
    if (bytesWritten == -1)
    {
        std::cerr << "Error writing to the serial port." << std::endl;
    }
}

/**
 * @brief Reads data from a serial port.
 *
 * Continuously reads data from the specified serial port until the flag is set.
 * If the record flag is set, the data is also copied to a raw log.
 *
 * @param serialPort Reference to the serial port.
 * @param flag Atomic boolean to control the reading loop.
 * @param record Atomic boolean to control recording the data.
 */
void readSerialData(const int &serialPort, std::atomic<bool> &flag, std::atomic<bool> &record)
{
    const int bufferSize = 1024;
    char buffer[bufferSize];

    while (!flag)
    {
        ssize_t bytesRead = read(serialPort, buffer, bufferSize);
        if (bytesRead > 0)
        {
            if (record)
            {
                logger.copyToRawLog(buffer, bytesRead);
            }
            storage.copyToStorage(buffer, bytesRead);
        }
        else if (bytesRead == -1)
        {
            std::cerr << "Error reading from the serial port." << std::endl;
        }
    }
}

/**
 * @brief Generate a timestamp and store it in the provided buffer.
 *
 * The timestamp includes year (LSB), month, day, hour, minute, second, and milliseconds.
 *
 * @param buffer Pointer to the buffer where the timestamp will be stored.
 */
void generateTimestamp(uint8_t *buffer)
{
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm *timeInfo = std::localtime(&currentTime);
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch())
                            .count();

    buffer[1] = static_cast<uint8_t>(timeInfo->tm_year % 100);    // Year (LSB of year)
    buffer[2] = static_cast<uint8_t>(timeInfo->tm_mon + 1);       // Month
    buffer[3] = static_cast<uint8_t>(timeInfo->tm_mday);          // Day
    buffer[4] = static_cast<uint8_t>(timeInfo->tm_hour);          // Hour
    buffer[5] = static_cast<uint8_t>(timeInfo->tm_min);           // Minute
    buffer[6] = static_cast<uint8_t>(timeInfo->tm_sec);           // Second
    buffer[7] = static_cast<uint8_t>((milliseconds >> 8) & 0xFF); // MSB of milliseconds
    buffer[8] = static_cast<uint8_t>(milliseconds & 0xFF);        // LSB of milliseconds
}

/**
 * @brief Opens a serial port with specified settings.
 *
 * Attempts to open the serial port specified by 'portName' with read-write access and
 * non-blocking mode. Sets baud rate to 460800, 8 data bits, no parity, and 1 stop bit.
 *
 * @return true if the serial port is opened successfully, false otherwise.
 */
bool openSerialPort()
{
    portName = findSerialPort();
    serialPort = open(portName, O_RDWR | O_NOCTTY); // Opening serial port with non-blocking flag

    if (serialPort < 0)
    {
        std::cerr << "Failed to open the serial port." << std::endl;
        return false;
    }
    struct termios options;
    tcgetattr(serialPort, &options);

    options.c_cc[VMIN] = 0;  // Minimum number of characters for non-canonical read
    options.c_cc[VTIME] = 0; // Timeout in deciseconds (1 second)

    cfsetispeed(&options, BAUD);
    cfsetospeed(&options, BAUD);

    options.c_cflag &= ~PARENB; // No parity bit
    options.c_cflag &= ~CSTOPB; // 1 stop bit
    options.c_cflag &= ~CSIZE;  // Clear current char size mask
    options.c_cflag |= CS8;     // 8 data bits

    tcsetattr(serialPort, TCSANOW, &options); // Apply settings

    usleep(100000); // Delay of 100 milliseconds

    cout << "Serial port opened successfully.\n";
    return true;
}

/**
 * @brief Starts a thread for reading serial data.
 *
 * Creates a new thread to execute the 'readSerialData' function with the provided arguments.
 */
void startThread()
{
    readThread = thread(readSerialData, std::ref(serialPort), std::ref(stopFlag), std::ref(recording));
}

/**
 * @brief Cleans up resources and stops the read thread.
 *
 * Sets the stop flag, waits for the read thread to finish, closes log files,
 * and closes the serial port. If GUI logging is enabled, it also processes the
 * raw log for the GUI.
 */
void cleanup()
{
    stopFlag = true;
    readThread.join();
#ifdef GUI_LOG
    guiLogger.closeRawLog();
    guiLogger.parseRawLog("shownToGUI");
#endif
    logger.closeRawLog();
    close(serialPort);
}

/**
 * @brief Determines the type of packet based on the given MSB and LSB.
 *
 * Compares the MSB and LSB with predefined values to identify the packet type.
 *
 * @param MSB Most significant byte of the packet.
 * @param LSB Least significant byte of the packet.
 * @return The type of the packet as a Packet_t enum.
 */
Packet_t determinePacketType(char MSB, char LSB)
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

    if (((MSB & 0xFF) == 0xDD) && ((LSB & 0xFF) == 0xDD))
    {
        return ERROR_PACKET;
    }

    return UNDEFINED;
}

/**
 * @brief Converts an integer value to a voltage.
 *
 * Converts a given integer value to its corresponding voltage based on the
 * specified resolution, reference voltage, and multiplier.
 *
 * @param value The integer value to convert.
 * @param resolution The resolution of the ADC (e.g., 12 or 16 bits).
 * @param ref The reference voltage.
 * @param mult The multiplier to apply to the resulting voltage.
 * @return The calculated voltage.
 */
double intToVoltage(int value, int resolution, double ref, float mult)
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

/**
 * @brief Calculate temperature for ADHV4702 given voltage
 *
 */
float calculateTemperature(float tmpVoltage)
{

    // Calculate temperature
    float temperature = 25.0f + (tmpVoltage - 1.9f) / -0.0045f;

    return temperature;
}

/**
 * @brief Converts a raw temperature sensor value to Celsius.
 *
 * Converts a raw 12-bit temperature sensor value, accounting for potential
 * negative temperatures using 2's complement, to a Celsius temperature value.
 *
 * @param val The raw temperature sensor value.
 * @return The temperature in Celsius.
 */
double tempsToCelsius(int val)
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

    snprintf(convertedChar, 16, "%u.%u",
             ((unsigned int)temp_c / 100),
             ((unsigned int)temp_c % 100));

    convertedTemp = std::stod(convertedChar);

    return convertedTemp;
}

// ******************************************************************************************************************* CALLBACKS
/**
 * @brief Callback function for auto-startup.
 *
 * Activates specified pins and sets their values. Writes data to serial port for configuration.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback (unused).
 */
void autoStartUpCallback(Fl_Widget *)
{
    PB6->activate();
    PC10->activate();
    PC13->activate();
    PC7->activate();
    PC8->activate();
    PC9->activate();
    PC6->activate();

    SDN1->value(1);
    PB5->value(1);
    PC7->value(1);
    PC10->value(1);
    PC6->value(1);
    PC8->value(1);
    PC9->value(1);

    writeSerialData(serialPort, 0xE0);
}

/**
 * @brief Callback function for auto-shutdown.
 *
 * Deactivates specified pins and sets their values. Writes data to serial port for configuration.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback (unused).
 */
void autoShutDownCallback(Fl_Widget *)
{
    PB6->deactivate();
    PC10->deactivate();
    PC13->deactivate();
    PC7->deactivate();
    PC8->deactivate();
    PC9->deactivate();
    PC6->deactivate();

    SDN1->value(0);
    PB5->value(0);
    PC7->value(0);
    PC10->value(0);
    PC6->value(0);
    PC8->value(0);
    PC9->value(0);

    writeSerialData(serialPort, 0xD0);
}
bool waitForResponse()
{
    fd_set read_fds;
    struct timeval timeout;

    FD_ZERO(&read_fds);
    FD_SET(serialPort, &read_fds);

    timeout.tv_sec = 5; // wait for 1 second
    timeout.tv_usec = 0;

    int selectResult = select(serialPort + 1, &read_fds, NULL, NULL, &timeout);

    if (selectResult == -1)
    {
        std::cerr << "Error during select.\n";
        return false;
    }
    else if (selectResult == 0)
    {
        std::cerr << "Timeout waiting for response.\n";
        return false;
    }
    else
    {
        return true;
    }
}
/**
 * @brief Callback function for synchronization.
 *
 * Opens the serial port for communication and sends a timestamp to the microcontroller.
 * Waits to receive synchronization acknowledgment from the microcontroller.
 * Updates UI elements and activates necessary controls upon successful synchronization.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback (unused).
 */
void syncCallback(Fl_Widget *)
{

    // 1) Open serial port
    // 2) Send sync command (0xAF)
    // 3) Wait for ACK (0xFF) from iMCU
    // 4) Send timestamp to iMCU
    if (!openSerialPort())
    {
        cerr << "Sync failed on serial port.\n";
        return;
    }

    uint8_t rx_buffer[5];
    uint8_t tx_buffer[9];
    int bytesRead = 0;

    tx_buffer[0] = 0xAF;
    write(serialPort, tx_buffer, 1 * sizeof(uint8_t));

    if (waitForResponse())
    {
        rx_buffer[0] = 0x11;
        bytesRead = read(serialPort, rx_buffer, sizeof(uint8_t));
        if (bytesRead > 0 && rx_buffer[0] == 0xFF)
        {
            std::cout << "Initial ACK received from iMCU.\n";

            tx_buffer[0] = 0xFF;
            generateTimestamp(tx_buffer);

            write(serialPort, tx_buffer, 9 * sizeof(uint8_t));
            if (waitForResponse())
            {
                bytesRead = read(serialPort, rx_buffer, sizeof(uint8_t));
                if (bytesRead > 0 && rx_buffer[0] == 0xFF)
                {
                    std::cout << "Final ACK received from MCU.\n";
                }
                else
                {
                    std::cerr << "Failed to receive final valid ACK.\n";
                }
            }
        }
        else
        {
            std::cerr << "Failed to receive initial valid ACK.\n";
        }
    }

    startThread();

    syncWithInstruments->deactivate();
    stepUp->activate();
    stepDown->activate();
    enterStopMode->activate();
    exitStopMode->activate();
    increaseFactor->activate();
    decreaseFactor->activate();
    startRecording->activate();
    PMTOn->activate();
    ERPAOn->activate();
    HKOn->activate();
    PB5->activate();
    SDN1->activate();
    autoSweep->activate();
    autoStartUp->activate();
    autoShutDown->activate();
    startRecording->activate();
    scienceMode->activate();
    idleMode->activate();
    return;
}

/**
 * @brief Callback function to handle the quit action.
 *
 * Cleans up resources by calling the cleanup function and exits the application.
 *
 * @param widget The widget that triggered the callback.
 */
void quitCallback(Fl_Widget *)
{
    writeSerialData(serialPort, 0xD0); // Same command auto deinit sends, just ensuring everything gets turned off when quit button is pressed
    cleanup();
    exit(0);
}

/**
 * @brief Sends a stop mode command over the serial port.
 *
 * This function sends a stop mode command (0x0F) over the specified serial port.
 *
 * @param serialPort The file descriptor for the open serial port.
 */
void stopModeCallback(Fl_Widget *)
{
    writeSerialData(serialPort, 0x0F);
}

/**
 * @brief Sends exit stop mode commands over the serial port.
 *
 * This function sends exit stop mode commands (0x1F) over the specified serial port.
 *
 * @param serialPort The file descriptor for the open serial port.
 */
void exitStopModeCallback(Fl_Widget *)
{
    for (int i = 0; i < 12; i++)
    {
        writeSerialData(serialPort, 0x1F);
    }
}

/**
 * @brief Callback function to step up a process.
 *
 * This function sends a command to step up a process via the serial port.
 * If the current step is less than 7, it increments the step counter.
 *
 * @param widget The Fl_Widget triggering the callback.
 */
void stepUpCallback(Fl_Widget *)
{
    writeSerialData(serialPort, 0x1D);
    if (step < 7)
    {
        step++;
    }
}

/**
 * @brief Callback function to step down a process.
 *
 * This function sends a command to step down a process via the serial port.
 * If the current step is greater than 0, it decrements the step counter.
 *
 * @param widget The Fl_Widget triggering the callback.
 */
void stepDownCallback(Fl_Widget *)
{
    writeSerialData(serialPort, 0x0D);
    if (step > 0)
    {
        step--;
    }
}

/**
 * @brief Callback function to increase a factor.
 *
 * This function sends a command to increase a factor via the serial port.
 * If the current factor is less than 32, it doubles the current factor.
 *
 * @param widget The Fl_Widget triggering the callback.
 */
void factorUpCallback(Fl_Widget *)
{
    writeSerialData(serialPort, 0x1E);
    if (currentFactor < 32)
    {
        currentFactor *= 2;
    }
}

/**
 * @brief Callback function to decrease a factor.
 *
 * This function sends a command to decrease a factor via the serial port.
 * If the current factor is greater than 1, it divides the current factor by 2.
 *
 * @param widget The Fl_Widget triggering the callback.
 */
void factorDownCallback(Fl_Widget *)
{
    writeSerialData(serialPort, 0x0E);
    if (currentFactor > 1)
    {
        currentFactor /= 2;
    }
}

/**
 * @brief Callback function to handle the start and stop of recording.
 *
 * Toggles the recording state. If recording is not active, starts recording
 * and updates the button label to indicate recording status. If recording is
 * active, stops recording, updates the button label, and processes the recorded log.
 *
 * @param widget The widget that triggered the callback.
 */
void startRecordingCallback(Fl_Widget *)
{
    if (!recording)
    {
        startRecording->label("RECORDING @square");
        recording = true;
        logger.createRawLog("recordingData");
    }
    else
    {
        startRecording->label("RECORD @circle");
        recording = false;
        logger.closeRawLog();
        logger.parseRawLog("recordingData");
    }
}

/**
 * @brief Callback function to initiate auto sweep.
 *
 * This function sends a command to initiate auto sweep via the serial port.
 *
 * @param widget The Fl_Widget triggering the callback.
 */
void autoSweepCallback(Fl_Widget *)
{
    int autoSweeping = autoSweep->value();
    if (autoSweeping)
    {
        writeSerialData(serialPort, 0x19);
    }
    else
    {
        writeSerialData(serialPort, 0x09);
    }
}

/**
 * @brief Callback function for SDN1.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void SDN1Callback(Fl_Widget *widget)
{
    int sdn1On = SDN1->value();
    if (sdn1On)
    {
        writeSerialData(serialPort, 0x10);
    }
    else
    {
        writeSerialData(serialPort, 0x00);
    }
}

/**
 * @brief Callback function for PMT ON.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void PMTOnCallback(Fl_Widget *widget)
{
    int pmtOn = PMTOn->value();
    if (pmtOn)
    {
        writeSerialData(serialPort, 0x1B);
    }
    else
    {
        writeSerialData(serialPort, 0x0B);
    }
}

/**
 * @brief Callback function for ERPA ON.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void ERPAOnCallback(Fl_Widget *widget)
{
    int erpaOn = ERPAOn->value();
    if (erpaOn)
    {
        writeSerialData(serialPort, 0x1A);
    }
    else
    {
        writeSerialData(serialPort, 0x0A);
    }
}

/**
 * @brief Callback function for HK ON.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void HKOnCallback(Fl_Widget *widget)
{
    int hkOn = HKOn->value();
    if (hkOn)
    {
        writeSerialData(serialPort, 0x1C);
    }
    else
    {
        writeSerialData(serialPort, 0x0C);
    }
}

/**
 * @brief Callback function for PB5 state change.
 *
 * Handles the state change of PB5 button. If PB5 is turned on, it sends corresponding data
 * to the serial port and activates related controls. If PB5 is turned off, it sends data to
 * turn off related controls and updates internal state accordingly.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void PB5Callback(Fl_Widget *widget)
{
    int pb5On = PB5->value();
    if (pb5On)
    {
        writeSerialData(serialPort, 0x11);
        PB6->activate();
        PC10->activate();
        PC13->activate();
        PC7->activate();
        PC8->activate();
        PC9->activate();
        PC6->activate();
    }
    else
    {
        writeSerialData(serialPort, 0x01);
        PB6->deactivate();
        PC10->deactivate();
        PC13->deactivate();
        PC7->deactivate();
        PC8->deactivate();
        PC9->deactivate();
        PC6->deactivate();
        PB6->value(0);
        PC6->value(0);
        PC9->value(0);
        PC10->value(0);
        PC13->value(0);
        PC7->value(0);
        PC8->value(0);
    }
}

/**
 * @brief Callback function for PB6.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void PB6Callback(Fl_Widget *widget)
{
    int pb6On = PB6->value();
    if (pb6On)
    {
        writeSerialData(serialPort, 0x18);
    }
    else
    {
        writeSerialData(serialPort, 0x08);
    }
}

/**
 * @brief Callback function for PC10.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void PC10Callback(Fl_Widget *widget)
{
    int pc10On = PC10->value();
    if (pc10On)
    {
        writeSerialData(serialPort, 0x12);
    }
    else
    {
        writeSerialData(serialPort, 0x02);
    }
}

/**
 * @brief Callback function for PC13.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void PC13Callback(Fl_Widget *widget)
{
    int pc13On = PC13->value();
    if (pc13On)
    {
        writeSerialData(serialPort, 0x17);
    }
    else
    {
        writeSerialData(serialPort, 0x07);
    }
}

/**
 * @brief Callback function for PC17.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void PC7Callback(Fl_Widget *widget)
{
    int pc7On = PC7->value();
    if (pc7On)
    {
        writeSerialData(serialPort, 0x13);
    }
    else
    {
        writeSerialData(serialPort, 0x03);
    }
}

/**
 * @brief Callback function for PC8.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void PC8Callback(Fl_Widget *widget)
{
    int pc8On = PC8->value();
    if (pc8On)
    {
        writeSerialData(serialPort, 0x15);
    }
    else
    {
        writeSerialData(serialPort, 0x05);
    }
}

/**
 * @brief Callback function for PC9.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void PC9Callback(Fl_Widget *widget)
{
    int pc9On = PC9->value();
    if (pc9On)
    {
        writeSerialData(serialPort, 0x16);
    }
    else
    {
        writeSerialData(serialPort, 0x06);
    }
}

/**
 * @brief Callback function for PC6.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void PC6Callback(Fl_Widget *widget)
{
    int pc6On = PC6->value();
    if (pc6On)
    {
        writeSerialData(serialPort, 0x14);
    }
    else
    {
        writeSerialData(serialPort, 0x04);
    }
}

void scienceModeCallback(Fl_Widget *widget)
{
    writeSerialData(serialPort, 0xBF);
}

void idleModeCallback(Fl_Widget *widget)
{
    writeSerialData(serialPort, 0xCF);
}

/**
 * @brief The main entry point of the program.
 *
 * @return An integer representing the exit status of the program.
 */
int main()
{
    // ******************************************************************************************************************* GUI Widget Setup
    Fl_Color darkBackground = fl_rgb_color(28, 28, 30);
    Fl_Color text = fl_rgb_color(203, 207, 213);
    Fl_Color box = fl_rgb_color(46, 47, 56);
    Fl_Color output = fl_rgb_color(60, 116, 239);
    window = new Fl_Window(windowWidth, windowHeight, "IS Packet Interpreter");
    group6 = new Fl_Box(xGUIOffset + 285, yGUIOffset + 75, 130, 410, "GUI");
    group4 = new Fl_Box(xControlOffset + 15, yControlOffset + 75, 130, 410, "CONTROLS");
    group2 = new Fl_Box(xPacketOffset + 295, yPacketOffset, 200, 410, "ERPA PACKET");
    group1 = new Fl_Box(xPacketOffset + 15, yPacketOffset, 200, 410, "PMT PACKET");
    group3 = new Fl_Box(xPacketOffset + 575, yPacketOffset, 200, 410, "HK PACKET");
    ERPA1 = new Fl_Box(xPacketOffset + 300, yPacketOffset + 5, 50, 20, "SYNC:");
    ERPA2 = new Fl_Box(xPacketOffset + 300, yPacketOffset + 25, 50, 20, "SEQ:");
    ERPA4 = new Fl_Box(xPacketOffset + 300, yPacketOffset + 45, 50, 20, "SWP MON:");
    ERPA3 = new Fl_Box(xPacketOffset + 300, yPacketOffset + 65, 50, 20, "ADC:");
    PMT1 = new Fl_Box(xPacketOffset + 18, yPacketOffset + 5, 50, 20, "SYNC:");
    PMT2 = new Fl_Box(xPacketOffset + 18, yPacketOffset + 25, 50, 20, "SEQ:");
    PMT3 = new Fl_Box(xPacketOffset + 18, yPacketOffset + 45, 50, 20, "ADC:");
    HK1 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 5, 50, 20, "SYNC:");
    HK2 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 25, 50, 20, "SEQ:");
    HK14 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 45, 50, 20, "vsense:");
    HK15 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 65, 50, 20, "vrefint:");
    tempLabel1 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 85, 50, 20, "TEMP1:");
    tempLabel2 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 105, 50, 20, "TEMP2:");
    tempLabel3 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 125, 50, 20, "TEMP3:");
    tempLabel4 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 145, 50, 20, "TEMP4:");
    HK3 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 165, 50, 20, "BUSvmon:");
    HK4 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 185, 50, 20, "BUSimon:");
    HK8 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 205, 50, 20, "2v5mon:");
    HK5 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 225, 50, 20, "3v3mon:");
    HK10 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 245, 50, 20, "5vmon:");
    HK11 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 265, 50, 20, "n3v3mon:");
    HK9 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 285, 50, 20, "n5vmon:");
    HK13 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 305, 50, 20, "15vmon:");
    HK12 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 325, 50, 20, "5vrefmon:");
    HK6 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 345, 50, 20, "n200vmon:");
    HK7 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 365, 50, 20, "n800vmon:");
    HK16 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 385, 50, 20, "TMP1:");

    syncWithInstruments = new Fl_Button(xGUIOffset + 295, yGUIOffset + 90, 110, 40, "Sync");
    autoStartUp = new Fl_Button(xGUIOffset + 295, yGUIOffset + 130, 110, 40, "Auto Init");
    autoShutDown = new Fl_Button(xGUIOffset + 295, yGUIOffset + 170, 110, 40, "Auto DeInit");
    enterStopMode = new Fl_Button(xGUIOffset + 295, yGUIOffset + 210, 110, 40, "Sleep");
    exitStopMode = new Fl_Button(xGUIOffset + 295, yGUIOffset + 250, 110, 40, "Wake Up");
    startRecording = new Fl_Button(xGUIOffset + 295, yGUIOffset + 290, 110, 40, "RECORD @circle");
    scienceMode = new Fl_Button(xGUIOffset + 295, yGUIOffset + 330, 110, 40, "Science Mode");
    idleMode = new Fl_Button(xGUIOffset + 295, yGUIOffset + 370, 110, 40, "Idle Mode");

    quit = new Fl_Button(xGUIOffset + 295, yGUIOffset + 410, 110, 65, "Quit");
    stepUp = new Fl_Button(xPacketOffset + 305, yPacketOffset + 195, 180, 20, "Step Up");
    stepDown = new Fl_Button(xPacketOffset + 305, yPacketOffset + 245, 180, 20, "Step Down");
    increaseFactor = new Fl_Button(xPacketOffset + 305, yPacketOffset + 305, 180, 20, "Factor Up");
    decreaseFactor = new Fl_Button(xPacketOffset + 305, yPacketOffset + 355, 180, 20, "Factor Down");
    PMTOn = new Fl_Round_Button(xPacketOffset + 165, yPacketOffset - 18, 20, 20);
    ERPAOn = new Fl_Round_Button(xPacketOffset + 450, yPacketOffset - 18, 20, 20);
    HKOn = new Fl_Round_Button(xPacketOffset + 725, yPacketOffset - 18, 20, 20);
    PB5 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 80, 100, 50, "sys_on PB5");
    PC7 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 180, 100, 50, "5v_en PC7");
    PC10 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 130, 100, 50, "3v3_en PC10");
    PC6 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 230, 100, 50, "n3v3_en PC6");
    PC8 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 280, 100, 50, "n5v_en PC8");
    PC9 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 330, 100, 50, "15v_en PC9");
    PC13 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 380, 100, 50, "n200v_en PC13");
    PB6 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 430, 100, 50, "800v_en PB6");
    curFactor = new Fl_Output(xPacketOffset + 385, yPacketOffset + 330, 20, 20);
    currStep = new Fl_Output(xPacketOffset + 355, yPacketOffset + 220, 20, 20);
    stepVoltage = new Fl_Output(xPacketOffset + 400, yPacketOffset + 220, 20, 20);
    ERPAsync = new Fl_Output(xPacketOffset + 417, yPacketOffset + 5, 60, 20);
    ERPAseq = new Fl_Output(xPacketOffset + 417, yPacketOffset + 25, 60, 20);
    ERPAswp = new Fl_Output(xPacketOffset + 417, yPacketOffset + 45, 60, 20);
    ERPAadc = new Fl_Output(xPacketOffset + 417, yPacketOffset + 65, 60, 20);
    PMTsync = new Fl_Output(xPacketOffset + 135, yPacketOffset + 5, 60, 20);
    PMTseq = new Fl_Output(xPacketOffset + 135, yPacketOffset + 25, 60, 20);
    PMTadc = new Fl_Output(xPacketOffset + 135, yPacketOffset + 45, 60, 20);
    HKsync = new Fl_Output(xPacketOffset + 682, yPacketOffset + 5, 60, 20);
    HKseq = new Fl_Output(xPacketOffset + 682, yPacketOffset + 25, 60, 20);
    HKvsense = new Fl_Output(xPacketOffset + 682, yPacketOffset + 45, 60, 20);
    HKvrefint = new Fl_Output(xPacketOffset + 682, yPacketOffset + 65, 60, 20);
    HKtemp1 = new Fl_Output(xPacketOffset + 682, yPacketOffset + 85, 60, 20);
    HKtemp2 = new Fl_Output(xPacketOffset + 682, yPacketOffset + 105, 60, 20);
    HKtemp3 = new Fl_Output(xPacketOffset + 682, yPacketOffset + 125, 60, 20);
    HKtemp4 = new Fl_Output(xPacketOffset + 682, yPacketOffset + 145, 60, 20);
    HKbusvmon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 165, 60, 20);
    HKbusimon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 185, 60, 20);
    HK2v5mon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 205, 60, 20);
    HK3v3mon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 225, 60, 20);
    HK5vmon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 245, 60, 20);
    HKn3v3mon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 265, 60, 20);
    HKn5vmon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 285, 60, 20);
    HK15vmon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 305, 60, 20);
    HK5vrefmon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 325, 60, 20);
    HKn150vmon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 345, 60, 20);
    HKn800vmon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 365, 60, 20);
    HKtmp1 = new Fl_Output(xPacketOffset + 682, yPacketOffset + 385, 60, 20);

    SDN1 = new Fl_Light_Button(xPacketOffset + 305, yPacketOffset + 105, 150, 35, "  SDN1 High");
    autoSweep = new Fl_Light_Button(xPacketOffset + 305, yPacketOffset + 155, 150, 35, "  Auto Sweep");
    guiVersion = new Fl_Output(5, 575, 100, 20);
    instrumentVersion = new Fl_Output(110, 575, 100, 20);
    dateTime = new Fl_Output(215, 575, 200, 20);
    errorCodeOutput = new Fl_Output(550, 575, 100, 20);

    // Main window styling
    window->color(darkBackground);
    dateTime->color(darkBackground);
    dateTime->value(0);
    dateTime->box(FL_FLAT_BOX);
    dateTime->textcolor(output);
    dateTime->labelsize(2);
    guiVersion->color(darkBackground);
    guiVersion->box(FL_FLAT_BOX);
    guiVersion->textcolor(output);
    guiVersion->labelsize(2);
    guiVersion->value(GUI_VERSION_NUM);
    instrumentVersion->color(darkBackground);
    instrumentVersion->box(FL_FLAT_BOX);
    instrumentVersion->textcolor(output);
    instrumentVersion->labelsize(2);
    instrumentVersion->value("I-x.y.z-n");

    errorCodeOutput->color(darkBackground);
    errorCodeOutput->box(FL_FLAT_BOX);
    errorCodeOutput->textcolor(output);
    errorCodeOutput->labelsize(2);
    errorCodeOutput->value("ERROR: NULL");

    // GUI group styling
    group6->color(box);
    group6->box(FL_BORDER_BOX);
    group6->labelcolor(text);
    group6->labelfont(FL_BOLD);
    group6->align(FL_ALIGN_TOP);
    startRecording->labelcolor(FL_RED);
    startRecording->callback(startRecordingCallback);
    enterStopMode->callback(stopModeCallback);
    exitStopMode->callback(exitStopModeCallback);
    autoStartUp->callback(autoStartUpCallback);
    autoShutDown->callback(autoShutDownCallback);
    syncWithInstruments->align(FL_ALIGN_CENTER);
    syncWithInstruments->callback(syncCallback);
    scienceMode->callback(scienceModeCallback);
    idleMode->callback(idleModeCallback);
    quit->align(FL_ALIGN_CENTER);
    quit->color(FL_RED);
    quit->callback(quitCallback);

    // Control group styling
    group4->color(box);
    group4->box(FL_BORDER_BOX);
    group4->labelcolor(text);
    group4->labelfont(FL_BOLD);
    group4->align(FL_ALIGN_TOP);
    PB5->labelcolor(text);
    PB5->callback(PB5Callback);
    PB6->labelcolor(text);
    PB6->callback(PB6Callback);
    PC10->labelcolor(text);
    PC10->callback(PC10Callback);
    PC13->labelcolor(text);
    PC13->callback(PC13Callback);
    PC7->labelcolor(text);
    PC7->callback(PC7Callback);
    PC8->labelcolor(text);
    PC8->callback(PC8Callback);
    PC9->labelcolor(text);
    PC9->callback(PC9Callback);
    PC6->labelcolor(text);
    PC6->callback(PC6Callback);

    // PMT group styling
    group1->color(box);
    group1->box(FL_BORDER_BOX);
    group1->labelcolor(text);
    group1->labelfont(FL_BOLD);
    group1->align(FL_ALIGN_TOP);
    PMTOn->callback(PMTOnCallback);
    PMTsync->color(box);
    PMTsync->value(0);
    PMTsync->box(FL_FLAT_BOX);
    PMTsync->textcolor(output);
    PMT1->box(FL_FLAT_BOX);
    PMT1->color(box);
    PMT1->labelfont();
    PMT1->labelcolor(text);
    PMT1->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    PMTseq->color(box);
    PMTseq->value(0);
    PMTseq->box(FL_FLAT_BOX);
    PMTseq->textcolor(output);
    PMT2->box(FL_FLAT_BOX);
    PMT2->color(box);
    PMT2->labelfont();
    PMT2->labelcolor(text);
    PMT2->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    PMTadc->color(box);
    PMTadc->value(0);
    PMTadc->box(FL_FLAT_BOX);
    PMTadc->textcolor(output);
    PMT3->box(FL_FLAT_BOX);
    PMT3->color(box);
    PMT3->labelfont();
    PMT3->labelcolor(text);
    PMT3->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    // ERPA group styling
    group2->color(box);
    group2->box(FL_BORDER_BOX);
    group2->labelcolor(text);
    group2->labelfont(FL_BOLD);
    group2->align(FL_ALIGN_TOP);
    ERPAOn->callback(ERPAOnCallback);
    SDN1->selection_color(FL_GREEN);
    SDN1->box(FL_FLAT_BOX);
    SDN1->color(box);
    SDN1->labelcolor(text);
    SDN1->callback(SDN1Callback);
    SDN1->labelsize(16);
    autoSweep->selection_color(FL_GREEN);
    autoSweep->box(FL_FLAT_BOX);
    autoSweep->color(box);
    autoSweep->labelcolor(text);
    autoSweep->callback(autoSweepCallback);
    autoSweep->labelsize(16);
    ERPAsync->color(box);
    ERPAsync->value(0);
    ERPAsync->box(FL_FLAT_BOX);
    ERPAsync->textcolor(output);
    ERPA1->box(FL_FLAT_BOX);
    ERPA1->color(box);
    ERPA1->labelfont();
    ERPA1->labelcolor(text);
    ERPA1->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    ERPAseq->color(box);
    ERPAseq->value(0);
    ERPAseq->box(FL_FLAT_BOX);
    ERPAseq->textcolor(output);
    ERPA2->box(FL_FLAT_BOX);
    ERPA2->color(box);
    ERPA2->labelfont();
    ERPA2->labelcolor(text);
    ERPA2->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    ERPAswp->color(box);
    ERPAswp->value(0);
    ERPAswp->box(FL_FLAT_BOX);
    ERPAswp->textcolor(output);
    ERPA4->box(FL_FLAT_BOX);
    ERPA4->color(box);
    ERPA4->labelfont();
    ERPA4->labelcolor(text);
    ERPA4->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HKtmp1->color(box);
    HKtmp1->value(0);
    HKtmp1->box(FL_FLAT_BOX);
    HKtmp1->textcolor(output);
    HK16->box(FL_FLAT_BOX);
    HK16->color(box);
    HK16->labelfont();
    HK16->labelcolor(text);
    HK16->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    ERPAadc->color(box);
    ERPAadc->value(0);
    ERPAadc->box(FL_FLAT_BOX);
    ERPAadc->textcolor(output);
    ERPA3->box(FL_FLAT_BOX);
    ERPA3->color(box);
    ERPA3->labelfont();
    ERPA3->labelcolor(text);
    ERPA3->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    curFactor->color(box);
    curFactor->value(0);
    curFactor->box(FL_FLAT_BOX);
    curFactor->textcolor(output);
    currStep->color(box);
    currStep->value(0);
    currStep->box(FL_FLAT_BOX);
    currStep->textcolor(output);
    stepVoltage->color(box);
    stepVoltage->value(0);
    stepVoltage->box(FL_FLAT_BOX);
    stepVoltage->textcolor(output);
    stepDown->callback(stepDownCallback);
    stepUp->callback(stepUpCallback);
    stepUp->label("Step Up         @8->");
    stepUp->align(FL_ALIGN_CENTER);
    stepDown->label("Step Down     @2->");
    stepDown->align(FL_ALIGN_CENTER);
    increaseFactor->callback(factorUpCallback);
    decreaseFactor->callback(factorDownCallback);
    increaseFactor->label("Factor Up       @8->");
    increaseFactor->align(FL_ALIGN_CENTER);
    decreaseFactor->label("Factor Down  @2->");
    decreaseFactor->align(FL_ALIGN_CENTER);

    // HK group styling
    group3->color(box);
    group3->box(FL_BORDER_BOX);
    group3->labelcolor(text);
    group3->labelfont(FL_BOLD);
    group3->align(FL_ALIGN_TOP);
    HKOn->callback(HKOnCallback);
    HKsync->color(box);
    HKsync->value(0);
    HKsync->box(FL_FLAT_BOX);
    HKsync->textcolor(output);
    HK1->box(FL_FLAT_BOX);
    HK1->color(box);
    HK1->labelfont();
    HK1->labelcolor(text);
    HK1->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HKseq->color(box);
    HKseq->value(0);
    HKseq->box(FL_FLAT_BOX);
    HKseq->textcolor(output);
    HK2->box(FL_FLAT_BOX);
    HK2->color(box);
    HK2->labelfont();
    HK2->labelcolor(text);
    HK2->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HKvsense->color(box);
    HKvsense->value(0);
    HKvsense->box(FL_FLAT_BOX);
    HKvsense->textcolor(output);
    HK14->box(FL_FLAT_BOX);
    HK14->color(box);
    HK14->labelfont();
    HK14->labelcolor(text);
    HK14->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HKvrefint->color(box);
    HKvrefint->value(0);
    HKvrefint->box(FL_FLAT_BOX);
    HKvrefint->textcolor(output);
    HK15->box(FL_FLAT_BOX);
    HK15->color(box);
    HK15->labelfont();
    HK15->labelcolor(text);
    HK15->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HKtemp1->color(box);
    HKtemp1->value(0);
    HKtemp1->box(FL_FLAT_BOX);
    HKtemp1->textcolor(output);
    tempLabel1->box(FL_FLAT_BOX);
    tempLabel1->color(box);
    tempLabel1->labelfont();
    tempLabel1->labelcolor(text);
    tempLabel1->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HKtemp2->color(box);
    HKtemp2->value(0);
    HKtemp2->box(FL_FLAT_BOX);
    HKtemp2->textcolor(output);
    tempLabel2->color(box);
    tempLabel2->box(FL_FLAT_BOX);
    tempLabel2->labelfont();
    tempLabel2->labelcolor(text);
    tempLabel2->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HKtemp3->color(box);
    HKtemp3->value(0);
    HKtemp3->box(FL_FLAT_BOX);
    HKtemp3->textcolor(output);
    tempLabel3->color(box);
    tempLabel3->box(FL_FLAT_BOX);
    tempLabel3->labelfont();
    tempLabel3->labelcolor(text);
    tempLabel3->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HKtemp4->color(box);
    HKtemp4->value(0);
    HKtemp4->box(FL_FLAT_BOX);
    HKtemp4->textcolor(output);
    tempLabel4->color(box);
    tempLabel4->box(FL_FLAT_BOX);
    tempLabel4->labelfont();
    tempLabel4->labelcolor(text);
    tempLabel4->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HKbusvmon->color(box);
    HKbusvmon->value(0);
    HKbusvmon->box(FL_FLAT_BOX);
    HKbusvmon->textcolor(output);
    HK3->box(FL_FLAT_BOX);
    HK3->color(box);
    HK3->labelfont();
    HK3->labelcolor(text);
    HK3->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HKbusimon->color(box);
    HKbusimon->value(0);
    HKbusimon->box(FL_FLAT_BOX);
    HKbusimon->textcolor(output);
    HK4->box(FL_FLAT_BOX);
    HK4->color(box);
    HK4->labelfont();
    HK4->labelcolor(text);
    HK4->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HK2v5mon->color(box);
    HK2v5mon->value(0);
    HK2v5mon->box(FL_FLAT_BOX);
    HK2v5mon->textcolor(output);
    HK8->box(FL_FLAT_BOX);
    HK8->color(box);
    HK8->labelfont();
    HK8->labelcolor(text);
    HK8->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HK3v3mon->color(box);
    HK3v3mon->value(0);
    HK3v3mon->box(FL_FLAT_BOX);
    HK3v3mon->textcolor(output);
    HK5->box(FL_FLAT_BOX);
    HK5->color(box);
    HK5->labelfont();
    HK5->labelcolor(text);
    HK5->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HK5vmon->color(box);
    HK5vmon->value(0);
    HK5vmon->box(FL_FLAT_BOX);
    HK5vmon->textcolor(output);
    HK10->box(FL_FLAT_BOX);
    HK10->color(box);
    HK10->labelfont();
    HK10->labelcolor(text);
    HK10->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HKn3v3mon->color(box);
    HKn3v3mon->value(0);
    HKn3v3mon->box(FL_FLAT_BOX);
    HKn3v3mon->textcolor(output);
    HK11->box(FL_FLAT_BOX);
    HK11->color(box);
    HK11->labelfont();
    HK11->labelcolor(text);
    HK11->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HKn5vmon->color(box);
    HKn5vmon->value(0);
    HKn5vmon->box(FL_FLAT_BOX);
    HKn5vmon->textcolor(output);
    HK9->box(FL_FLAT_BOX);
    HK9->color(box);
    HK9->labelfont();
    HK9->labelcolor(text);
    HK9->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HK15vmon->color(box);
    HK15vmon->value(0);
    HK15vmon->box(FL_FLAT_BOX);
    HK15vmon->textcolor(output);
    HK13->color(box);
    HK13->box(FL_FLAT_BOX);
    HK13->labelfont();
    HK13->labelcolor(text);
    HK13->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HK5vrefmon->color(box);
    HK5vrefmon->value(0);
    HK5vrefmon->box(FL_FLAT_BOX);
    HK5vrefmon->textcolor(output);
    HK12->box(FL_FLAT_BOX);
    HK12->color(box);
    HK12->labelfont();
    HK12->labelcolor(text);
    HK12->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HKn150vmon->color(box);
    HKn150vmon->value(0);
    HKn150vmon->box(FL_FLAT_BOX);
    HKn150vmon->textcolor(output);
    HK6->box(FL_FLAT_BOX);
    HK6->color(box);
    HK6->labelfont();
    HK6->labelcolor(text);
    HK6->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    HKn800vmon->color(box);
    HKn800vmon->value(0);
    HKn800vmon->box(FL_FLAT_BOX);
    HKn800vmon->textcolor(output);
    HK7->box(FL_FLAT_BOX);
    HK7->color(box);
    HK7->labelfont();
    HK7->labelcolor(text);
    HK7->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    // ******************************************************************************************************************* Pre-Startup Operations
    stepUp->deactivate();
    stepDown->deactivate();
    enterStopMode->deactivate();
    exitStopMode->deactivate();
    startRecording->deactivate();
    scienceMode->deactivate();
    idleMode->deactivate();
    increaseFactor->deactivate();
    decreaseFactor->deactivate();
    PMTOn->deactivate();
    ERPAOn->deactivate();
    HKOn->deactivate();
    PB5->deactivate();
    PC7->deactivate();
    PC10->deactivate();
    PC6->deactivate();
    PC8->deactivate();
    PC9->deactivate();
    PC13->deactivate();
    PB6->deactivate();
    SDN1->deactivate();
    autoSweep->deactivate();
    autoStartUp->deactivate();
    autoShutDown->deactivate();
    ERPAOn->value(0);
    HKOn->value(0);

    window->show();
    Fl::check();
    int errorCount = 0;
#ifdef GUI_LOG
    guiLogger.createRawLog("shownToGUI");
#endif
    // ******************************************************************************************************************* Event Loop
    while (1)
    {
        snprintf(buffer, sizeof(buffer), "%d", currentFactor);
        curFactor->value(buffer);
        snprintf(buffer, sizeof(buffer), "%d", step);
        currStep->value(buffer);
        snprintf(buffer, sizeof(buffer), "%f", stepVoltages[step]);
        stepVoltage->value(buffer);

        char bytes[150000];
        int bytesRead = 0;
        int index = 0;
        Packet_t packetType;

        bytesRead = storage.getNextBytes(bytes);
        while (index < bytesRead)
        {
            packetType = determinePacketType(bytes[index], bytes[index + 1]);
            // cout << hex << static_cast<int>(bytes[index]) << endl;
            switch (packetType)
            {
            case ERROR_PACKET:
            {
                index += 2; // Skipping sync word
                uint8_t tag = bytes[index];
                string errorName;
                index++;

                switch (tag)
                {
                case 0:
                {
                    errorName = "RAIL_BUSVMON";
                    break;
                }
                case 1:
                {
                    errorName = "RAIL_BUSIMON";
                    break;
                }
                case 2:
                {
                    errorName = "RAIL_2v5";
                    break;
                }
                case 3:
                {
                    errorName = "RAIL_3v3";
                    break;
                }
                case 4:
                {
                    errorName = "RAIL_5v";
                    break;
                }
                case 5:
                {
                    errorName = "RAIL_n3v3";
                    break;
                }
                case 6:
                {
                    errorName = "RAIL_n5v";
                    break;
                }
                case 7:
                {
                    errorName = "RAIL_15v";
                    break;
                }
                case 8:
                {
                    errorName = "RAIL_5vref";
                    break;
                }
                case 9:
                {
                    errorName = "RAIL_n200v";
                    break;
                }
                case 10:
                {
                    errorName = "RAIL_n800v";
                    break;
                }
                default:
                {
                    errorName = "UNKNOWN";
                    break;
                }
                }
                cout << errorCount << " ERROR ON " << errorName << endl;
                errorCount++;
                errorCodeOutput->value(errorName.c_str());
                break;
            }
            case PMT:
            {
                if (index + PMT_PACKET_SIZE >= bytesRead)
                {
                    index = bytesRead;
                    break;
                }

#ifdef GUI_LOG
                guiLogger.copyToRawLog(bytes + index, PMT_PACKET_SIZE);
#endif
                char res[50];
                int value;
                value = ((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF);
                index += 2;
                snprintf(res, 50, "0x%X", value);
                PMTsync->value(res);

                value = ((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF);
                index += 2;
                snprintf(res, 50, "%04d", value);
                PMTseq->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%08.7f", intToVoltage(value, 16, 5, 1.0));
                PMTadc->value(res);

                value = ((bytes[index] & 0xFF) << 24) | ((bytes[index + 1] & 0xFF) << 16) | ((bytes[index + 2] & 0xFF) << 8) | (bytes[index + 3] & 0xFF);
                index += 4;
                snprintf(res, 50, "%06d", value);
                break;
            }
            case ERPA:
            {
                if (index + ERPA_PACKET_SIZE >= bytesRead)
                {
                    index = bytesRead;
                    break;
                }

#ifdef GUI_LOG
                guiLogger.copyToRawLog(bytes + index, ERPA_PACKET_SIZE);
#endif
                char res[50];
                uint32_t value;
                value = ((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF);
                index += 2;
                snprintf(res, 50, "0x%X", value);
                ERPAsync->value(res);

                value = ((bytes[index] & 0xFF) << 16) | ((bytes[index + 1] & 0xFF) << 8) | (bytes[index + 2] & 0xFF);
                index += 3;
                snprintf(res, 50, "%04d", value);
                ERPAseq->value(res);

                index++; // GUI doesn't need to do anything with the step sent from the iMCU, only useful for obc and science people (not me)

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
                ERPAswp->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%08.7f", intToVoltage(value, 16, 5, 1.0));
                ERPAadc->value(res);

                value = ((bytes[index] & 0xFF) << 24) | ((bytes[index + 1] & 0xFF) << 16) | ((bytes[index + 2] & 0xFF) << 8) | (bytes[index + 3] & 0xFF);
                index += 4;
                snprintf(res, 50, "%06d", value);
                break;
            }
            case HK:
            {

                if (index + HK_PACKET_SIZE >= bytesRead)
                {
                    index = bytesRead;
                    break;
                }

#ifdef GUI_LOG
                guiLogger.copyToRawLog(bytes + index, HK_PACKET_SIZE);
#endif
                char res[50];
                int value;
                value = ((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF);
                index += 2;
                snprintf(res, 50, "0x%X", value);
                HKsync->value(res);

                value = ((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF);
                index += 2;
                snprintf(res, 50, "%04d", value);
                HKseq->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
                HKvsense->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
                HKvrefint->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", tempsToCelsius(value));
                HKtemp1->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", tempsToCelsius(value));
                HKtemp2->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", tempsToCelsius(value));
                HKtemp3->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", tempsToCelsius(value));
                HKtemp4->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
                HKbusvmon->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
                HKbusimon->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
                HK2v5mon->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
                HK3v3mon->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
                HK5vmon->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
                HKn3v3mon->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
                HKn5vmon->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
                HK15vmon->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
                HK5vrefmon->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
                HKn150vmon->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", intToVoltage(value, 12, 3.3, 1.0));
                HKn800vmon->value(res);

                value = (((bytes[index] & 0xFF) << 8) | (bytes[index + 1] & 0xFF));
                index += 2;
                snprintf(res, 50, "%06.5f", calculateTemperature(intToVoltage(value, 12, 3.3, 1.0)));
                HKtmp1->value(res);

                index += 8; // skipping datetime and uptime
                break;
            }
            default:
            {
                index++;
                break;
            }
            }
        }

        window->redraw(); // Refreshing main window with new data every loop
        Fl::check();
    }
    return Fl::run();
}
