// ------------------------- README -----------------------
// If you get error "Failed to open serial port" go to line 120

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Round_Button.H>
#include <string>
#include <iostream>
#include <fstream>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include <chrono>
#include <mutex>
#include "interpreter/interpreter.cpp"

using namespace std;

// ------------------- Quit button event ------------------
void buttonCallback(Fl_Widget *widget)
{
    exit(0);
}

// --------------- Write data to serial port --------------
void writeSerialData(const int &serialPort, const std::string &data)
{
    ssize_t bytesWritten = write(serialPort, data.c_str(), data.length());
    if (bytesWritten == -1)
    {
        std::cerr << "Error writing to the serial port." << std::endl;
    }
}

// ----- Continuously reads data from the serial port -----
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
            buffer[bytesRead] = '\0';
            outputFile << buffer;
            bytesReadTotal += bytesRead;
            if (bytesReadTotal >= flushInterval) // Flush and truncate the file if the byte count exceeds the flush interval
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

// ----------------- Main Program Function ----------------
int main(int argc, char **argv)
{
    // ---------------- Output Field Vars -----------------
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
    float hk_busimon = 0;
    float hk_busvmon = 0;
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

    // ------- Vars Keeping Track Of Packet States --------
    unsigned char valPMT;
    unsigned char valERPA;
    unsigned char valHK;
    int turnedOff = 0;

    // --------------------- Thread Vars ------------------
    struct termios options = {};
    std::atomic<bool> stopFlag(false);
    const char *portName = "/dev/cu.usbserial-FT6E0L8J"; // CHANGE TO YOUR PORT NAME
    std::ofstream outputFile("mylog.0", std::ios::out | std::ios::trunc);

    // ------------------ Thread/Port Setup ---------------
    int serialPort = open(portName, O_RDWR | O_NOCTTY); // Opening serial port
    if (serialPort == -1)
    {
        std::cerr << "Failed to open the serial port." << std::endl;
        ::exit(0);
    }

    tcgetattr(serialPort, &options);
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    options.c_cflag |= O_NONBLOCK;
    tcsetattr(serialPort, TCSANOW, &options);

    std::thread readingThread([&serialPort, &stopFlag, &outputFile]
                              { return readSerialData(serialPort, std::ref(stopFlag), std::ref(outputFile)); });

    // ------------ Main Window Elements Setup ------------
    int width = 1100; // Width and Height of Main Window
    int height = 600;
    int x_packet_offset = 250; // X and Y offsets for the three packet groups
    int y_packet_offset = 150;

    Fl_Window *window = new Fl_Window(width, height, "IS Packet Interpreter");
    Fl_Color darkBackground = fl_rgb_color(28, 28, 30);
    Fl_Color text = fl_rgb_color(203, 207, 213);
    Fl_Color box = fl_rgb_color(46, 47, 56);
    Fl_Color output = fl_rgb_color(60, 116, 239);

    window->color(darkBackground);

    Fl_Button *quit = new Fl_Button(15, 10, 40, 40, "ⓧ");
    quit->box(FL_FLAT_BOX);
    quit->color(darkBackground);
    quit->labelcolor(FL_RED);
    quit->labelsize(40);
    quit->callback(buttonCallback);
    Fl_Round_Button *PMT_ON = new Fl_Round_Button(x_packet_offset + 165, y_packet_offset - 18, 20, 20);
    Fl_Round_Button *ERPA_ON = new Fl_Round_Button(x_packet_offset + 450, y_packet_offset - 18, 20, 20);
    Fl_Round_Button *HK_ON = new Fl_Round_Button(x_packet_offset + 725, y_packet_offset - 18, 20, 20);

    // ------------------- CONTROLS GROUP -----------------
    Fl_Box *group4 = new Fl_Box(15, 100, 130, 410, "CONTROLS");
    group4->color(box);
    group4->box(FL_BORDER_BOX);
    group4->labelcolor(text);
    group4->labelfont(FL_BOLD);
    group4->align(FL_ALIGN_TOP);
    Fl_Round_Button *PB5 = new Fl_Round_Button(20, 105, 100, 50, "sys_on PB5");
    Fl_Round_Button *PB6 = new Fl_Round_Button(20, 155, 100, 50, "800v_en PB6");
    Fl_Round_Button *PC10 = new Fl_Round_Button(20, 205, 100, 50, "3v3_en PC10");
    Fl_Round_Button *PC13 = new Fl_Round_Button(20, 255, 100, 50, "n150v_en PC13");
    Fl_Round_Button *PC7 = new Fl_Round_Button(20, 305, 100, 50, "15v_en PC7");
    Fl_Round_Button *PC8 = new Fl_Round_Button(20, 355, 100, 50, "n5v_en PC8");
    Fl_Round_Button *PC9 = new Fl_Round_Button(20, 405, 100, 50, "5v_en PC9");
    Fl_Round_Button *PC6 = new Fl_Round_Button(20, 455, 100, 50, "n3v3_en PC6");
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
    PMT_ON->value(1);
    ERPA_ON->value(1);
    HK_ON->value(1);
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

    // ---------------- PMT Packet Group ------------------
    Fl_Group *group1 = new Fl_Group(x_packet_offset + 15, y_packet_offset, 200, 320,
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

    // ------------------ ERPA Packet Group ---------------
    Fl_Group *group2 = new Fl_Group(x_packet_offset + 295, y_packet_offset, 200, 320,
                                    "ERPA PACKET");
    group2->color(box);
    group2->box(FL_BORDER_BOX);
    group2->labelfont(FL_BOLD);
    group2->labelcolor(text);
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
    Fl_Box *ERPA3 = new Fl_Box(x_packet_offset + 300, y_packet_offset + 45, 50, 20, "ADC:");
    Fl_Output *ERPAadc = new Fl_Output(x_packet_offset + 417, y_packet_offset + 45, 60, 20);
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
    Fl_Box *ERPA4 = new Fl_Box(x_packet_offset + 300, y_packet_offset + 65, 50, 20, "SWP:");
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
    Fl_Box *ERPA7 = new Fl_Box(x_packet_offset + 300, y_packet_offset + 145, 50, 20, "ENDmon:");
    Fl_Output *ERPAendmon = new Fl_Output(x_packet_offset + 417, y_packet_offset + 145, 60, 20);
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

    // ------------------ HK Packet Group -----------------
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
    Fl_Box *HK3 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 45, 50, 20, "BUSimon:");
    Fl_Output *HKbusimon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 45, 60, 20);
    HKbusimon->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_busimon);
    HKbusimon->value(buffer);
    HKbusimon->box(FL_FLAT_BOX);
    HKbusimon->textcolor(output);
    HK3->box(FL_FLAT_BOX);
    HK3->color(box);
    HK3->labelfont();
    HK3->labelcolor(text);
    HK3->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *HK4 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 65, 50, 20, "BUSvmon:");
    Fl_Output *HKbusvmon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 65, 60, 20);
    HKbusvmon->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_busvmon);
    HKbusvmon->value(buffer);
    HKbusvmon->box(FL_FLAT_BOX);
    HKbusvmon->textcolor(output);
    HK4->box(FL_FLAT_BOX);
    HK4->color(box);
    HK4->labelfont();
    HK4->labelcolor(text);
    HK4->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Box *HK5 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 85, 50, 20, "3v3mon:");
    Fl_Output *HK3v3mon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 85, 60, 20);
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
    Fl_Box *HK6 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 105, 50, 20, "n150vmon:");
    Fl_Output *HKn150vmon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 105, 60, 20);
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
    Fl_Box *HK7 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 125, 50, 20, "n800vmon:");
    Fl_Output *HKn800vmon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 125, 60, 20);
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
    Fl_Box *HK8 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 145, 50, 20, "2v5mon:");
    Fl_Output *HK2v5mon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 145, 60, 20);
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
    Fl_Box *HK9 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 165, 50, 20, "n5vmon:");
    Fl_Output *HKn5vmon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 165, 60, 20);
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
    Fl_Box *HK10 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 185, 50, 20, "5vmon:");
    Fl_Output *HK5vmon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 185, 60, 20);
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
    Fl_Box *HK11 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 205, 50, 20, "n3v3mon:");
    Fl_Output *HKn3v3mon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 205, 60, 20);
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
    Fl_Box *HK12 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 225, 50, 20, "5vrefmon:");
    Fl_Output *HK5vrefmon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 225, 60, 20);
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
    Fl_Box *HK13 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 245, 50, 20, "15vmon:");
    Fl_Output *HK15vmon = new Fl_Output(x_packet_offset + 682, y_packet_offset + 245, 60, 20);
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
    Fl_Box *HK14 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 265, 50, 20, "vsense:");
    Fl_Output *HKvsense = new Fl_Output(x_packet_offset + 682, y_packet_offset + 265, 60, 20);
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
    Fl_Box *HK15 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 285, 50, 20, "vrefint:");
    Fl_Output *HKvrefint = new Fl_Output(x_packet_offset + 682, y_packet_offset + 285, 60, 20);
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

    Fl_Box *HK16 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 305, 50, 20, "temp1:");
    Fl_Output *HKtemp1 = new Fl_Output(x_packet_offset + 682, y_packet_offset + 305, 60, 20);
    HKtemp1->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_temp1);
    HKtemp1->value(buffer);
    HKtemp1->box(FL_FLAT_BOX);
    HKtemp1->textcolor(output);
    HK16->box(FL_FLAT_BOX);
    HK16->color(box);
    HK16->labelfont();
    HK16->labelcolor(text);
    HK16->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    Fl_Box *HK17 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 325, 50, 20, "temp2:");
    Fl_Output *HKtemp2 = new Fl_Output(x_packet_offset + 682, y_packet_offset + 325, 60, 20);
    HKtemp2->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_temp2);
    HKtemp2->value(buffer);
    HKtemp2->box(FL_FLAT_BOX);
    HKtemp2->textcolor(output);
    HK17->box(FL_FLAT_BOX);
    HK17->color(box);
    HK17->labelfont();
    HK17->labelcolor(text);
    HK17->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    Fl_Box *HK18 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 345, 50, 20, "temp3:");
    Fl_Output *HKtemp3 = new Fl_Output(x_packet_offset + 682, y_packet_offset + 345, 60, 20);
    HKtemp3->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_temp3);
    HKtemp3->value(buffer);
    HKtemp3->box(FL_FLAT_BOX);
    HKtemp3->textcolor(output);
    HK18->box(FL_FLAT_BOX);
    HK18->color(box);
    HK18->labelfont();
    HK18->labelcolor(text);
    HK18->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    Fl_Box *HK19 = new Fl_Box(x_packet_offset + 580, y_packet_offset + 365, 50, 20, "temp4:");
    Fl_Output *HKtemp4 = new Fl_Output(x_packet_offset + 682, y_packet_offset + 365, 60, 20);
    HKtemp4->color(box);
    snprintf(buffer, sizeof(buffer), "%f", hk_temp4);
    HKtemp4->value(buffer);
    HKtemp4->box(FL_FLAT_BOX);
    HKtemp4->textcolor(output);
    HK19->box(FL_FLAT_BOX);
    HK19->color(box);
    HK19->labelfont();
    HK19->labelcolor(text);
    HK19->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);




    window->show(); // Opening main window before entering main loop
    Fl::check();

    // -------------- MAIN PROGRAM EVENT LOOP -------------
    while (1)
    {
        if (valPMT == '0' && valERPA == '0' && valHK == '0' && turnedOff == 0) // Checking if all packet data is toggled off
        {
            turnedOff = 1;
        }
        else if ((valPMT == '1' || valERPA == '1' || valHK == '1') && turnedOff == 1)
        {
            turnedOff = 0;
        }
        if (PMT_ON->value() != valPMT) // Toggling individual packet data
        {
            valPMT = PMT_ON->value();
            writeSerialData(serialPort, "1");
        }
        if (ERPA_ON->value() != valERPA)
        {
            valERPA = ERPA_ON->value();
            writeSerialData(serialPort, "2");
        }
        if (HK_ON->value() != valHK)
        {
            valHK = HK_ON->value();
            writeSerialData(serialPort, "3");
        }

        if (PB5->value() != valPB5) // Making sure sys_on (PB5) is on before activating other GPIO buttons
        {
            valPB5 = PB5->value();
            writeSerialData(serialPort, "a");
            if (PB5->value())
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
            }
        }
        if (PB5->value())
        {
            if (PB6->value() != valPB6)
            {
                valPB6 = PB6->value();
                writeSerialData(serialPort, "b");
            }
            if (PC10->value() != valPC10)
            {
                valPC10 = PC10->value();
                writeSerialData(serialPort, "c");
            }
            if (PC13->value() != valPC13)
            {
                valPC13 = PC13->value();
                writeSerialData(serialPort, "d");
            }
            if (PC7->value() != valPC7)
            {
                valPC7 = PC7->value();
                writeSerialData(serialPort, "e");
            }
            if (PC8->value() != valPC8)
            {
                valPC8 = PC8->value();
                writeSerialData(serialPort, "f");
            }
            if (PC9->value() != valPC9)
            {
                valPC9 = PC9->value();
                writeSerialData(serialPort, "g");
            }
            if (PC6->value() != valPC6)
            {
                valPC6 = PC6->value();
                writeSerialData(serialPort, "h");
            }
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
                    cout << strings[i] << endl;
                    char letter = strings[i][0];
                    strings[i] = strings[i].substr(2);
                    if (ERPA_ON->value())
                    {
                        switch (letter)
                        {
                        case 'a':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            ERPAsync->value(buffer);
                            break;
                        }
                        case 'b':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            ERPAseq->value(buffer);
                            break;
                        }
                        case 'c':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            ERPAadc->value(buffer);
                            break;
                        }
                        case 'd':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            ERPAswp->value(buffer);
                            break;
                        }
                        case 'f':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            ERPAtemp1->value(buffer);
                            break;
                        }
                        case 'g':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            ERPAtemp2->value(buffer);
                            break;
                        }
                        case 'h':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            ERPAendmon->value(buffer);
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
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            PMTsync->value(buffer);
                            break;
                        }
                        case 'j':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            PMTseq->value(buffer);
                            break;
                        }
                        case 'k':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            PMTadc->value(buffer);
                            break;
                        }
                        }
                    }
                    if (HK_ON->value())
                    {
                        switch (letter)
                        {
                        case 'B':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HKvrefint->value(buffer);
                            break;
                        }
                        case 'C':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HKtemp1->value(buffer);
                            break;
                        }
                        case 'D':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HKtemp2->value(buffer);
                            break;
                        }
                        case 'E':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HKtemp3->value(buffer);
                            break;
                        }
                        case 'F':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HKtemp4->value(buffer);
                            break;
                        }
                        case 'l':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HKsync->value(buffer);
                            break;
                        }
                        case 'm':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HKseq->value(buffer);
                            break;
                        }
                        case 'n':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HKbusimon->value(buffer);
                            break;
                        }
                        case 'o':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HKbusvmon->value(buffer);
                            break;
                        }
                        case 'p':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HK3v3mon->value(buffer);
                            break;
                        }
                        case 'q':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HKn150vmon->value(buffer);
                            break;
                        }
                        case 'r':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HKn800vmon->value(buffer);
                            break;
                        }
                        case 's':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HK2v5mon->value(buffer);
                            break;
                        }
                        case 't':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HKn5vmon->value(buffer);
                            break;
                        }
                        case 'u':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HK5vmon->value(buffer);
                            break;
                        }
                        case 'v':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HKn3v3mon->value(buffer);
                            break;
                        }
                        case 'w':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HK5vrefmon->value(buffer);
                            break;
                        }
                        case 'x':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HK15vmon->value(buffer);
                            break;
                        }
                        case 'y':
                        {
                            snprintf(buffer, sizeof(buffer), "%s", strings[i].c_str());
                            HKvsense->value(buffer);
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

    // ---------------------- Cleanup ---------------------
    stopFlag = true;
    readingThread.join();
    outputFile << '\0';
    outputFile.flush();
    outputFile.close();
    close(serialPort);
    return Fl::run();
}
