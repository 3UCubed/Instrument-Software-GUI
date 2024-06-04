/**
 * @file
 * @author Jared Morrison, Jared King, Shane Woods
 * @version 1.0
 * @section DESCRIPTION
 *
 * GUI that connects to H7-Instrument-Software and shows packet data in real time
 */

// ******************************************************************************************************************* INCLUDES
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Value_Input.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Round_Button.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Light_Button.H>
#include <iomanip>
#include <string>
#include <iostream>
#include <fstream>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include <chrono>
#include <mutex>
#include <sstream>
#include "interpreter.cpp"
#include "logger.cpp"

// ******************************************************************************************************************* DEFINES

// ******************************************************************************************************************* GLOBALS
Fl_Output *instrumentVersionPtr;
const char *GUI_VERSION_NUM = "G-1.0.0-beta";
bool synced = false;
bool PB5IsOn = false;
const char *portName = "/dev/cu.usbserial-FT6DXVWX"; // CHANGE TO YOUR PORT NAME
int currentFactor = 1;
char currentFactorBuf[8];
int serialPort = open(portName, O_RDWR | O_NOCTTY); // Opening serial port
int step = 0;
string erpaFrame[6];
string pmtFrame[4];
string hkFrame[20];
using namespace std;
bool recording = false;
bool autoSweepStarted = false;
bool steppingUp = true;

// ******************************************************************************************************************* HELPER FUNCTIONS
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
 * @param stopFlag Atomic boolean flag indicating whether to stop reading.
 * @param outputFile The output file stream where the data will be written.
 */
