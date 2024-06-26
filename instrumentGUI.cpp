// ------------------------- README -----------------------
// If you get error "Failed to open serial port" go to line 120

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Value_Input.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Round_Button.H>
#include <FL/Fl_Value_Slider.H>
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
#include "interpreter/interpreter.cpp"

#define ERPA_HEADER "date, time, sync, seq, endMon, SWPMON, temp1, temp2, adc"
#define PMT_HEADER "date, time, sync, seq, adc"
#define HK_HEADER "date, time, sync, seq, vsense, vrefint, temp1, temp2, temp3, temp4, busvmon, busimon, 2v5mov, 3v3mon, 5vmon, n3v3mon, n5vmon, 15vmon, 5refmon, n200vmon, n800vmon"

const char *portName = "/dev/cu.usbserial-FT6DXNPY"; // CHANGE TO YOUR PORT NAME
const float erpaBPS = 140.0;
const float hkBPS = 5.6;
const float pmtBPS = 48.0;
const float tempsBPS = 2.4;
float totalBPS = 0;
int currentFactor = 1;
char currentFactorBuf[8];
int serialPort = open(portName, O_RDWR | O_NOCTTY); // Opening serial port
int step = 0;
string pmtLabels[3] = {"PMT sync", "PMT seq ", "PMT adc "};
string erpaLabels[7] = {"ERPA sync", "ERPA seq", "ERPA endmon", "ERPA swp-mon", "ERPA temp1", "ERPA temp2", "ERPA adc"};
string hkLabels[19] = {"HK sync       ", "HK seq        ", "HK busvmon   ", "HK busimon    ", "HK 3v3mon     ", "HK n150vmon   ", "HK n800vmon   ", "HK 2v5mon     ", "HK n5vmon     ", "HK 5vmon      ", "HK n3v3mon    ", "HK 5vrefmon   ", "HK 15vmon     ", "HK vsense     ", "HK vrefint    ", "TMP 1         ", "TMP 2         ", "TMP 3         ", "TMP 4         "};
string erpaFrame[7];
string pmtFrame[3];
string hkFrame[19];
using namespace std;
const float tolerance = 0.01;
bool recording = false;
bool autoSweepStarted = false;
bool steppingUp = true;
string erpaLog = "";
string pmtLog = "";
string hkLog = "";
string controlsLog = "";
ofstream erpaStream;
ofstream pmtStream;
ofstream hkStream;
ofstream controlsStream;
// --------------------- Generate New Log Name -----------------
string newLogName()
{
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    ostringstream oss;
    oss << put_time(&tm, "%Y-%m-%d %H-%M-%S");
    return (oss.str());
}


// --------------------- Write to Event Log --------------------
void writeToErpaLog(string sync, string seq, string endMon, string SWPMON, string temp1, string temp2, string adc)
{
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);

    auto now = chrono::system_clock::now();
    time_t now_c = chrono::system_clock::to_time_t(now);
    auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
    erpaStream << put_time(&tm, "%m-%d-%Y, %H:%M:%S:") << ms.count() << ", " << sync << ", " << seq << ", " << endMon << ", " << SWPMON << ", " << temp1 << ", " << temp2 << ", " << adc << "\n";
}

void writeToPMTLog(string sync, string seq, string adc)
{
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);

    auto now = chrono::system_clock::now();
    time_t now_c = chrono::system_clock::to_time_t(now);
    auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
    pmtStream << put_time(&tm, "%m-%d-%Y, %H:%M:%S:") << ms.count() << ", " << sync << ", " << seq << ", " << adc << "\n";
}

void writeToHKLog(string sync, string seq, string vsense, string vrefint, string temp1, string temp2, string temp3, string temp4, string busvmon, string busimon, string mon2v5, string mon3v3, string vmon5, string n3v3mon, string n5vmon, string vmon15, string vrefmon5, string n200vmon, string n800vmon)
{
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);

    auto now = chrono::system_clock::now();
    time_t now_c = chrono::system_clock::to_time_t(now);
    auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
    hkStream << put_time(&tm, "%m-%d-%Y, %H:%M:%S:") << ms.count() << ", " << sync << ", " << seq << ", " << vsense << ", " << vrefint << ", " << temp1 << ", " << temp2 << ", " << temp3 << ", " << temp4 << ", " << busvmon << ", " << busimon << ", " << mon2v5 << ", " << mon3v3 << ", " << vmon5 << ", " << n3v3mon << ", " << n5vmon << ", " << vmon15 << ", " << vrefmon5 << ", " << n200vmon << ", " << n800vmon << "\n";
}

void writeToControlsLog(string pmt_on, string erpa_on, string hk_on, string c_sys_on, string c_800v_en, string c_5v_en, string c_n150v_en, string c_3v3_en, string c_n5v_en, string c_15v_en, string c_n3v3_en, string c_sdn1, string c_sdn2)
{
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);

    auto now = chrono::system_clock::now();
    time_t now_c = chrono::system_clock::to_time_t(now);
    auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
    controlsStream << put_time(&tm, "%m-%d-%Y, %H:%M:%S:") << ms.count() << ", " << pmt_on << ", " << erpa_on << ", " << hk_on << ", " << c_sys_on << ", " << c_800v_en << ", " << c_5v_en << ", " << c_n150v_en << ", " << c_3v3_en << ", " << c_n5v_en << ", " << c_15v_en << ", " << c_n3v3_en << ", " << c_sdn1 << ", " << c_sdn2 << "\n";
}

// ---------------- Start Recording button event ---------------
void startRecordingCallback(Fl_Widget *widget)
{

    if (!recording)
    {
        recording = true;
        ((Fl_Button *)widget)->label("RECORDING @square");
        erpaLog = "logs/ERPA/ERPA " + newLogName() + ".csv";
        pmtLog = "logs/PMT/PMT " + newLogName() + ".csv";
        hkLog = "logs/HK/HK " + newLogName() + ".csv";

        erpaStream.open(erpaLog, ios::app);
        pmtStream.open(pmtLog, ios::app);
        hkStream.open(hkLog, ios::app);

        //Write the headers
        erpaStream << ERPA_HEADER << "\n";
        pmtStream << PMT_HEADER << "\n";
        hkStream << HK_HEADER << "\n";
      }
    else
    {
        recording = false;
        ((Fl_Button *)widget)->label("RECORD @circle");
        erpaStream.close();
        pmtStream.close();
        hkStream.close();
    }
}
// --------------------- Quit button event ---------------------
void quitCallback(Fl_Widget *)
{
    controlsStream.close();
    exit(0);
}

