/**
 * @file instrumentGUI.cpp
 * @author Jared Morrison, Jared King, Shane Woods
 * @version 1.0.0-beta
 * @section DESCRIPTION
 *
 * GUI that connects to H7-Instrument-Software and shows packet data in real time
 */

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
const char* findSerialPort() {
    const char* devPath = "/dev/";
    const char* prefix = "cu.usbserial-"; // Your prefix here
    DIR* dir = opendir(devPath);
    if (dir == nullptr) {
        std::cerr << "Error opening directory" << std::endl;
        return nullptr;
    }

    dirent* entry;
    const char* portName = nullptr;

    while ((entry = readdir(dir)) != nullptr) {
        const char* filename = entry->d_name;
        if (strstr(filename, prefix) != nullptr) {
            // Use strncpy to copy the path to a buffer
            char buffer[1024]; // Adjust the buffer size as needed
            snprintf(buffer, sizeof(buffer), "%s%s", devPath, filename);
            portName = buffer;
            cout << "Using port: " << portName << endl;
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
 * @brief Reads data from the serial port and writes it to an output file.
 *
 * This function continuously reads data from the specified serial port and writes it to the specified output file.
 * It stops when the stopFlag is set to true.
 *
 * @param serialPort The file descriptor for the open serial port.
 * @param flag Atomic boolean flag indicating whether to stop reading.
 * @param outputFile The output file stream where the data will be written.
 */
void readSerialData(const int &serialPort, std::atomic<bool> &flag, std::ofstream &outputFile)
{
    const int bufferSize = 64;
    char buffer[bufferSize + 1];
    int flushInterval = 1000; // Flush the file every 1000 bytes
    int bytesReadTotal = 0;

    while (!flag)
    {
        ssize_t bytesRead = read(serialPort, buffer, bufferSize - 1);
        if (bytesRead > 0)
        {
            // buffer[bytesRead] = '\0';
            outputFile.write(buffer, bytesRead);
            bytesReadTotal += bytesRead;
            if (bytesReadTotal >=
                flushInterval) // Flush and truncate the file if the byte count exceeds the flush interval
            {
                outputFile.flush();
                outputFile.close();
                truncate("mylog.0", 0);
                outputFile.open("mylog.0", std::ios::app);
                bytesReadTotal = 0;
            }
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
    if (serialPort == -1)
    {
        std::cerr << "Failed to open the serial port." << std::endl;
        return false;
    }

    struct termios options;
    tcgetattr(serialPort, &options);

    options.c_cc[VMIN] = 0;  // Minimum number of characters for non-canonical read
    options.c_cc[VTIME] = 0; // Timeout in deciseconds (1 second)

    cfsetispeed(&options, 460800);
    cfsetospeed(&options, 460800);

    options.c_cflag &= ~PARENB; // No parity bit
    options.c_cflag &= ~CSTOPB; // 1 stop bit
    options.c_cflag &= ~CSIZE;  // Clear current char size mask
    options.c_cflag |= CS8;     // 8 data bits

    tcsetattr(serialPort, TCSANOW, &options); // Apply settings
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
    readThread = std::thread(readSerialData, std::ref(serialPort), std::ref(stopFlag), std::ref(outputFile));
}

/**
 * @brief Cleans up resources used for serial communication and threading.
 *
 * Stops the thread for reading serial data, closes the output file, and closes the serial port.
 */
void cleanup()
{
    stopFlag = true;
    readThread.join();
    outputFile.close();
    close(serialPort);
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

    writeSerialData(serialPort, 0x0B); // sdn1
    writeSerialData(serialPort, 0x00); // pb5
    writeSerialData(serialPort, 0x04); // pc7
    writeSerialData(serialPort, 0x02); // pc10
    writeSerialData(serialPort, 0x07); // pc6
    writeSerialData(serialPort, 0x05); // pc8
    writeSerialData(serialPort, 0x06); // pc9
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

    writeSerialData(serialPort, 0x19); // pc9
    writeSerialData(serialPort, 0x18); // pc8
    writeSerialData(serialPort, 0x1A); // pc6
    writeSerialData(serialPort, 0x15); // pc10
    writeSerialData(serialPort, 0x17); // pc7
    writeSerialData(serialPort, 0x13); // pb5
    writeSerialData(serialPort, 0x0A); // sdn1
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
    if (!openSerialPort())
    {
        cerr << "Sync failed on serial port.\n";
        return;
    }
    uint8_t rx_buffer[5];
    uint8_t tx_buffer[9];
    uint8_t key = 0x00;
    int bytesRead = 0;
    int attempts = 0;

    // Set first byte of tx_buffer to 0xFF and fill rest with timestamp info
    tx_buffer[0] = 0xFF;
    generateTimestamp(tx_buffer);
    write(serialPort, tx_buffer, 9 * sizeof(uint8_t));

    // Wait to receive 0xFA (along with version #) from MCU
    do
    {
        bytesRead = read(serialPort, rx_buffer, 5 * sizeof(uint8_t));
        if (bytesRead > 0)
        {
            key = rx_buffer[0];
        }
        attempts++;
    } while (key != 0xFA && attempts < 30000);

    if (attempts >= 29999)
    {
        cerr << "Sync failed on attempts, attempts taken: " << attempts << endl;
        return;
    }
    else
    {
        // [0]    [1]    [2]    [3]    [4]
        // 0xFA     1      0      0      2   = I-1.0.0-beta
        cout << "ATTEMPTS: " << attempts << endl;
        string versionNum = "I-";
        versionNum += to_string(rx_buffer[1]) + "." + to_string(rx_buffer[2]) + "." + to_string(rx_buffer[3]);
        switch (rx_buffer[4])
        {
        case 0:
        {
            // No alpha or beta
            break;
        }
        case 1:
        {
            versionNum += "-alpha";
            break;
        }
        case 2:
        {
            versionNum += "-beta";
            break;
        }
        default:
        {
            break;
        }
        }
        cout << versionNum << endl;
        instrumentVersion->value(versionNum.c_str());
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
        return;
    }

    // Should never get here, but just to be safe:
    cerr << "Sync failed, cause unknown.\n";
    return;
}

/**
 * @brief Callback function to start or stop recording logs.
 *
 * This function toggles the recording state. When recording starts, it creates new log files for ERPA, PMT, and HK,
 * opens the file streams, and writes headers to the log files. When recording stops, it closes the log file streams.
 *
 * @param widget Pointer to the FLTK widget triggering this callback.
 */
void startRecordingCallback(Fl_Widget *widget)
{
    if (!recording)
    {
        recording = true;
        startRecording->label("RECORDING @square");
        createNewLogs();
    }
    else
    {
        recording = false;
        startRecording->label("RECORD @circle");
        closeLogs();
    }
}

/**
 * @brief Callback function to quit the application.
 *
 * This function closes the controls log stream and terminates the application.
 *
 * @param widget Unused parameter for the FLTK widget triggering this callback.
 */
void quitCallback(Fl_Widget *)
{
    cleanup();
    exit(0);
}

/**
 * @brief Sends a stop mode command over the serial port.
 *
 * This function sends a stop mode command (0x0C) over the specified serial port.
 *
 * @param serialPort The file descriptor for the open serial port.
 */
void stopModeCallback(Fl_Widget *)
{
    writeSerialData(serialPort, 0x0C);
}

/**
 * @brief Sends exit stop mode commands over the serial port.
 *
 * This function sends exit stop mode commands (0x5B) over the specified serial port.
 *
 * @param serialPort The file descriptor for the open serial port.
 */
void exitStopModeCallback(Fl_Widget *)
{
    for (int i = 0; i < 12; i++)
    {
        writeSerialData(serialPort, 0x5B);
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
    writeSerialData(serialPort, 0x1B);
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
    writeSerialData(serialPort, 0x1C);
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
    writeSerialData(serialPort, 0x24);
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
    writeSerialData(serialPort, 0x25);
    if (currentFactor > 1)
    {
        currentFactor /= 2;
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
    writeSerialData(serialPort, 0x1D);
}

/**
 * @brief Callback function for SDN1.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void SDN1Callback(Fl_Widget *widget)
{
    int currValue = ((Fl_Light_Button *)widget)->value();
    if (currValue == 1)
    {
        writeSerialData(serialPort, 0x0B);
        Controls.c_sdn1 = true;
        writeToLog(Controls);
    }
    else
    {
        writeSerialData(serialPort, 0x0A);
        Controls.c_sdn1 = false;
        writeToLog(Controls);
    }
}

/**
 * @brief Callback function for PMT ON.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void PMTOnCallback(Fl_Widget *widget)
{
    int currValue = ((Fl_Round_Button *)widget)->value();
    if (currValue == 1)
    {
        writeSerialData(serialPort, 0x0D);
        Controls.c_pmtOn = true;
        writeToLog(Controls);
    }
    else
    {
        writeSerialData(serialPort, 0x10);
        Controls.c_pmtOn = false;
        writeToLog(Controls);
    }
}

/**
 * @brief Callback function for ERPA ON.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void ERPAOnCallback(Fl_Widget *widget)
{
    int currValue = ((Fl_Round_Button *)widget)->value();
    if (currValue == 1)
    {
        writeSerialData(serialPort, 0x0E);
        Controls.c_erpaOn = true;
        writeToLog(Controls);
    }
    else
    {
        writeSerialData(serialPort, 0x11);
        Controls.c_erpaOn = false;
        writeToLog(Controls);
    }
}

/**
 * @brief Callback function for HK ON.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void HKOnCallback(Fl_Widget *widget)
{
    int currValue = ((Fl_Round_Button *)widget)->value();
    if (currValue == 1)
    {
        writeSerialData(serialPort, 0x0F);
        Controls.c_hkOn = true;
        writeToLog(Controls);
    }
    else
    {
        writeSerialData(serialPort, 0x12);
        Controls.c_hkOn = false;
        writeToLog(Controls);
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
    bool PB5IsOn = ((Fl_Round_Button *)widget)->value();
    if (PB5IsOn)
    {
        writeSerialData(serialPort, 0x00);
        Controls.c_sysOn = true;
        writeToLog(Controls);
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
        writeSerialData(serialPort, 0x13);
        writeSerialData(serialPort, 0x14);
        writeSerialData(serialPort, 0x15);
        writeSerialData(serialPort, 0x16);
        writeSerialData(serialPort, 0x17);
        writeSerialData(serialPort, 0x18);
        writeSerialData(serialPort, 0x19);
        writeSerialData(serialPort, 0x1A);
        Controls.c_sysOn = false;
        writeToLog(Controls);
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
    int currValue = ((Fl_Round_Button *)widget)->value();
    if (currValue == 1)
    {
        writeSerialData(serialPort, 0x01);
        Controls.c_800v = true;
        writeToLog(Controls);
    }
    else
    {
        writeSerialData(serialPort, 0x14);
        Controls.c_800v = false;
        writeToLog(Controls);
    }
}

/**
 * @brief Callback function for PC10.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void PC10Callback(Fl_Widget *widget)
{
    int currValue = ((Fl_Round_Button *)widget)->value();
    if (currValue == 1)
    {
        writeSerialData(serialPort, 0x04);
        Controls.c_3v3 = true;
        writeToLog(Controls);
    }
    else
    {
        writeSerialData(serialPort, 0x17);
        Controls.c_3v3 = false;
        writeToLog(Controls);
    }
}

/**
 * @brief Callback function for PC13.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void PC13Callback(Fl_Widget *widget)
{
    int currValue = ((Fl_Round_Button *)widget)->value();
    if (currValue == 1)
    {
        writeSerialData(serialPort, 0x03);
        Controls.c_n150v = true;
        writeToLog(Controls);
    }
    else
    {
        writeSerialData(serialPort, 0x16);
        Controls.c_n150v = false;
        writeToLog(Controls);
    }
}

/**
 * @brief Callback function for PC17.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void PC7Callback(Fl_Widget *widget)
{
    int currValue = ((Fl_Round_Button *)widget)->value();
    if (currValue == 1)
    {
        writeSerialData(serialPort, 0x02);
        Controls.c_5v = true;
        writeToLog(Controls);
    }
    else
    {
        writeSerialData(serialPort, 0x15);
        Controls.c_5v = false;
        writeToLog(Controls);
    }
}

/**
 * @brief Callback function for PC8.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void PC8Callback(Fl_Widget *widget)
{
    int currValue = ((Fl_Round_Button *)widget)->value();
    if (currValue == 1)
    {
        writeSerialData(serialPort, 0x05);
        Controls.c_n5v = true;
        writeToLog(Controls);
    }
    else
    {
        writeSerialData(serialPort, 0x18);
        Controls.c_n5v = false;
        writeToLog(Controls);
    }
}

/**
 * @brief Callback function for PC9.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void PC9Callback(Fl_Widget *widget)
{
    int currValue = ((Fl_Round_Button *)widget)->value();
    if (currValue == 1)
    {
        writeSerialData(serialPort, 0x06);
        Controls.c_15v = true;
        writeToLog(Controls);
    }
    else
    {
        writeSerialData(serialPort, 0x19);
        Controls.c_15v = false;
        writeToLog(Controls);
    }
}

/**
 * @brief Callback function for PC6.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void PC6Callback(Fl_Widget *widget)
{
    int currValue = ((Fl_Round_Button *)widget)->value();
    if (currValue == 1)
    {
        writeSerialData(serialPort, 0x07);
        Controls.c_n3v3 = true;
        writeToLog(Controls);
    }
    else
    {
        writeSerialData(serialPort, 0x1A);
        Controls.c_n3v3 = false;
        writeToLog(Controls);
    }
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
    group6 = new Fl_Box(xGUIOffset + 285, yGUIOffset + 75, 130, 400, "GUI");
    group4 = new Fl_Box(xControlOffset + 15, yControlOffset + 75, 130, 400, "CONTROLS");
    group2 = new Fl_Box(xPacketOffset + 295, yPacketOffset, 200, 400, "ERPA PACKET");
    group1 = new Fl_Box(xPacketOffset + 15, yPacketOffset, 200, 400, "PMT PACKET");
    group3 = new Fl_Box(xPacketOffset + 575, yPacketOffset, 200, 400, "HK PACKET");
    ERPA1 = new Fl_Box(xPacketOffset + 300, yPacketOffset + 5, 50, 20, "SYNC:");
    ERPA2 = new Fl_Box(xPacketOffset + 300, yPacketOffset + 25, 50, 20, "SEQ:");
    ERPA4 = new Fl_Box(xPacketOffset + 300, yPacketOffset + 45, 50, 20, "SWP MON:");
    ERPA5 = new Fl_Box(xPacketOffset + 300, yPacketOffset + 65, 50, 20, "TEMP1:");
    ERPA3 = new Fl_Box(xPacketOffset + 300, yPacketOffset + 85, 50, 20, "ADC:");
    PMT1 = new Fl_Box(xPacketOffset + 18, yPacketOffset + 5, 50, 20, "SYNC:");
    PMT2 = new Fl_Box(xPacketOffset + 18, yPacketOffset + 25, 50, 20, "SEQ:");
    PMT3 = new Fl_Box(xPacketOffset + 18, yPacketOffset + 45, 50, 20, "ADC:");
    HK1 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 5, 50, 20, "SYNC:");
    HK2 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 25, 50, 20, "SEQ:");
    HK14 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 45, 50, 20, "vsense:");
    HK15 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 65, 50, 20, "vrefint:");
    tempLabel1 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 85, 50, 20, "TMP1:");
    tempLabel2 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 105, 50, 20, "TMP2:");
    tempLabel3 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 125, 50, 20, "TMP3:");
    tempLabel4 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 145, 50, 20, "TMP4:");
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
    syncWithInstruments = new Fl_Button(xGUIOffset + 295, yGUIOffset + 90, 110, 53, "Sync");
    autoStartUp = new Fl_Button(xGUIOffset + 295, yGUIOffset + 143, 110, 53, "Auto Init");
    autoShutDown = new Fl_Button(xGUIOffset + 295, yGUIOffset + 196, 110, 53, "Auto DeInit");
    enterStopMode = new Fl_Button(xGUIOffset + 295, yGUIOffset + 249, 110, 53, "Sleep");
    exitStopMode = new Fl_Button(xGUIOffset + 295, yGUIOffset + 302, 110, 53, "Wake Up");
    startRecording = new Fl_Button(xGUIOffset + 295, yGUIOffset + 355, 110, 53, "RECORD @circle");
    quit = new Fl_Button(xGUIOffset + 295, yGUIOffset + 408, 110, 53, "Quit");
    stepUp = new Fl_Button(xPacketOffset + 305, yPacketOffset + 195, 180, 20, "Step Up");
    stepDown = new Fl_Button(xPacketOffset + 305, yPacketOffset + 245, 180, 20, "Step Down");
    increaseFactor = new Fl_Button(xPacketOffset + 305, yPacketOffset + 305, 180, 20, "Factor Up");
    decreaseFactor = new Fl_Button(xPacketOffset + 305, yPacketOffset + 355, 180, 20, "Factor Down");
    PMTOn = new Fl_Round_Button(xPacketOffset + 165, yPacketOffset - 18, 20, 20);
    ERPAOn = new Fl_Round_Button(xPacketOffset + 450, yPacketOffset - 18, 20, 20);
    HKOn = new Fl_Round_Button(xPacketOffset + 725, yPacketOffset - 18, 20, 20);
    PB5 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 80, 100, 50, "sys_on PB5");
    PC7 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 130, 100, 50, "5v_en PC7");
    PC10 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 180, 100, 50, "3v3_en PC10");
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
    ERPAtemp1 = new Fl_Output(xPacketOffset + 417, yPacketOffset + 65, 60, 20);
    ERPAadc = new Fl_Output(xPacketOffset + 417, yPacketOffset + 85, 60, 20);
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
    SDN1 = new Fl_Light_Button(xPacketOffset + 305, yPacketOffset + 105, 150, 35, "  SDN1 High");
    autoSweep = new Fl_Light_Button(xPacketOffset + 305, yPacketOffset + 155, 150, 35, "  Auto Sweep");
    guiVersion = new Fl_Output(5, 575, 100, 20);
    instrumentVersion = new Fl_Output(110, 575, 100, 20);
    dateTime = new Fl_Output(215, 575, 200, 20);

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
    ERPAtemp1->color(box);
    ERPAtemp1->value(0);
    ERPAtemp1->box(FL_FLAT_BOX);
    ERPAtemp1->textcolor(output);
    ERPA5->box(FL_FLAT_BOX);
    ERPA5->color(box);
    ERPA5->labelfont();
    ERPA5->labelcolor(text);
    ERPA5->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
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
    increaseFactor->deactivate();
    decreaseFactor->deactivate();
    startRecording->deactivate();
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

    // ******************************************************************************************************************* Event Loop
    while (1)
    {
        snprintf(buffer, sizeof(buffer), "%d", currentFactor);
        curFactor->value(buffer);
        snprintf(buffer, sizeof(buffer), "%d", step);
        currStep->value(buffer);
        snprintf(buffer, sizeof(buffer), "%f", stepVoltages[step]);
        stepVoltage->value(buffer);

        outputFile.flush();
        vector<string> strings = interpret("mylog.0");
        if (!strings.empty())
        {
            truncate("mylog.0", 0);
            string pmtDate = "";
            string erpaDate = "";
            string hkDate = "";

            for (int i = 0; i < strings.size(); i++)
            {
                char letter = strings[i][0];
                strings[i] = strings[i].substr(2);
                if (ERPAOn->value())
                {
                    switch (letter)
                    {
                    case 'a':
                    {
                        if (recording)
                        {
                            writeToLog(ERPA, erpaFrame);
                        }
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        ERPAsync->value(buffer);
                        string logMsg(buffer);
                        erpaFrame[0] = logMsg;
                        break;
                    }
                    case 'b':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        ERPAseq->value(buffer);
                        string logMsg(buffer);
                        erpaFrame[1] = logMsg;
                        break;
                    }
                    case 'd':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        ERPAswp->value(buffer);
                        string logMsg(buffer);
                        erpaFrame[3] = logMsg;
                        break;
                    }
                    case 'e':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        ERPAtemp1->value(buffer);
                        string logMsg(buffer);
                        erpaFrame[4] = logMsg;
                        break;
                    }
                    case 'g':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        ERPAadc->value(buffer);
                        string logMsg(buffer);
                        erpaFrame[2] = logMsg;
                        break;
                    }
                    case '1':
                    {
                        erpaDate = strings[i];
                        break;
                    }
                    case '2':
                    {
                        erpaDate += "-" + strings[i];
                        break;
                    }
                    case '3':
                    {
                        erpaDate += "-" + strings[i];
                        break;
                    }
                    case '4':
                    {
                        erpaDate += "T" + strings[i];
                        break;
                    }
                    case '5':
                    {
                        erpaDate += ":" + strings[i];
                        break;
                    }
                    case '6':
                    {
                        erpaDate += ":" + strings[i];
                        break;
                    }
                    case '7':
                    {
                        // Millis MSB
                        break;
                    }
                    case '8':
                    {
                        // Millis MSB & LSB
                        erpaDate += "." + strings[i] + "Z";
                        dateTime->value(erpaDate.c_str());
                        erpaFrame[5] = erpaDate;
                        break;
                    }
                    }
                }
                if (PMTOn->value())
                {
                    switch (letter)
                    {
                    case 'i':
                    {
                        if (recording)
                        {
                            writeToLog(PMT, pmtFrame);
                        }
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        PMTsync->value(buffer);
                        string logMsg(buffer);
                        pmtFrame[0] = logMsg;
                        break;
                    }
                    case 'j':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        PMTseq->value(buffer);
                        string logMsg(buffer);
                        pmtFrame[1] = logMsg;
                        break;
                    }
                    case 'k':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        PMTadc->value(buffer);
                        string logMsg(buffer);
                        pmtFrame[2] = logMsg;
                        break;
                    }
                    case '1':
                    {
                        pmtDate = strings[i];
                        break;
                    }
                    case '2':
                    {
                        pmtDate += "-" + strings[i];
                        break;
                    }
                    case '3':
                    {
                        pmtDate += "-" + strings[i];
                        break;
                    }
                    case '4':
                    {
                        pmtDate += "T" + strings[i];
                        break;
                    }
                    case '5':
                    {
                        pmtDate += ":" + strings[i];
                        break;
                    }
                    case '6':
                    {
                        pmtDate += ":" + strings[i];
                        break;
                    }
                    case '7':
                    {
                        // Millis MSB
                        break;
                    }
                    case '8':
                    {
                        // Millis MSB & LSB
                        pmtDate += "." + strings[i] + "Z";
                        dateTime->value(pmtDate.c_str());
                        pmtFrame[3] = pmtDate;
                        break;
                    }
                    }
                }
                if (HKOn->value())
                {
                    switch (letter)
                    {
                    case 'l':
                    {
                        if (recording)
                        {
                            writeToLog(HK, hkFrame);
                        }
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HKsync->value(buffer);
                        string logMsg(buffer);
                        hkFrame[0] = logMsg;
                        break;
                    }
                    case 'm':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HKseq->value(buffer);
                        string logMsg(buffer);
                        hkFrame[1] = logMsg;
                        break;
                    }
                    case 'n':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HKvsense->value(buffer);
                        string logMsg(buffer);
                        hkFrame[13] = logMsg;
                        break;
                    }
                    case 'o':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HKvrefint->value(buffer);
                        string logMsg(buffer);
                        hkFrame[14] = logMsg;
                        break;
                    }
                    case 'p':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HKtemp1->value(buffer);
                        string logMsg(buffer);
                        hkFrame[15] = logMsg;
                        break;
                    }
                    case 'q':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HKtemp2->value(buffer);
                        string logMsg(buffer);
                        hkFrame[16] = logMsg;
                        break;
                    }
                    case 'r':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HKtemp3->value(buffer);
                        string logMsg(buffer);
                        hkFrame[17] = logMsg;
                        break;
                    }
                    case 's':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HKtemp4->value(buffer);
                        string logMsg(buffer);
                        hkFrame[18] = logMsg;
                        break;
                    }
                    case 't':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HKbusvmon->value(buffer);
                        string logMsg(buffer);
                        hkFrame[2] = logMsg;
                        break;
                    }
                    case 'u':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HKbusimon->value(buffer);
                        string logMsg(buffer);
                        hkFrame[3] = logMsg;
                        break;
                    }
                    case 'v':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HK2v5mon->value(buffer);
                        string logMsg(buffer);
                        hkFrame[7] = logMsg;
                        break;
                    }
                    case 'w':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HK3v3mon->value(buffer);
                        string logMsg(buffer);
                        hkFrame[4] = logMsg;
                        break;
                    }
                    case 'x':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HK5vmon->value(buffer);
                        string logMsg(buffer);
                        hkFrame[9] = logMsg;
                        break;
                    }
                    case 'y':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HKn3v3mon->value(buffer);
                        string logMsg(buffer);
                        hkFrame[10] = logMsg;
                        break;
                    }
                    case 'z':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HKn5vmon->value(buffer);
                        string logMsg(buffer);
                        hkFrame[8] = logMsg;
                        break;
                    }
                    case 'A':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HK15vmon->value(buffer);
                        string logMsg(buffer);
                        hkFrame[12] = logMsg;
                        break;
                    }
                    case 'B':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HK5vrefmon->value(buffer);
                        string logMsg(buffer);
                        hkFrame[11] = logMsg;
                        break;
                    }
                    case 'C':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HKn150vmon->value(buffer);
                        string logMsg(buffer);
                        hkFrame[5] = logMsg;
                        break;
                    }
                    case 'D':
                    {
                        snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                        HKn800vmon->value(buffer);
                        string logMsg(buffer);
                        hkFrame[6] = logMsg;
                        break;
                    }
                    case '1':
                    {
                        hkDate = strings[i];
                        break;
                    }
                    case '2':
                    {
                        hkDate += "-" + strings[i];
                        break;
                    }
                    case '3':
                    {
                        hkDate += "-" + strings[i];
                        break;
                    }
                    case '4':
                    {
                        hkDate += "T" + strings[i];
                        break;
                    }
                    case '5':
                    {
                        hkDate += ":" + strings[i];
                        break;
                    }
                    case '6':
                    {
                        hkDate += ":" + strings[i];
                        break;
                    }
                    case '7':
                    {
                        // Millis MSB
                        break;
                    }
                    case '8':
                    {
                        // Millis MSB & LSB
                        hkDate += "." + strings[i] + "Z";
                        dateTime->value(hkDate.c_str());
                        hkFrame[19] = hkDate;
                        break;
                    }
                    }
                }
            }
        }
        window->redraw(); // Refreshing main window with new data every loop
        Fl::check();
    }
    return Fl::run();
}