void readSerialData(const int &serialPort, std::atomic<bool> &stopFlag, std::ofstream &outputFile)
{
    const int bufferSize = 64;
    char buffer[bufferSize + 1];
    int flushInterval = 1000; // Flush the file every 1000 bytes
    int bytesReadTotal = 0;

    while (!stopFlag)
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

// ******************************************************************************************************************* CALLBACKS

void syncCallback(Fl_Widget *)
{
    uint8_t rx_buffer[5];
    uint8_t tx_buffer[9];
    uint8_t key = 0x00;
    int bytesRead = 0;
    int tries = 0;

    // Set first byte of tx_buffer to 0xFF and fill rest with timestamp info
    tx_buffer[0] = 0xFF;
    generateTimestamp(tx_buffer);
    cout << "Generated Timestamp:\n";
    for (int i = 0; i < 9; i++)
    {
        cout << static_cast<int>(tx_buffer[i]) << endl;
    }

    // Continually send tx_buffer until we have received 0xFA (along with version #) from MCU
    do
    {
        write(serialPort, tx_buffer, 9 * sizeof(uint8_t));
        bytesRead = read(serialPort, rx_buffer, 5 * sizeof(uint8_t));
        if (bytesRead > 0)
        {
            key = rx_buffer[0];
        }
        tries++;
    } while (key != 0xFA && tries < 10);

    if (tries >= 9)
    {
        cerr << "Too many tries" << tries << endl;
        synced = false;
        return;
    }

    // [0]    [1]    [2]    [3]    [4]
    // 0xFA     1      0      0      2   = I-1.0.0-beta
    cout << "ACK Received\n";
    cout << "Received Version #:\n";
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
    instrumentVersionPtr->value(versionNum.c_str());
    synced = true;
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
        ((Fl_Button *)widget)->label("RECORDING @square");
        createNewLogs();
    }
    else
    {
        recording = false;
        ((Fl_Button *)widget)->label("RECORD @circle");
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
 * @brief Callback function for PB5.
 *
 * @param widget Pointer to the Fl_Widget triggering the callback.
 */
void PB5Callback(Fl_Widget *widget)
{
    PB5IsOn = ((Fl_Round_Button *)widget)->value();
    if (PB5IsOn)
    {
        writeSerialData(serialPort, 0x00);
        Controls.c_sysOn = true;
        writeToLog(Controls);
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
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return An integer representing the exit status of the program.
 */
int main(int argc, char **argv)
{
    // ******************************************************************************************************************* GUI Widget Setup
    // GUI variables
    const int windowWidth = 1175;
    const int windowHeight = 600;
    const int xPacketOffset = 380;
    const int yPacketOffset = 75;
    const int xControlOffset = 0;
    const int yControlOffset = 0;
    const int xGUIOffset = -120;
    const int yGUIOffset = 0;

    const string versionNumber = "1.0.0-beta";

    // GUI widgets
    Fl_Color darkBackground = fl_rgb_color(28, 28, 30);
    Fl_Color text = fl_rgb_color(203, 207, 213);
    Fl_Color box = fl_rgb_color(46, 47, 56);
    Fl_Color output = fl_rgb_color(60, 116, 239);
    Fl_Color white = fl_rgb_color(255, 255, 255);
    Fl_Window *window = new Fl_Window(windowWidth, windowHeight, "IS Packet Interpreter");
    Fl_Box *group6 = new Fl_Box(xGUIOffset + 285, yGUIOffset + 75, 130, 400, "GUI");
    Fl_Box *group4 = new Fl_Box(xControlOffset + 15, yControlOffset + 75, 130, 400, "CONTROLS");
    Fl_Box *group2 = new Fl_Box(xPacketOffset + 295, yPacketOffset, 200, 400, "ERPA PACKET");
    Fl_Box *group1 = new Fl_Box(xPacketOffset + 15, yPacketOffset, 200, 400, "PMT PACKET");
    Fl_Box *group3 = new Fl_Box(xPacketOffset + 575, yPacketOffset, 200, 400, "HK PACKET");
    Fl_Box *ERPA1 = new Fl_Box(xPacketOffset + 300, yPacketOffset + 5, 50, 20, "SYNC:");
    Fl_Box *ERPA2 = new Fl_Box(xPacketOffset + 300, yPacketOffset + 25, 50, 20, "SEQ:");
    Fl_Box *ERPA4 = new Fl_Box(xPacketOffset + 300, yPacketOffset + 45, 50, 20, "SWP MON:");
    Fl_Box *ERPA5 = new Fl_Box(xPacketOffset + 300, yPacketOffset + 65, 50, 20, "TEMP1:");
    Fl_Box *ERPA3 = new Fl_Box(xPacketOffset + 300, yPacketOffset + 85, 50, 20, "ADC:");
    Fl_Box *PMT1 = new Fl_Box(xPacketOffset + 18, yPacketOffset + 5, 50, 20, "SYNC:");
    Fl_Box *PMT2 = new Fl_Box(xPacketOffset + 18, yPacketOffset + 25, 50, 20, "SEQ:");
    Fl_Box *PMT3 = new Fl_Box(xPacketOffset + 18, yPacketOffset + 45, 50, 20, "ADC:");
    Fl_Box *HK1 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 5, 50, 20, "SYNC:");
    Fl_Box *HK2 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 25, 50, 20, "SEQ:");
    Fl_Box *HK14 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 45, 50, 20, "vsense:");
    Fl_Box *HK15 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 65, 50, 20, "vrefint:");
    Fl_Box *tempLabel1 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 85, 50, 20, "TMP1:");
    Fl_Box *tempLabel2 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 105, 50, 20, "TMP2:");
    Fl_Box *tempLabel3 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 125, 50, 20, "TMP3:");
    Fl_Box *tempLabel4 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 145, 50, 20, "TMP4:");
    Fl_Box *HK3 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 165, 50, 20, "BUSvmon:");
    Fl_Box *HK4 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 185, 50, 20, "BUSimon:");
    Fl_Box *HK8 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 205, 50, 20, "2v5mon:");
    Fl_Box *HK5 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 225, 50, 20, "3v3mon:");
    Fl_Box *HK10 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 245, 50, 20, "5vmon:");
    Fl_Box *HK11 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 265, 50, 20, "n3v3mon:");
    Fl_Box *HK9 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 285, 50, 20, "n5vmon:");
    Fl_Box *HK13 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 305, 50, 20, "15vmon:");
    Fl_Box *HK12 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 325, 50, 20, "5vrefmon:");
    Fl_Box *HK6 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 345, 50, 20, "n200vmon:");
    Fl_Box *HK7 = new Fl_Box(xPacketOffset + 580, yPacketOffset + 365, 50, 20, "n800vmon:");
    Fl_Button *quit = new Fl_Button(xGUIOffset + 295, yGUIOffset + 386, 110, 74, "Quit");
    Fl_Button *sync = new Fl_Button(xGUIOffset + 295, yGUIOffset + 90, 110, 74, "Sync");
    Fl_Button *stepUp = new Fl_Button(xPacketOffset + 305, yPacketOffset + 195, 180, 20, "Step Up");
    Fl_Button *stepDown = new Fl_Button(xPacketOffset + 305, yPacketOffset + 245, 180, 20, "Step Down");
    Fl_Button *enterStopMode = new Fl_Button(xGUIOffset + 295, yGUIOffset + 164, 110, 74, "Sleep");
    Fl_Button *exitStopMode = new Fl_Button(xGUIOffset + 295, yGUIOffset + 238, 110, 74, "Wake Up");
    Fl_Button *increaseFactor = new Fl_Button(xPacketOffset + 305, yPacketOffset + 305, 180, 20, "Factor Up");
    Fl_Button *decreaseFactor = new Fl_Button(xPacketOffset + 305, yPacketOffset + 355, 180, 20, "Factor Down");
    Fl_Button *startRecording = new Fl_Button(xGUIOffset + 295, yGUIOffset + 312, 110, 74, "RECORD @circle");
    Fl_Round_Button *PMTOn = new Fl_Round_Button(xPacketOffset + 165, yPacketOffset - 18, 20, 20);
    Fl_Round_Button *ERPAOn = new Fl_Round_Button(xPacketOffset + 450, yPacketOffset - 18, 20, 20);
    Fl_Round_Button *HKOn = new Fl_Round_Button(xPacketOffset + 725, yPacketOffset - 18, 20, 20);
    Fl_Round_Button *PB5 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 80, 100, 50, "sys_on PB5");
    Fl_Round_Button *PC7 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 130, 100, 50, "3v3_en PC7");
    Fl_Round_Button *PC10 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 180, 100, 50, "5v_en PC10");
    Fl_Round_Button *PC6 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 230, 100, 50, "n3v3_en PC6");
    Fl_Round_Button *PC8 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 280, 100, 50, "n5v_en PC8");
    Fl_Round_Button *PC9 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 330, 100, 50, "15v_en PC9");
    Fl_Round_Button *PC13 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 380, 100, 50, "n200v_en PC13");
    Fl_Round_Button *PB6 = new Fl_Round_Button(xControlOffset + 20, yControlOffset + 430, 100, 50, "800v_en PB6");
    Fl_Output *curFactor = new Fl_Output(xPacketOffset + 385, yPacketOffset + 330, 20, 20);
    Fl_Output *currStep = new Fl_Output(xPacketOffset + 355, yPacketOffset + 220, 20, 20);
    Fl_Output *stepVoltage = new Fl_Output(xPacketOffset + 400, yPacketOffset + 220, 20, 20);
    Fl_Output *ERPAsync = new Fl_Output(xPacketOffset + 417, yPacketOffset + 5, 60, 20);
    Fl_Output *ERPAseq = new Fl_Output(xPacketOffset + 417, yPacketOffset + 25, 60, 20);
    Fl_Output *ERPAswp = new Fl_Output(xPacketOffset + 417, yPacketOffset + 45, 60, 20);
    Fl_Output *ERPAtemp1 = new Fl_Output(xPacketOffset + 417, yPacketOffset + 65, 60, 20);
    Fl_Output *ERPAadc = new Fl_Output(xPacketOffset + 417, yPacketOffset + 85, 60, 20);
    Fl_Output *PMTsync = new Fl_Output(xPacketOffset + 135, yPacketOffset + 5, 60, 20);
    Fl_Output *PMTseq = new Fl_Output(xPacketOffset + 135, yPacketOffset + 25, 60, 20);
    Fl_Output *PMTadc = new Fl_Output(xPacketOffset + 135, yPacketOffset + 45, 60, 20);
    Fl_Output *HKsync = new Fl_Output(xPacketOffset + 682, yPacketOffset + 5, 60, 20);
    Fl_Output *HKseq = new Fl_Output(xPacketOffset + 682, yPacketOffset + 25, 60, 20);
    Fl_Output *HKvsense = new Fl_Output(xPacketOffset + 682, yPacketOffset + 45, 60, 20);
    Fl_Output *HKvrefint = new Fl_Output(xPacketOffset + 682, yPacketOffset + 65, 60, 20);
    Fl_Output *HKtemp1 = new Fl_Output(xPacketOffset + 682, yPacketOffset + 85, 60, 20);
    Fl_Output *HKtemp2 = new Fl_Output(xPacketOffset + 682, yPacketOffset + 105, 60, 20);
    Fl_Output *HKtemp3 = new Fl_Output(xPacketOffset + 682, yPacketOffset + 125, 60, 20);
    Fl_Output *HKtemp4 = new Fl_Output(xPacketOffset + 682, yPacketOffset + 145, 60, 20);
    Fl_Output *HKbusvmon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 165, 60, 20);
    Fl_Output *HKbusimon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 185, 60, 20);
    Fl_Output *HK2v5mon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 205, 60, 20);
    Fl_Output *HK3v3mon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 225, 60, 20);
    Fl_Output *HK5vmon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 245, 60, 20);
    Fl_Output *HKn3v3mon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 265, 60, 20);
    Fl_Output *HKn5vmon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 285, 60, 20);
    Fl_Output *HK15vmon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 305, 60, 20);
    Fl_Output *HK5vrefmon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 325, 60, 20);
    Fl_Output *HKn150vmon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 345, 60, 20);
    Fl_Output *HKn800vmon = new Fl_Output(xPacketOffset + 682, yPacketOffset + 365, 60, 20);
    Fl_Light_Button *SDN1 = new Fl_Light_Button(xPacketOffset + 305, yPacketOffset + 105, 150, 35, "  SDN1 High");
    Fl_Light_Button *autoSweep = new Fl_Light_Button(xPacketOffset + 305, yPacketOffset + 155, 150, 35, "  Auto Sweep");
    Fl_Output *guiVersion = new Fl_Output(5, 575, 100, 20);
    Fl_Output *instrumentVersion = new Fl_Output(110, 575, 100, 20);
    Fl_Output *dateTime = new Fl_Output(215, 575, 200, 20);

    // Main window styling
    window->color(darkBackground);
    dateTime->color(darkBackground);
    dateTime->value(0);
    dateTime->box(FL_FLAT_BOX);
    dateTime->textcolor(output);
    dateTime->labelsize(2);

    guiVersion->color(darkBackground);
    guiVersion->value(versionNumber.c_str());
    guiVersion->box(FL_FLAT_BOX);
    guiVersion->textcolor(output);
    guiVersion->labelsize(2);
    guiVersion->value(GUI_VERSION_NUM);

    instrumentVersion->color(darkBackground);
    instrumentVersion->value(versionNumber.c_str());
    instrumentVersion->box(FL_FLAT_BOX);
    instrumentVersion->textcolor(output);
    instrumentVersion->labelsize(2);
    instrumentVersion->value("I-x.y.z-n");
    instrumentVersionPtr = instrumentVersion;

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
    sync->align(FL_ALIGN_CENTER);
    sync->callback(syncCallback);
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

    // ******************************************************************************************************************* Thread Setup
    std::atomic<bool> stopFlag(false);
    std::ofstream outputFile("mylog.0", std::ios::out | std::ios::trunc);
    int serialPort = open(portName, O_RDWR | O_NOCTTY | O_NONBLOCK); // Opening serial port with non-blocking flag
    if (serialPort == -1)
    {
        std::cerr << "Failed to open the serial port." << std::endl;
        exit(1);
    }

    struct termios options;
    tcgetattr(serialPort, &options);

    options.c_cc[VMIN] = 0;   // Minimum number of characters for non-canonical read
    options.c_cc[VTIME] = 10; // Timeout in deciseconds (1 second)

    cfsetispeed(&options, 460800);
    cfsetospeed(&options, 460800);

    options.c_cflag &= ~PARENB; // No parity bit
    options.c_cflag &= ~CSTOPB; // 1 stop bit
    options.c_cflag &= ~CSIZE;  // Clear current char size mask
    options.c_cflag |= CS8;     // 8 data bits

    tcsetattr(serialPort, TCSANOW, &options); // Apply settings
    std::thread readingThread([&serialPort, &stopFlag, &outputFile]
                              { return readSerialData(serialPort, std::ref(stopFlag), std::ref(outputFile)); });

    // ******************************************************************************************************************* Pre-Startup Operations/Variables
    char buffer[32];
    float stepVoltages[8] = {0, 0.5, 1, 1.5, 2, 2.5, 3, 3.3};

    PB6->deactivate();
    PC10->deactivate();
    PC13->deactivate();
    PC7->deactivate();
    PC8->deactivate();
    PC9->deactivate();
    PC6->deactivate();
    ERPAOn->value(0);
    HKOn->value(0);

    window->show();
    Fl::check();

    // ******************************************************************************************************************* Event Loop
    while (1)
    {
        if (synced)
        {
            sync->deactivate();
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

            snprintf(buffer, sizeof(buffer), "%d", currentFactor);
            curFactor->value(buffer);
            snprintf(buffer, sizeof(buffer), "%d", step);
            currStep->value(buffer);
            snprintf(buffer, sizeof(buffer), "%f", stepVoltages[step]);
            stepVoltage->value(buffer);

            if (PB5IsOn) // Making sure sys_on (PB5) is on before activating other GPIO buttons
            {
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
                    // cout << strings[i] << endl;
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
        }
        else
        {
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
        }

        window->redraw(); // Refreshing main window with new data every loop
        Fl::check();
    }

    // ------------------------ Cleanup ------------------------
    stopFlag = true;
    readingThread.join();
    outputFile << '\0';
    outputFile.flush();
    outputFile.close();
    close(serialPort);
    return Fl::run();
}