// ----------------- Write data to serial port -----------------
void writeSerialData(const int &serialPort, const unsigned char data)
{
    const unsigned char * ptr = &data;
    ssize_t bytesWritten = write(serialPort, ptr, sizeof(unsigned char));
    if (bytesWritten == -1)
    {
        std::cerr << "Error writing to the serial port." << std::endl;
    }
}

// --------------------- Stop Callback -------------------------
void stopModeCallback(Fl_Widget *)
{
    writeSerialData(serialPort, 0x0C);
}

// ---------------------- Wake-Up Callback ---------------------
void exitStopModeCallback(Fl_Widget *)
{
    for (int i = 0; i < 12; i++)
    {
        writeSerialData(serialPort, 0x5B);
    }
}

// ------- Continuously reads data from the serial port --------
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
            //buffer[bytesRead] = '\0';
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


// ------------------- Step Up button event --------------------
void stepUpCallback(Fl_Widget *)
{
    writeSerialData(serialPort, 0x1B);
    if (step < 7)
    {
        step++;
    }
}

// ------------------- Step Down button event ------------------
void stepDownCallback(Fl_Widget *)
{
    writeSerialData(serialPort, 0x1C);
    if (step > 0)
    {
        step--;
    }
}

void factorUpCallback(Fl_Widget *) {
    writeSerialData(serialPort, 0x24);
    if (currentFactor < 32) {
        currentFactor *= 2;
    }
}

void factorDownCallback(Fl_Widget*) {
    writeSerialData(serialPort, 0x25);
    if (currentFactor > 1) {
        currentFactor /= 2;
    }
}

// ------------------- 100ms Timer Callback --------------------


// ------------------- Auto Sweep Callback --------------------
void autoSweepCallback(Fl_Widget *){
    writeSerialData(serialPort, 0x1D);
}



// ----------- Check If Value Is In Tolerance Range ------------
bool toleranceCheck(Fl_Output *widgetToCheck, Fl_Box *widgetToAlarm, float initialValue)
{ // Returns true if current value is outside of tolerance range, false otherwise
    float currVal = stof(widgetToCheck->value());
    float allowedTolerance = initialValue * tolerance;
    float min = initialValue - allowedTolerance;
    float max = initialValue + allowedTolerance;
    if (currVal > max)
    {
        widgetToAlarm->color(FL_RED);
        return true;
    }
    else if (currVal < min)
    {
        widgetToAlarm->color(FL_BLUE);
        return true;
    }
    else
    {
        Fl_Color box = fl_rgb_color(46, 47, 56);
        widgetToAlarm->color(box);
        return false;
    }
}

// ------ Check If Value Is In Tolerance Range Overloaded ------
bool toleranceCheck(Fl_Value_Slider *widgetToCheck, Fl_Box *widgetToAlarm, float initialValue)
{ // Function overloaded for temperature sliders
    float currVal = (float)widgetToCheck->value();
    float allowedTolerance = initialValue * tolerance;
    float min = initialValue - allowedTolerance;
    float max = initialValue + allowedTolerance;
    if (currVal > max)
    {
        widgetToCheck->color(FL_RED);
        return true;
    }
    else if (currVal < min)
    {
        widgetToCheck->color(FL_BLUE);
        return true;
    }
    else
    {
        Fl_Color box = fl_rgb_color(46, 47, 56);
        widgetToCheck->color(box);
        return false;
    }
}

/*
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
            break; // Use the first matching serial port found
        }
    }

    closedir(dir);
    return portName;
}
*/

