#include <cstdio>
#include <iostream>
#include <vector>
#include <fstream>

using namespace std;

double tempsToCelsius(int val) {
    char *convertedChar;
    double convertedTemp;

    // Convert to 2's complement, since temperature can be negative
    if (val > 0x7FF) {
        val |= 0xF000;
    }

    // Convert to float temperature value (Celsius)
    float temp_c = val * 0.0625;

    // Convert temperature to decimal value
    temp_c *= 100;

    sprintf(convertedChar, "%u.%u",
            ((unsigned int) temp_c / 100),
            ((unsigned int) temp_c % 100));

    convertedTemp = std::stod(convertedChar);

    return convertedTemp;

}

double intToVoltage(int value, int resolution, int ref, float mult) {
    double voltage;
    if (resolution == 12) {
        voltage = (double) (value * ref) / 4095 * mult;
    }

    if (resolution == 16) {
        voltage = (double) (value * ref) / 65535 * mult;
    }
    return voltage;
}

double intToCelsius(int value, int resolution, int ref) {
    double mVoltage;
    double temperature;
    if (resolution == 12) {
        mVoltage = (intToVoltage(value, resolution, ref, 1) * 1000);
        temperature = (mVoltage - 2035) / -4.5;
    }

    if (resolution == 16) {
        mVoltage = (intToVoltage(value, resolution, ref, 1) * 1000);
        temperature = (mVoltage - 2035) / -4.5;
    }
    return temperature;
}

