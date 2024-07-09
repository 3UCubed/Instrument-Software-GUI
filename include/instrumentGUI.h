/**
 * @file instrumentGUI.h
 * @author Jared Morrison
 * @version 2.0.0-alpha
 * @section DESCRIPTION
 *
 * GUI that connects to H7-Instrument-Software and shows packet data in real time
 */

#define BAUD 460800

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
#include "../include/doubleBuffer.h"
#include "../include/logger.h"
using namespace std;

enum Packet_t{
    ERPA,
    PMT,
    HK,
    ERROR_PACKET,
    UNDEFINED
};

// ******************************************************************************************************************* GLOBALS
Logger logger;
Logger guiLogger;
DoubleBuffer storage;
const int windowWidth = 1175;
const int windowHeight = 600;
const int xPacketOffset = 380;
const int yPacketOffset = 75;
const int xControlOffset = 0;
const int yControlOffset = 0;
const int xGUIOffset = -120;
const int yGUIOffset = 0;
const char *GUI_VERSION_NUM = "G-2.0.0-alpha";
int currentFactor = 1;
char currentFactorBuf[8];
char buffer[32];
int step = 0;
string erpaFrame[6];
string pmtFrame[4];
string hkFrame[20];
float stepVoltages[8] = {0, 0.5, 1, 1.5, 2, 2.5, 3, 3.3};

const char *portName = "";
int serialPort;
std::thread readThread;
std::atomic<bool> stopFlag(false);
std::atomic<bool> recording(false);

Fl_Window *window;
Fl_Output *instrumentVersion;
Fl_Box *group6;
Fl_Box *group4;
Fl_Box *group2;
Fl_Box *group1;
Fl_Box *group3;
Fl_Box *ERPA1;
Fl_Box *ERPA2;
Fl_Box *ERPA4;
Fl_Box *ERPA5;
Fl_Box *ERPA3;
Fl_Box *PMT1;
Fl_Box *PMT2;
Fl_Box *PMT3;
Fl_Box *HK1;
Fl_Box *HK2;
Fl_Box *HK14;
Fl_Box *HK15;
Fl_Box *tempLabel1;
Fl_Box *tempLabel2;
Fl_Box *tempLabel3;
Fl_Box *tempLabel4;
Fl_Box *HK3;
Fl_Box *HK4;
Fl_Box *HK8;
Fl_Box *HK5;
Fl_Box *HK10;
Fl_Box *HK11;
Fl_Box *HK9;
Fl_Box *HK13;
Fl_Box *HK12;
Fl_Box *HK6;
Fl_Box *HK7;
Fl_Button *quit;
Fl_Button *syncWithInstruments;
Fl_Button *autoStartUp;
Fl_Button *autoShutDown;
Fl_Button *stepUp;
Fl_Button *stepDown;
Fl_Button *enterStopMode;
Fl_Button *exitStopMode;
Fl_Button *increaseFactor;
Fl_Button *decreaseFactor;
Fl_Button *startRecording;
Fl_Round_Button *PMTOn;
Fl_Round_Button *ERPAOn;
Fl_Round_Button *HKOn;
Fl_Round_Button *PB5;
Fl_Round_Button *PC7;
Fl_Round_Button *PC10;
Fl_Round_Button *PC6;
Fl_Round_Button *PC8;
Fl_Round_Button *PC9;
Fl_Round_Button *PC13;
Fl_Round_Button *PB6;
Fl_Light_Button *SDN1;
Fl_Light_Button *autoSweep;
Fl_Output *curFactor;
Fl_Output *currStep;
Fl_Output *stepVoltage;
Fl_Output *ERPAsync;
Fl_Output *ERPAseq;
Fl_Output *ERPAswp;
Fl_Output *ERPAtemp1;
Fl_Output *ERPAadc;
Fl_Output *PMTsync;
Fl_Output *PMTseq;
Fl_Output *PMTadc;
Fl_Output *HKsync;
Fl_Output *HKseq;
Fl_Output *HKvsense;
Fl_Output *HKvrefint;
Fl_Output *HKtemp1;
Fl_Output *HKtemp2;
Fl_Output *HKtemp3;
Fl_Output *HKtemp4;
Fl_Output *HKbusvmon;
Fl_Output *HKbusimon;
Fl_Output *HK2v5mon;
Fl_Output *HK3v3mon;
Fl_Output *HK5vmon;
Fl_Output *HKn3v3mon;
Fl_Output *HKn5vmon;
Fl_Output *HK15vmon;
Fl_Output *HK5vrefmon;
Fl_Output *HKn150vmon;
Fl_Output *HKn800vmon;
Fl_Output *dateTime;
Fl_Output *guiVersion;
Fl_Output *errorCodeOutput;