// ------------------- Main Program Function -------------------
int main(int argc, char **argv)
{
    float stepVoltages[8] = {0, 0.5, 1, 1.5, 2, 2.5, 3, 3.3};
    // ------------------ Output Field Vars --------------------
    char buffer[32];
    float pmt_sync = 0;
    float pmt_seq = 0;
    float pmt_adc = 0;
    float erpa_sync = 0;
    float erpa_seq = 0;
    float erpa_adc = 0;
    float erpa_swp = 0;
    float erpa_temp1 = 0;
    float erpa_temp2 = 0;
    float erpa_endmon = 0;
    float hk_sync = 0;
    float hk_seq = 0;
    float hk_busvmon = 0;
    float hk_busimon = 0;
    float hk_3v3vmon = 0;
    float hk_n150vmon = 0;
    float hk_n800vmon = 0;
    float hk_2v5mon = 0;
    float hk_n5vmon = 0;
    float hk_5vmon = 0;
    float hk_n3v3mon = 0;
    float hk_5vrefmon = 0;
    float hk_15vmon = 0;
    float hk_vsense = 0;
    float hk_vrefint = 0;
    float hk_temp1 = 0;
    float hk_temp2 = 0;
    float hk_temp3 = 0;
    float hk_temp4 = 0;
    // resolve zero issue
    // averaging on on board MCU
    // timing for different packets
    // RTC
    // separate data into separate CSV's
    //
    // // sync, seq, endmon, swpmon, tmp1, tmp2,adc
    controlsLog = "logs/Controls/Controls" + newLogName() + ".csv";
    controlsStream.open(controlsLog, ios::app);
    writeToControlsLog("pmt_on", "erpa_on", "hk_on", "c_sys_on", "c_800v_en", "c_5v_en", "c_n150v_en", "c_3v3_en", "c_n5v_en", "c_15v_en", "c_n3v3_en", "c_sdn1", "c_sdn2");

    // --------- Vars Keeping Track Of Packet States -----------
    unsigned char valPMT;
    unsigned char valERPA;
    unsigned char valHK;
    int turnedOff = 0;

    // ------------------------ Thread Vars --------------------
    struct termios options = {};
    std::atomic<bool> stopFlag(false);

    // portName = findSerialPort();
    std::ofstream outputFile("mylog.0", std::ios::out | std::ios::trunc);

    // -------------------- Thread/Port Setup ------------------
    int serialPort = open(portName, O_RDWR | O_NOCTTY); // Opening serial port
    if (serialPort == -1)
    {
        std::cerr << "Failed to open the serial port." << std::endl;
        ::exit(0);
    }
    tcgetattr(serialPort, &options);
    cfsetispeed(&options, B57600);
    cfsetospeed(&options, B57600);
    options.c_cflag |= O_NONBLOCK;
    tcsetattr(serialPort, TCSANOW, &options);

    std::thread readingThread([&serialPort, &stopFlag, &outputFile]
                              { return readSerialData(serialPort, std::ref(stopFlag), std::ref(outputFile)); });

    // --------------- Main Window Elements Setup --------------
    int width = 1300; // Width and Height of Main Window
    int height = 800;
    int x_packet_offset = 320; // X and Y offsets for the three packet groups
    int y_packet_offset = 200;

    Fl_Window *window = new Fl_Window(width, height, "IS Packet Interpreter");
    Fl_Color darkBackground = fl_rgb_color(28, 28, 30);
    Fl_Color text = fl_rgb_color(203, 207, 213);
    Fl_Color box = fl_rgb_color(46, 47, 56);
    Fl_Color output = fl_rgb_color(60, 116, 239);
    Fl_Color white = fl_rgb_color(255, 255, 255);

    window->color(darkBackground);


    Fl_Button *quit = new Fl_Button(15, 10, 40, 40, "ⓧ");
    quit->box(FL_FLAT_BOX);
    quit->color(darkBackground);
    quit->labelcolor(FL_RED);
    quit->labelsize(40);
    quit->callback(quitCallback);

    Fl_Round_Button *PMT_ON = new Fl_Round_Button(x_packet_offset + 165, y_packet_offset - 18, 20, 20);
    Fl_Round_Button *ERPA_ON = new Fl_Round_Button(x_packet_offset + 450, y_packet_offset - 18, 20, 20);
    Fl_Round_Button *HK_ON = new Fl_Round_Button(x_packet_offset + 725, y_packet_offset - 18, 20, 20);

    // --------------------- CONTROLS GROUP --------------------
    Fl_Box *group4 = new Fl_Box(15, 75, 130, 700, "CONTROLS");
    group4->color(box);
    group4->box(FL_BORDER_BOX);
    group4->labelcolor(text);
    group4->labelfont(FL_BOLD);
    group4->align(FL_ALIGN_TOP);

    Fl_Round_Button *PB5 = new Fl_Round_Button(20, 80, 100, 50, "sys_on PB5");
    Fl_Round_Button *PC7 = new Fl_Round_Button(20, 130, 100, 50, "3v3_en PC7");
    Fl_Round_Button *PC10 = new Fl_Round_Button(20, 180, 100, 50, "5v_en PC10");
    Fl_Round_Button *PC6 = new Fl_Round_Button(20, 230, 100, 50, "n3v3_en PC6");
    Fl_Round_Button *PC8 = new Fl_Round_Button(20, 280, 100, 50, "n5v_en PC8");
    Fl_Round_Button *PC9 = new Fl_Round_Button(20, 330, 100, 50, "15v_en PC9");
    Fl_Round_Button *PC13 = new Fl_Round_Button(20, 380, 100, 50, "n200v_en PC13");
    Fl_Round_Button *PB6 = new Fl_Round_Button(20, 430, 100, 50, "800v_en PB6");
    
    Fl_Button *autoSweep = new Fl_Button(25, 475, 110, 25, "Auto Sweep");
    Fl_Button *stepUp = new Fl_Button(25, 510, 110, 25, "Step Up");
    Fl_Button *stepDown = new Fl_Button(25, 565, 110, 25, "Step Down");
    Fl_Button *enterStopMode = new Fl_Button(25, 610, 110, 35, "Sleep");
    Fl_Button *exitStopMode = new Fl_Button(25, 660, 110, 35, "Wake Up");

    Fl_Button *increaseFactor = new Fl_Button(300, 75, 110, 25, "Factor Up");
    increaseFactor->callback(factorUpCallback);
    Fl_Button *decreaseFactor = new Fl_Button(300, 115, 110, 25, "Factor Down");
    decreaseFactor->callback(factorDownCallback);
    Fl_Output *curFactor = new Fl_Output(300, 155, 110, 25);
    curFactor->color(box);
    curFactor->value(buffer);
    curFactor->box(FL_FLAT_BOX);
    curFactor->textcolor(output);


    Fl_Button *startRecording = new Fl_Button(25, 720, 110, 35, "RECORD @circle");
    startRecording->labelcolor(FL_RED);
    startRecording->callback(startRecordingCallback);

    autoSweep->callback(autoSweepCallback);
    stepDown->callback(stepDownCallback);
    stepUp->callback(stepUpCallback);
    stepUp->label("Step Up      @8->");
    stepUp->align(FL_ALIGN_CENTER);
    stepDown->label("Step Down  @2->");
    stepDown->align(FL_ALIGN_CENTER);
    Fl_Output *currStep = new Fl_Output(112, 540, 20, 20);
    Fl_Output *stepVoltage = new Fl_Output(40, 540, 20, 20);

    enterStopMode->callback(stopModeCallback);
    exitStopMode->callback(exitStopModeCallback);

    currStep->color(box);
    currStep->value(buffer);
    currStep->box(FL_FLAT_BOX);
    currStep->textcolor(output);

    stepVoltage->color(box);
    stepVoltage->value(buffer);
    stepVoltage->box(FL_FLAT_BOX);
    stepVoltage->textcolor(output);

    PB5->labelcolor(text);
    PB6->labelcolor(text);
    PC10->labelcolor(text);
    PC13->labelcolor(text);
    PC7->labelcolor(text);
    PC8->labelcolor(text);
    PC9->labelcolor(text);
    PC6->labelcolor(text);
    PB6->deactivate();
    PC10->deactivate();
    PC13->deactivate();
    PC7->deactivate();
    PC8->deactivate();
    PC9->deactivate();
    PC6->deactivate();
    PMT_ON->value(0);
    ERPA_ON->value(0);
    HK_ON->value(0);
    unsigned char valPB6 = PB6->value();
    unsigned char valPB5 = PB5->value();
    unsigned char valPC10 = PC10->value();
    unsigned char valPC13 = PC13->value();
    unsigned char valPC7 = PC7->value();
    unsigned char valPC8 = PC8->value();
    unsigned char valPC9 = PC9->value();
    unsigned char valPC6 = PC6->value();
    valPMT = PMT_ON->value();
    valERPA = ERPA_ON->value();
    valHK = HK_ON->value();
    turnedOff = 0;

    // -------------------- ERPA Packet Group ------------------
    Fl_Box *group2 = new Fl_Box(x_packet_offset + 295, y_packet_offset, 200, 400,
                                "ERPA PACKET");
    group2->color(box);
    group2->box(FL_BORDER_BOX);
    group2->labelcolor(text);
    group2->labelfont(FL_BOLD);
    group2->align(FL_ALIGN_TOP);

    Fl_Light_Button *SDN1 = new Fl_Light_Button(x_packet_offset + 305, y_packet_offset + 185, 150, 50, " SDN1 HIGH");
    SDN1->selection_color(FL_GREEN);
    SDN1->box(FL_FLAT_BOX);
    SDN1->color(box);
    SDN1->labelcolor(text);
    SDN1->labelsize(17);
    unsigned char valSDN1 = SDN1->value();

    Fl_Light_Button *SDN2 = new Fl_Light_Button(x_packet_offset + 305, y_packet_offset + 235, 150, 50, " SDN2 HIGH");
    SDN2->selection_color(FL_GREEN);
    SDN2->box(FL_FLAT_BOX);
    SDN2->color(box);
    SDN2->labelcolor(text);
    SDN2->labelsize(17);
    unsigned char valSDN2 = SDN2->value();

    Fl_Box *ERPA1 = new Fl_Box(x_packet_offset + 300, y_packet_offset + 5, 50, 20, "SYNC:");
    Fl_Output *ERPAsync = new Fl_Output(x_packet_offset + 417, y_packet_offset + 5, 60, 20);
    ERPAsync->color(box);
    snprintf(buffer, sizeof(buffer), "%f", erpa_sync);
    ERPAsync->value(buffer);
    ERPAsync->box(FL_FLAT_BOX);
    ERPAsync->textcolor(output);
    ERPA1->box(FL_FLAT_BOX);
    ERPA1->color(box);
    ERPA1->labelfont();
    ERPA1->labelcolor(text);
    ERPA1->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    Fl_Box *ERPA2 = new Fl_Box(x_packet_offset + 300, y_packet_offset + 25, 50, 20, "SEQ:");
    Fl_Output *ERPAseq = new Fl_Output(x_packet_offset + 417, y_packet_offset + 25, 60, 20);
    ERPAseq->color(box);
    snprintf(buffer, sizeof(buffer), "%f", erpa_seq);
    ERPAseq->value(buffer);
    ERPAseq->box(FL_FLAT_BOX);
    ERPAseq->textcolor(output);
    ERPA2->box(FL_FLAT_BOX);
    ERPA2->color(box);
    ERPA2->labelfont();
    ERPA2->labelcolor(text);
    ERPA2->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *ERPA7 = new Fl_Box(x_packet_offset + 300, y_packet_offset + 45, 50, 20, "ENDmon:");
    Fl_Output *ERPAendmon = new Fl_Output(x_packet_offset + 417, y_packet_offset + 45, 60, 20);
    ERPAendmon->color(box);
    snprintf(buffer, sizeof(buffer), "%f", erpa_endmon);
    ERPAendmon->value(buffer);
    ERPAendmon->box(FL_FLAT_BOX);
    ERPAendmon->textcolor(output);
    ERPA7->box(FL_FLAT_BOX);
    ERPA7->color(box);
    ERPA7->labelfont();
    ERPA7->labelcolor(text);
    ERPA7->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *ERPA4 = new Fl_Box(x_packet_offset + 300, y_packet_offset + 65, 50, 20, "SWP MON:");
    Fl_Output *ERPAswp = new Fl_Output(x_packet_offset + 417, y_packet_offset + 65, 60, 20);
    ERPAswp->color(box);
    snprintf(buffer, sizeof(buffer), "%f", erpa_swp);
    ERPAswp->value(buffer);
    ERPAswp->box(FL_FLAT_BOX);
    ERPAswp->textcolor(output);
    ERPA4->box(FL_FLAT_BOX);
    ERPA4->color(box);
    ERPA4->labelfont();
    ERPA4->labelcolor(text);
    ERPA4->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *ERPA5 = new Fl_Box(x_packet_offset + 300, y_packet_offset + 105, 50, 20, "TEMP1:");
    Fl_Output *ERPAtemp1 = new Fl_Output(x_packet_offset + 417, y_packet_offset + 105, 60, 20);
    ERPAtemp1->color(box);
    snprintf(buffer, sizeof(buffer), "%f", erpa_temp1);
    ERPAtemp1->value(buffer);
    ERPAtemp1->box(FL_FLAT_BOX);
    ERPAtemp1->textcolor(output);
    ERPA5->box(FL_FLAT_BOX);
    ERPA5->color(box);
    ERPA5->labelfont();
    ERPA5->labelcolor(text);
    ERPA5->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *ERPA6 = new Fl_Box(x_packet_offset + 300, y_packet_offset + 125, 50, 20, "TEMP2:");
    Fl_Output *ERPAtemp2 = new Fl_Output(x_packet_offset + 417, y_packet_offset + 125, 60, 20);
    ERPAtemp2->color(box);
    snprintf(buffer, sizeof(buffer), "%f", erpa_temp2);
    ERPAtemp2->value(buffer);
    ERPAtemp2->box(FL_FLAT_BOX);
    ERPAtemp2->textcolor(output);
    ERPA6->box(FL_FLAT_BOX);
    ERPA6->color(box);
    ERPA6->labelfont();
    ERPA6->labelcolor(text);
    ERPA6->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *ERPA3 = new Fl_Box(x_packet_offset + 300, y_packet_offset + 145, 50, 20, "ADC:");
    Fl_Output *ERPAadc = new Fl_Output(x_packet_offset + 417, y_packet_offset + 145, 60, 20);
    ERPAadc->color(box);
    snprintf(buffer, sizeof(buffer), "%f", erpa_adc);
    ERPAadc->value(buffer);
    ERPAadc->box(FL_FLAT_BOX);
    ERPAadc->textcolor(output);
    ERPA3->box(FL_FLAT_BOX);
    ERPA3->color(box);
    ERPA3->labelfont();
    ERPA3->labelcolor(text);
    ERPA3->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    // ------------------ PMT Packet Group ---------------------
    Fl_Group *group1 = new Fl_Group(x_packet_offset + 15, y_packet_offset, 200, 400,
                                    "PMT PACKET");
    group1->color(box);
    group1->box(FL_BORDER_BOX);
    group1->labelfont(FL_BOLD);
    group1->labelcolor(text);
    Fl_Box *PMT1 = new Fl_Box(x_packet_offset + 18, y_packet_offset + 5, 50, 20, "SYNC:");
    Fl_Output *PMTsync = new Fl_Output(x_packet_offset + 135, y_packet_offset + 5, 60, 20);
    PMTsync->color(box);
    snprintf(buffer, sizeof(buffer), "%f", pmt_sync);
    PMTsync->value(buffer);
    PMTsync->box(FL_FLAT_BOX);
    PMTsync->textcolor(output);
    PMT1->box(FL_FLAT_BOX);
    PMT1->color(box);
    PMT1->labelfont();
    PMT1->labelcolor(text);
    PMT1->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *PMT2 = new Fl_Box(x_packet_offset + 18, y_packet_offset + 25, 50, 20, "SEQ:");
    Fl_Output *PMTseq = new Fl_Output(x_packet_offset + 135, y_packet_offset + 25, 60, 20);
    PMTseq->color(box);
    snprintf(buffer, sizeof(buffer), "%f", pmt_seq);
    PMTseq->value(buffer);
    PMTseq->box(FL_FLAT_BOX);
    PMTseq->textcolor(output);
    PMT2->box(FL_FLAT_BOX);
    PMT2->color(box);
    PMT2->labelfont();
    PMT2->labelcolor(text);
    PMT2->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *PMT3 = new Fl_Box(x_packet_offset + 18, y_packet_offset + 45, 50, 20, "ADC:");
    Fl_Output *PMTadc = new Fl_Output(x_packet_offset + 135, y_packet_offset + 45, 60, 20);
    PMTadc->color(box);
    snprintf(buffer, sizeof(buffer), "%f", pmt_adc);
    PMTadc->value(buffer);
    PMTadc->box(FL_FLAT_BOX);
    PMTadc->textcolor(output);
    PMT3->box(FL_FLAT_BOX);
    PMT3->color(box);
    PMT3->labelfont();
    PMT3->labelcolor(text);
    PMT3->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    // -------------------- HK Packet Group --------------------
    Fl_Group *group3 = new Fl_Group(x_packet_offset + 575, y_packet_offset, 200, 400,
                                    "HK PACKET");
    group3->color(box);
    group3->box(FL_BORDER_BOX);
    group3->labelfont(FL_BOLD);
    group3->labelcolor(text);
    Fl_Box *HK1 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 5, 50, 20, "SYNC:");
    Fl_Output *HKsync = new Fl_Output(x_packet_offset + 682, y_packet_offset + 5, 60, 20);
    HKsync->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_sync);
    HKsync->value(buffer);
    HKsync->box(FL_FLAT_BOX);
    HKsync->textcolor(output);
    HK1->box(FL_FLAT_BOX);
    HK1->color(box);
    HK1->labelfont();
    HK1->labelcolor(text);
    HK1->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *HK2 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 25, 50, 20, "SEQ:");
    Fl_Output *HKseq = new Fl_Output(x_packet_offset + 682, y_packet_offset + 25, 60, 20);
    HKseq->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_seq);
    HKseq->value(buffer);
    HKseq->box(FL_FLAT_BOX);
    HKseq->textcolor(output);
    HK2->box(FL_FLAT_BOX);
    HK2->color(box);
    HK2->labelfont();
    HK2->labelcolor(text);
    HK2->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *HK14 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 45, 50, 20, "vsense:");
    Fl_Output *HKvsense = new Fl_Output(x_packet_offset + 682, y_packet_offset + 45, 60, 20);
    HKvsense->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_vsense);
    HKvsense->value(buffer);
    HKvsense->box(FL_FLAT_BOX);
    HKvsense->textcolor(output);
    HK14->box(FL_FLAT_BOX);
    HK14->color(box);
    HK14->labelfont();
    HK14->labelcolor(text);
    HK14->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *HK15 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 65, 50, 20, "vrefint:");
    Fl_Output *HKvrefint = new Fl_Output(x_packet_offset + 682, y_packet_offset + 65, 60, 20);
    HKvrefint->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_vrefint);
    HKvrefint->value(buffer);
    HKvrefint->box(FL_FLAT_BOX);
    HKvrefint->textcolor(output);
    HK15->box(FL_FLAT_BOX);
    HK15->color(box);
    HK15->labelfont();
    HK15->labelcolor(text);
    HK15->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    Fl_Box *tempLabel1 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 85, 50, 20, "TMP1:");
    Fl_Output *HKtemp1 = new Fl_Output(x_packet_offset + 682, y_packet_offset + 85, 60, 20);
    HKtemp1->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_temp1);
    HKtemp1->value(buffer);
    HKtemp1->box(FL_FLAT_BOX);
    HKtemp1->textcolor(output);
    tempLabel1->box(FL_FLAT_BOX);
    tempLabel1->color(box);
    tempLabel1->labelfont();
    tempLabel1->labelcolor(text);
    tempLabel1->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    Fl_Box *tempLabel2 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 105, 50, 20, "TMP2:");
    Fl_Output *HKtemp2 = new Fl_Output(x_packet_offset + 682, y_packet_offset + 105, 60, 20);
    HKtemp2->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_temp2);
    HKtemp2->value(buffer);
    HKtemp2->box(FL_FLAT_BOX);
    HKtemp2->textcolor(output);
    tempLabel2->color(box);
    tempLabel2->box(FL_FLAT_BOX);
    tempLabel2->labelfont();
    tempLabel2->labelcolor(text);
    tempLabel2->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    Fl_Box *tempLabel3 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 125, 50, 20, "TMP3:");
    Fl_Output *HKtemp3 = new Fl_Output(x_packet_offset + 682, y_packet_offset + 125, 60, 20);
    HKtemp3->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_temp3);
    HKtemp3->value(buffer);
    HKtemp3->box(FL_FLAT_BOX);
    HKtemp3->textcolor(output);
    tempLabel3->color(box);
    tempLabel3->box(FL_FLAT_BOX);
    tempLabel3->labelfont();
    tempLabel3->labelcolor(text);
    tempLabel3->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    Fl_Box *tempLabel4 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 145, 50, 20, "TMP4:");
    Fl_Output *HKtemp4 = new Fl_Output(x_packet_offset + 682, y_packet_offset + 145, 60, 20);
    HKtemp4->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_temp4);
    HKtemp4->value(buffer);
    HKtemp4->box(FL_FLAT_BOX);
    HKtemp4->textcolor(output);
    tempLabel4->color(box);
    tempLabel4->box(FL_FLAT_BOX);
    tempLabel4->labelfont();
    tempLabel4->labelcolor(text);
    tempLabel4->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *HK3 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 165, 50, 20, "BUSvmon:");
    Fl_Output *HKbusvmon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 165, 60, 20);
    HKbusvmon->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_busvmon);
    HKbusvmon->value(buffer);
    HKbusvmon->box(FL_FLAT_BOX);
    HKbusvmon->textcolor(output);
    HK3->box(FL_FLAT_BOX);
    HK3->color(box);
    HK3->labelfont();
    HK3->labelcolor(text);
    HK3->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *HK4 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 185, 50, 20, "BUSimon:");
    Fl_Output *HKbusimon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 185, 60, 20);
    HKbusimon->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_busimon);
    HKbusimon->value(buffer);
    HKbusimon->box(FL_FLAT_BOX);
    HKbusimon->textcolor(output);
    HK4->box(FL_FLAT_BOX);
    HK4->color(box);
    HK4->labelfont();
    HK4->labelcolor(text);
    HK4->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *HK8 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 205, 50, 20, "2v5mon:");
    Fl_Output *HK2v5mon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 205, 60, 20);
    HK2v5mon->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_2v5mon);
    HK2v5mon->value(buffer);
    HK2v5mon->box(FL_FLAT_BOX);
    HK2v5mon->textcolor(output);
    HK8->box(FL_FLAT_BOX);
    HK8->color(box);
    HK8->labelfont();
    HK8->labelcolor(text);
    HK8->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *HK5 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 225, 50, 20, "3v3mon:");
    Fl_Output *HK3v3mon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 225, 60, 20);
    HK3v3mon->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_n3v3mon);
    HK3v3mon->value(buffer);
    HK3v3mon->box(FL_FLAT_BOX);
    HK3v3mon->textcolor(output);
    HK5->box(FL_FLAT_BOX);
    HK5->color(box);
    HK5->labelfont();
    HK5->labelcolor(text);
    HK5->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *HK10 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 245, 50, 20, "5vmon:");
    Fl_Output *HK5vmon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 245, 60, 20);
    HK5vmon->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_5vmon);
    HK5vmon->value(buffer);
    HK5vmon->box(FL_FLAT_BOX);
    HK5vmon->textcolor(output);
    HK10->box(FL_FLAT_BOX);
    HK10->color(box);
    HK10->labelfont();
    HK10->labelcolor(text);
    HK10->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *HK11 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 265, 50, 20, "n3v3mon:");
    Fl_Output *HKn3v3mon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 265, 60, 20);
    HKn3v3mon->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_n3v3mon);
    HKn3v3mon->value(buffer);
    HKn3v3mon->box(FL_FLAT_BOX);
    HKn3v3mon->textcolor(output);
    HK11->box(FL_FLAT_BOX);
    HK11->color(box);
    HK11->labelfont();
    HK11->labelcolor(text);
    HK11->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *HK9 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 285, 50, 20, "n5vmon:");
    Fl_Output *HKn5vmon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 285, 60, 20);
    HKn5vmon->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_n5vmon);
    HKn5vmon->value(buffer);
    HKn5vmon->box(FL_FLAT_BOX);
    HKn5vmon->textcolor(output);
    HK9->box(FL_FLAT_BOX);
    HK9->color(box);
    HK9->labelfont();
    HK9->labelcolor(text);
    HK9->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *HK13 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 305, 50, 20, "15vmon:");
    Fl_Output *HK15vmon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 305, 60, 20);
    HK15vmon->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_15vmon);
    HK15vmon->value(buffer);
    HK15vmon->box(FL_FLAT_BOX);
    HK15vmon->textcolor(output);
    HK13->color(box);
    HK13->box(FL_FLAT_BOX);
    HK13->labelfont();
    HK13->labelcolor(text);
    HK13->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *HK12 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 325, 50, 20, "5vrefmon:");
    Fl_Output *HK5vrefmon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 325, 60, 20);
    HK5vrefmon->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_5vrefmon);
    HK5vrefmon->value(buffer);
    HK5vrefmon->box(FL_FLAT_BOX);
    HK5vrefmon->textcolor(output);
    HK12->box(FL_FLAT_BOX);
    HK12->color(box);
    HK12->labelfont();
    HK12->labelcolor(text);
    HK12->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *HK6 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 345, 50, 20, "n200vmon:");
    Fl_Output *HKn150vmon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 345, 60, 20);
    HKn150vmon->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_n150vmon);
    HKn150vmon->value(buffer);
    HKn150vmon->box(FL_FLAT_BOX);
    HKn150vmon->textcolor(output);
    HK6->box(FL_FLAT_BOX);
    HK6->color(box);
    HK6->labelfont();
    HK6->labelcolor(text);
    HK6->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *HK7 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 365, 50, 20, "n800vmon:");
    Fl_Output *HKn800vmon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 365, 60, 20);
    HKn800vmon->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_n800vmon);
    HKn800vmon->value(buffer);
    HKn800vmon->box(FL_FLAT_BOX);
    HKn800vmon->textcolor(output);
    HK7->box(FL_FLAT_BOX);
    HK7->color(box);
    HK7->labelfont();
    HK7->labelcolor(text);
    HK7->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    writeSerialData(serialPort, 0x10);
    usleep(10000);
    writeSerialData(serialPort, 0x11);
    usleep(10000);
    writeSerialData(serialPort, 0x12);
    usleep(10000);
    writeSerialData(serialPort, 0x13);
    usleep(10000);
    writeSerialData(serialPort, 0x14);
    usleep(10000);
    writeSerialData(serialPort, 0x15);
    usleep(10000);
    writeSerialData(serialPort, 0x16);
    usleep(10000);
    writeSerialData(serialPort, 0x17);
    usleep(10000);
    writeSerialData(serialPort, 0x18);
    usleep(10000);
    writeSerialData(serialPort, 0x19);
    usleep(10000);
    writeSerialData(serialPort, 0x1A);
    usleep(10000);
    writeSerialData(serialPort, 0x0A);
    usleep(10000);
    writeSerialData(serialPort, 0x09);

    window->show(); // Opening main window before entering main loop
    Fl::check();

    // ---------------- MAIN PROGRAM EVENT LOOP ----------------
    while (1)
    {
        
        // if (toleranceCheck(HKn800vmon, HK7, 800))
        // {
        //     PB6->value(0);
        // }
        // if (toleranceCheck(HK3v3mon, HK11, 3.3))
        // {
        //     PC10->value(0);
        // }
        // if (toleranceCheck(HKn150vmon, HK6, 1.54))
        // {
        //     PC13->value(0);
        // }
        // if (toleranceCheck(HK15vmon, HK13, 15))
        // {
        //     PC7->value(0);
        // }
        // if (toleranceCheck(HKn5vmon, HK9, 5))
        // {
        //     PC8->value(0);
        // }
        // if (toleranceCheck(HK5vmon, HK10, 5))
        // {
        //     PC9->value(0);
        // }
        // if (toleranceCheck(HKn3v3mon, HK11, 3.3))
        // {
        //     PC6->value(0);
        // }
        // toleranceCheck(HKtemp1, tempLabel1, 27);
        // toleranceCheck(HKtemp2, tempLabel2, 27);
        // toleranceCheck(HKtemp3, tempLabel3, 27);
        // toleranceCheck(HKtemp4, tempLabel4, 27);

        // No GPIO associated with the following:
        // - 2v5 mon
        // - 5vref mon
        // - vsense
        // - vrefint
        snprintf(currentFactorBuf, sizeof(currentFactorBuf), "%d", currentFactor);
        curFactor->value(currentFactorBuf);
        char tempBuf[8];
        snprintf(tempBuf, sizeof(tempBuf), "%d", step);
        currStep->value(tempBuf);

        snprintf(tempBuf, sizeof(tempBuf), "%f", stepVoltages[step]);
        stepVoltage->value(tempBuf);

        if (valPMT == '0' && valERPA == '0' && valHK == '0' &&
            turnedOff == 0) // Checking if all packet data is toggled off
        {
            turnedOff = 1;
        }
        else if ((valPMT == '1' || valERPA == '1' || valHK == '1') && turnedOff == 1)
        {
            turnedOff = 0;
        }
        if (PMT_ON->value() != valPMT && PMT_ON->value() == 1) // Toggling individual packet data
        {
            totalBPS += pmtBPS;
            valPMT = PMT_ON->value();
            writeSerialData(serialPort, 0x0D);
            writeToControlsLog("1", "", "", "", "", "", "", "", "", "", "", "", "");
        }
        else if (PMT_ON->value() != valPMT && PMT_ON->value() == 0) // Toggling individual packet data
        {
            totalBPS -= pmtBPS;
            valPMT = PMT_ON->value();
            writeSerialData(serialPort, 0x10);
            writeToControlsLog("0", "", "", "", "", "", "", "", "", "", "", "", "");
        }
        if (ERPA_ON->value() != valERPA && ERPA_ON->value() == 1)
        {
            totalBPS += erpaBPS;
            valERPA = ERPA_ON->value();
            writeSerialData(serialPort, 0x0E);
            writeToControlsLog("", "1", "", "", "", "", "", "", "", "", "", "", "");
        }
        else if (ERPA_ON->value() != valERPA && ERPA_ON->value() == 0)
        {
            totalBPS -= erpaBPS;
            valERPA = ERPA_ON->value();
            writeSerialData(serialPort, 0x11);
            writeToControlsLog("", "0", "", "", "", "", "", "", "", "", "", "", "");
        }
        if (HK_ON->value() != valHK && HK_ON->value() == 1)
        {
            totalBPS += hkBPS;
            totalBPS += tempsBPS;
            valHK = HK_ON->value();
            writeSerialData(serialPort, 0x0F);
            writeToControlsLog("", "", "1", "", "", "", "", "", "", "", "", "", "");
        }
        else if (HK_ON->value() != valHK && HK_ON->value() == 0)
        {
            totalBPS -= hkBPS;
            totalBPS -= tempsBPS;
            valHK = HK_ON->value();
            writeSerialData(serialPort, 0x12);
            writeToControlsLog("", "", "0", "", "", "", "", "", "", "", "", "", "");
        }

        if (PB5->value() != valPB5 &&
            PB5->value() == 1) // Making sure sys_on (PB5) is on before activating other GPIO buttons
        {
            writeToControlsLog("", "", "", "1", "", "", "", "", "", "", "", "", "");

            valPB5 = PB5->value();
            writeSerialData(serialPort, 0x00);
            PB6->activate();
            PC10->activate();
            PC13->activate();
            PC7->activate();
            PC8->activate();
            PC9->activate();
            PC6->activate();
        }
        else if (PB5->value() != valPB5 &&
                 PB5->value() == 0)
        {
            writeToControlsLog("", "", "", "0", "", "", "", "", "", "", "", "", "");

            valPB5 = PB5->value();
            writeSerialData(serialPort, 0x13);

            PB6->deactivate();
            PC10->deactivate();
            PC13->deactivate();
            PC7->deactivate();
            PC8->deactivate();
            PC9->deactivate();
            PC6->deactivate();

            PB6->value(0);
            valPB6 = PB6->value();
            writeSerialData(serialPort, 0x14);
            PC10->value(0);
            valPC10 = PC10->value();
            writeSerialData(serialPort, 0x15);
            PC13->value(0);
            valPC13 = PC13->value();
            writeSerialData(serialPort, 0x16);
            PC7->value(0);
            valPC7 = PC7->value();
            writeSerialData(serialPort, 0x17);
            PC8->value(0);
            valPC8 = PC8->value();
            writeSerialData(serialPort, 0x18);
            PC9->value(0);
            valPC9 = PC9->value();
            writeSerialData(serialPort, 0x19);
            PC6->value(0);
            valPC6 = PC6->value();
            writeSerialData(serialPort, 0x1A);
        }
        if (PB5->value())
        {
            if (PB6->value() != valPB6 && PB6->value() == 1)
            {
                valPB6 = PB6->value();
                writeSerialData(serialPort, 0x01);
                writeToControlsLog("", "", "", "", "1", "", "", "", "", "", "", "", "");
            }
            else if (PB6->value() != valPB6 && PB6->value() == 0)
            {
                valPB6 = PB6->value();
                writeSerialData(serialPort, 0x14);
                writeToControlsLog("", "", "", "", "0", "", "", "", "", "", "", "", "");
            }
            if (PC10->value() != valPC10 && PC10->value() == 1)
            {
                valPC10 = PC10->value();
                writeSerialData(serialPort, 0x02);
                writeToControlsLog("", "", "", "", "", "1", "", "", "", "", "", "", "");
            }
            else if (PC10->value() != valPC10 && PC10->value() == 0)
            {
                valPC10 = PC10->value();
                writeSerialData(serialPort, 0x15);
                writeToControlsLog("", "", "", "", "", "0", "", "", "", "", "", "", "");
            }
            if (PC13->value() != valPC13 && PC13->value() == 1)
            {
                valPC13 = PC13->value();
                writeSerialData(serialPort, 0x03);
                writeToControlsLog("", "", "", "", "", "", "1", "", "", "", "", "", "");
            }
            else if (PC13->value() != valPC13 && PC13->value() == 0)
            {
                valPC13 = PC13->value();
                writeSerialData(serialPort, 0x16);
                writeToControlsLog("", "", "", "", "", "", "0", "", "", "", "", "", "");
            }
            if (PC7->value() != valPC7 && PC7->value() == 1)
            {
                valPC7 = PC7->value();
                writeSerialData(serialPort, 0x04);
                writeToControlsLog("", "", "", "", "", "", "", "1", "", "", "", "", "");
            }
            else if (PC7->value() != valPC7 && PC7->value() == 0)
            {
                valPC7 = PC7->value();
                writeSerialData(serialPort, 0x17);
                writeToControlsLog("", "", "", "", "", "", "", "0", "", "", "", "", "");
            }
            if (PC8->value() != valPC8 && PC8->value() == 1)
            {
                valPC8 = PC8->value();
                writeSerialData(serialPort, 0x05);
                writeToControlsLog("", "", "", "", "", "", "", "", "1", "", "", "", "");
            }
            else if (PC8->value() != valPC8 && PC8->value() == 0)
            {
                valPC8 = PC8->value();
                writeSerialData(serialPort, 0x18);
                writeToControlsLog("", "", "", "", "", "", "", "", "0", "", "", "", "");
            }
            if (PC9->value() != valPC9 && PC9->value() == 1)
            {
                valPC9 = PC9->value();
                writeSerialData(serialPort, 0x06);
                writeToControlsLog("", "", "", "", "", "", "", "", "", "1", "", "", "");
            }
            else if (PC9->value() != valPC9 && PC9->value() == 0)
            {
                valPC9 = PC9->value();
                writeSerialData(serialPort, 0x19);
                writeToControlsLog("", "", "", "", "", "", "", "", "", "0", "", "", "");
            }
            if (PC6->value() != valPC6 && PC6->value() == 1)
            {
                valPC6 = PC6->value();
                writeSerialData(serialPort, 0x07);
                writeToControlsLog("", "", "", "", "", "", "", "", "", "", "1", "", "");
            }
            if (PC6->value() != valPC6 && PC6->value() == 0)
            {
                valPC6 = PC6->value();
                writeSerialData(serialPort, 0x1A);
                writeToControlsLog("", "", "", "", "", "", "", "", "", "", "0", "", "");
            }
        }

        if (SDN1->value() != valSDN1 && SDN1->value() == 1)
        {
            valSDN1 = SDN1->value();
            writeSerialData(serialPort, 0x0B);
            writeToControlsLog("", "", "", "", "", "", "", "", "", "", "", "1", "");
        }
        else if (SDN1->value() != valSDN1 && SDN1->value() == 0)
        {
            valSDN1 = SDN1->value();
            writeSerialData(serialPort, 0x0A);
            writeToControlsLog("", "", "", "", "", "", "", "", "", "", "", "0", "");
        }
        if (SDN2->value() != valSDN2 && SDN2->value() == 1)
        {
            valSDN2 = SDN2->value();
            writeSerialData(serialPort, 0x08);
            writeToControlsLog("", "", "", "", "", "", "", "", "", "", "", "", "1");
        }
        else if (SDN2->value() != valSDN2 && SDN2->value() == 0)
        {
            valSDN2 = SDN2->value();
            writeSerialData(serialPort, 0x09);
            writeToControlsLog("", "", "", "", "", "", "", "", "", "", "", "", "0");
        }

        if (turnedOff == 0) // Checking if data is being received before going through packet data
        {
            outputFile.flush();
            vector<string> strings = interpret("mylog.0");
            if (!strings.empty())
            {
                truncate("mylog.0", 0);
                for (int i = 0; i < strings.size(); i++)
                {
                    //cout << strings[i] << endl;
                    char letter = strings[i][0];
                    strings[i] = strings[i].substr(2);
                    if (ERPA_ON->value())
                    {
                        switch (letter)
                        {
                        case 'a':
                        {
                            if (recording)
                            {
                                writeToErpaLog(erpaFrame[0], erpaFrame[1], erpaFrame[6], erpaFrame[3], erpaFrame[4], erpaFrame[5], erpaFrame[2]);
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
                        case 'c':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            ERPAendmon->value(buffer);
                            string logMsg(buffer);
                            erpaFrame[6] = logMsg;
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
                        case 'f':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            ERPAtemp2->value(buffer);
                            string logMsg(buffer);
                            erpaFrame[5] = logMsg;
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
                        }
                    }
                    if (PMT_ON->value())
                    {
                        switch (letter)
                        {
                        case 'i':
                        {
                            if (recording)
                            {
                                writeToPMTLog(pmtFrame[0], pmtFrame[1], pmtFrame[2]);
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
                        }
                    }
                    if (HK_ON->value())
                    {
                        switch (letter)
                        {
                        case 'l':
                        {
                            if (recording)
                            {
                                writeToHKLog(hkFrame[0], hkFrame[1], hkFrame[13], hkFrame[14], hkFrame[15], hkFrame[16], hkFrame[17], hkFrame[18], hkFrame[2], hkFrame[3], hkFrame[7], hkFrame[4], hkFrame[9], hkFrame[10], hkFrame[8], hkFrame[12], hkFrame[11], hkFrame[5], hkFrame[6]);
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
                        }
                    }
                }
            }
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