vector <string> interpret(const string &inputStr) {
    vector <string> strings;
    //  char strings[1000][1000];
    char result[1000];
    int arrCounter = 0;
    std::ifstream inputFile(inputStr, std::ios::binary);

    if (!inputFile) {
        printf("ERROR OPENING FILE!");
        exit(-1);
    }

    char byte = 0;
    char sync[2];
    int packet = 0;

    string erpaLabels[8] = {"a", "b", "c", "d", "e", "f", "g", "h"};
    int erpaValues[8];
    int erpaIndex = 0;
    int erpaValid = 0;

    string pmtLabels[3] = {"i", "j", "k"};
    int pmtValues[3];
    int pmtIndex = 0;
    int pmtValid = 0;

    string hkLabels[14] = {"l", "m", "n", "o", "p",
                           "q", "r", "s", "t",
                           "u", "v", "w", "x", "y"};
    int hkValues[14];
    int hkIndex = 0;
    int hkValid = 0;

    string tempsLabels[6] = {"A", "B", "C", "D", "E", "F"};
    int tempsValues[6];
    int tempsIndex = 0;
    int tempsValid = 0;

    while (inputFile.get(byte)) {
        //    for (int i = 0; input[i] != '\0'; i++) {
        sync[0] = sync[1];
        sync[1] = byte;

        // printf("0x%02X 0x%02X \n", (sync[0] & 0xFF), (sync[1] & 0xFF));

        if ((sync[0] & 0xFF) == 0xAA && (sync[1] & 0xFF) == 0xAA) {
            erpaValid = 1;
            erpaIndex = 0;
            packet = 1;
        } else if ((sync[0] & 0xFF) == 0xBB && (sync[1] & 0xFF) == 0xBB) {
            pmtValid = 1;
            pmtIndex = 0;
            packet = 2;
        } else if ((sync[0] & 0xFF) == 0xCC && (sync[1] & 0xFF) == 0xCC) {
            hkValid = 1;
            hkIndex = 0;
            packet = 3;
        } else if ((sync[0] & 0xFF) == 0xDD && (sync[1] & 0xFF) == 0xDD) {
            tempsValid = 1;
            tempsIndex = 0;
            packet = 4;
        } else {
            //            cout << "BAAAAAD BIIIIIIITS" << endl;
        }
        if (packet == 1) {
            if (erpaValid) {
                erpaValues[erpaIndex] = ((sync[0] & 0xFF) << 8) | (sync[1] & 0xFF);
                switch (erpaIndex) {
                    case 0:
                        /* SEQ Bytes; should be 0xAAAA */
                        sprintf(result, "%s:0x%X", erpaLabels[erpaIndex].c_str(), erpaValues[erpaIndex]);
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 1:
                        /* SYNC Bytes; 0-65535 */
                        sprintf(result, "%s:%04d", erpaLabels[erpaIndex].c_str(), erpaValues[erpaIndex]);
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 2:
                        /* ERPA eADC Bytes; Interpreted as Volts */
                        sprintf(result, "%s:%.3f", erpaLabels[erpaIndex].c_str(),
                                intToVoltage(erpaValues[erpaIndex], 16, 5, 1.0));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 3:
                        // /* SWP Commanded */
                        // sprintf(result, "%s:%.1f", erpaLabels[erpaIndex].c_str(),
                        //         intToVoltage(erpaValues[erpaIndex], 12, 3, 1.0));
                        // strings.push_back(result);
                        // arrCounter++;
                        // break;
                        
                        /* ENDMon */
                        sprintf(result, "%s:%06.3f", erpaLabels[erpaIndex].c_str(),
                                intToVoltage(erpaValues[erpaIndex], 12, 3, 1.0));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 4:
                        /* TEMP Op-Amp1 */
                        sprintf(result, "%s:%07.3f", erpaLabels[erpaIndex].c_str(),
                                intToCelsius(erpaValues[erpaIndex], 12, 3));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 5:
                        /* TEMP Op-Amp2 */
                        sprintf(result, "%s:%07.3f", erpaLabels[erpaIndex].c_str(),
                                intToCelsius(erpaValues[erpaIndex], 12, 3));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 6:
                        /* SWP Monitored */
                        sprintf(result, "%s:%.1f", erpaLabels[erpaIndex].c_str(),
                                intToVoltage(erpaValues[erpaIndex], 12, 3, 1.0));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                }
                erpaIndex = (erpaIndex + 1) % 8;
            }
            erpaValid = !erpaValid;
        } else if (packet == 2) {
            if (pmtValid) {
                pmtValues[pmtIndex] = ((sync[0] & 0xFF) << 8) | (sync[1] & 0xFF);
                switch (pmtIndex) {
                    case 0:
                        /* SEQ Bytes; should be 0xBBBB */
                        sprintf(result, "%s:0x%X", pmtLabels[pmtIndex].c_str(), pmtValues[pmtIndex]);
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 1:
                        /* SYNC Bytes; 0-65535 */
                        sprintf(result, "%s:%04d", pmtLabels[pmtIndex].c_str(), pmtValues[pmtIndex]);
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 2:
                        /* PMT eADC Bytes; Interpreted as Volts */
                        sprintf(result, "%s:%.3f", pmtLabels[pmtIndex].c_str(),
                                intToVoltage(pmtValues[pmtIndex], 16, 5, 1.0));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                }
                pmtIndex = (pmtIndex + 1) % 3;
            }
            pmtValid = !pmtValid;
        } else if (packet == 3) {
            if (hkValid) {
                hkValues[hkIndex] = ((sync[0] & 0xFF) << 8) | (sync[1] & 0xFF);
                switch (hkIndex) {
                    case 0:
                        /* SYNC Bytes; should be 0xCCCC */
                        sprintf(result, "%s:0x%X ", hkLabels[hkIndex].c_str(), hkValues[hkIndex]);
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 1:
                        /* SEQ Bytes; 0-65535 */
                        sprintf(result, "%s:%04d ", hkLabels[hkIndex].c_str(), hkValues[hkIndex]);
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 2:
                        /* BUS_Vmon */
                        sprintf(result, "%s:%.3f", hkLabels[hkIndex].c_str(),
                                intToVoltage(hkValues[hkIndex], 12, 3, 1.0));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 3:
                        /* BUS_Imon */
                        sprintf(result, "%s:%.3f", hkLabels[hkIndex].c_str(),
                                intToVoltage(hkValues[hkIndex], 12, 3, 1.0));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 4:
                        /* 3v3_mon */
                        sprintf(result, "%s:%.3f", hkLabels[hkIndex].c_str(),
                                intToVoltage(hkValues[hkIndex], 12, 3, 1.0));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 5:
                        /* n150v_mon */
                        sprintf(result, "%s:%.3f", hkLabels[hkIndex].c_str(),
                                intToVoltage(hkValues[hkIndex], 12, 3, 1.0));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 6:
                        /* n800v_mon */
                        sprintf(result, "%s:%.3f", hkLabels[hkIndex].c_str(),
                                intToVoltage(hkValues[hkIndex], 12, 3, 1.0));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 7:
                        /* 2v5_mon */
                        sprintf(result, "%s:%.3f", hkLabels[hkIndex].c_str(),
                                intToVoltage(hkValues[hkIndex], 12, 3, 1.0));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 8:
                        /* n5v_mon */
                        sprintf(result, "%s:%.3f", hkLabels[hkIndex].c_str(),
                                intToVoltage(hkValues[hkIndex], 12, 3, -1.65));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 9:
                        /* 5v_mon */
                        sprintf(result, "%s:%.3f", hkLabels[hkIndex].c_str(),
                                intToVoltage(hkValues[hkIndex], 12, 3, 1.67));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 10:
                        /* n3v3_mon */
                        sprintf(result, "%s:%.3f", hkLabels[hkIndex].c_str(),
                                intToVoltage(hkValues[hkIndex], 12, 3, -1.05));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 11:
                        /* 5vref_mon */
                        sprintf(result, "%s:%.3f", hkLabels[hkIndex].c_str(),
                                intToVoltage(hkValues[hkIndex], 12, 3, 1.67));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 12:
                        /* 15v_mon */
                        sprintf(result, "%s:%.3f", hkLabels[hkIndex].c_str(),
                                intToVoltage(hkValues[hkIndex], 12, 3, 5.0));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 13:
                        /* vsense */
                        sprintf(result, "%s:%.3f", hkLabels[hkIndex].c_str(),
                                intToVoltage(hkValues[hkIndex], 12, 3, 1.0));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                }
                hkIndex = (hkIndex + 1) % 14;
            }
            hkValid = !hkValid;
        } else if (packet == 4) {
            if (tempsValid) {
                tempsValues[tempsIndex] = ((sync[0] & 0xFF) << 8) | (sync[1] & 0xFF);
                switch (tempsIndex) {
                    case 0:
                        /* SYNC Bytes; should be 0xDDDD */
                        sprintf(result, "%s:0x%X ", tempsLabels[tempsIndex].c_str(), tempsValues[tempsIndex]);
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 1:
                        /* vrefint */
                        sprintf(result, "%s:%.3f", tempsLabels[tempsIndex].c_str(),
                                intToVoltage(tempsValues[tempsIndex], 12, 3, 1.0));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 2:
                        /* temp1 */
                        sprintf(result, "%s:%.3f", tempsLabels[tempsIndex].c_str(),
                                tempsToCelsius(tempsValues[tempsIndex]));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 3:
                        /* temp2 */
                        sprintf(result, "%s:%.3f", tempsLabels[tempsIndex].c_str(),
                                tempsToCelsius(tempsValues[tempsIndex]));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 4:
                        /* temp3 */
                        sprintf(result, "%s:%.3f", tempsLabels[tempsIndex].c_str(),
                                tempsToCelsius(tempsValues[tempsIndex]));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                    case 5:
                        /* temp4 */
                        sprintf(result, "%s:%.3f", tempsLabels[tempsIndex].c_str(),
                                tempsToCelsius(tempsValues[tempsIndex]));
                        strings.push_back(result);
                        arrCounter++;
                        break;
                }
                tempsIndex = (tempsIndex + 1) % 6;
            }
            tempsValid = !tempsValid;
        }
    }

    //    fclose(input);
    return strings;
}
