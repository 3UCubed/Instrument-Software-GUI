#include <cstdio>
#include <iostream>
#include <vector>
#include <fstream>

using namespace std;

double tempsToCelsius(int val)
{
    char *convertedChar;
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

double intToVoltage(int value, int resolution, int ref, float mult)
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

double intToCelsius(int value, int resolution, int ref)
{
    double mVoltage;
    double temperature;
    if (resolution == 12)
    {
        mVoltage = (intToVoltage(value, resolution, ref, 1) * 1000);
        temperature = (mVoltage - 2035) / -4.5;
    }

    if (resolution == 16)
    {
        mVoltage = (intToVoltage(value, resolution, ref, 1) * 1000);
        temperature = (mVoltage - 2035) / -4.5;
    }
    return temperature;
}

vector<string> interpret(const string &inputStr)
{
    vector<string> strings;
    //  char strings[1000][1000];
    char result[1000];
    int arrCounter = 0;
    std::ifstream inputFile(inputStr, std::ios::binary);

    if (!inputFile)
    {
        printf("ERROR OPENING FILE!");
        exit(-1);
    }

    char byte = 0;
    char sync[2];
    int packet = 0;

    string erpaLabels[9] = {"a", "b", "d", "e", "g", "H", "M", "S", "X"};
    int erpaValues[9];
    int erpaIndex = 0;
    int erpaValid = 0;

    string pmtLabels[7] = {"i", "j", "k", "H", "M", "S", "X"};
    int pmtValues[7];
    int pmtIndex = 0;
    int pmtValid = 0;

    string hkLabels[23] = {"l", "m", "n", "o", "p",
                           "q", "r", "s", "t",
                           "u", "v", "w", "x", "y", "z", "A", "B", "C", "D", "H", "M", "S", "X"};
    int hkValues[23];
    int hkIndex = 0;
    int hkValid = 0;

    while (inputFile.get(byte))
    {
        //    for (int i = 0; input[i] != '\0'; i++) {
        sync[0] = sync[1];
        sync[1] = byte;

        // printf("0x%02X 0x%02X \n", (sync[0] & 0xFF), (sync[1] & 0xFF));

        if ((sync[0] & 0xFF) == 0xAA && (sync[1] & 0xFF) == 0xAA)
        {
            erpaValid = 1;
            erpaIndex = 0;
            packet = 1;
        }
        else if ((sync[0] & 0xFF) == 0xBB && (sync[1] & 0xFF) == 0xBB)
        {
            pmtValid = 1;
            pmtIndex = 0;
            packet = 2;
        }
        else if ((sync[0] & 0xFF) == 0xCC && (sync[1] & 0xFF) == 0xCC)
        {
            hkValid = 1;
            hkIndex = 0;
            packet = 3;
        }
        else
        {
            //            cout << "BAAAAAD BIIIIIIITS" << endl;
        }
        if (packet == 1)
        {
            if (erpaValid)
            {
                erpaValues[erpaIndex] = ((sync[0] & 0xFF) << 8) | (sync[1] & 0xFF);
                switch (erpaIndex)
                {
                case 0:
                    /* SYNC Bytes; should be 0xAAAA */
                    sprintf(result, "%s:0x%X", erpaLabels[erpaIndex].c_str(), erpaValues[erpaIndex]);
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 1:
                    /* SEQ Bytes; 0-65535 */
                    sprintf(result, "%s:%04d", erpaLabels[erpaIndex].c_str(), erpaValues[erpaIndex]);
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 2:
                    /* SWP Monitored */
                    sprintf(result, "%s:%06.5f", erpaLabels[erpaIndex].c_str(),
                            intToVoltage(erpaValues[erpaIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 3:
                    /* TEMP Op-Amp1 */
                    sprintf(result, "%s:%06.5f", erpaLabels[erpaIndex].c_str(),
                            intToVoltage(erpaValues[erpaIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 4:
                    /* ERPA eADC Bytes; Interpreted as Volts */
                    sprintf(result, "%s:%08.7f", erpaLabels[erpaIndex].c_str(),
                            intToVoltage(erpaValues[erpaIndex], 16, 5, 1.0));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 5:
                    /* Hours*/
                    sprintf(result, "%s:%3d", erpaLabels[erpaIndex].c_str(), erpaValues[erpaIndex]);
                    strings.push_back(result);
                    cout << result << endl;
                    arrCounter++;
                    break;
                case 6:
                    /* Minutes */
                    sprintf(result, "%s:%3d", erpaLabels[erpaIndex].c_str(), erpaValues[erpaIndex]);
                    strings.push_back(result);
                    cout << result << endl;
                    arrCounter++;
                    break;
                case 7:
                    /* Seconds*/
                    sprintf(result, "%s:%3d", erpaLabels[erpaIndex].c_str(), erpaValues[erpaIndex]);
                    strings.push_back(result);
                    cout << result << endl;
                    arrCounter++;
                    break;
                case 8:
                    /* Millis */
                    sprintf(result, "%s:%3d", erpaLabels[erpaIndex].c_str(), erpaValues[erpaIndex]);
                    strings.push_back(result);
                    cout << result << endl;
                    arrCounter++;
                    break;
                }
                erpaIndex = (erpaIndex + 1) % 9;
            }
            erpaValid = !erpaValid;
        }
        else if (packet == 2)
        {
            if (pmtValid)
            {
                pmtValues[pmtIndex] = ((sync[0] & 0xFF) << 8) | (sync[1] & 0xFF);
                switch (pmtIndex)
                {
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
                    sprintf(result, "%s:%08.7f", pmtLabels[pmtIndex].c_str(),
                            intToVoltage(pmtValues[pmtIndex], 16, 5, 1.0));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 3:
                    /* Hours*/
                    sprintf(result, "%s:%3d", pmtLabels[pmtIndex].c_str(), pmtValues[pmtIndex]);
                    strings.push_back(result);
                    cout << result << endl;
                    arrCounter++;
                    break;
                case 4:
                    /* Minutes*/
                    sprintf(result, "%s:%3d", pmtLabels[pmtIndex].c_str(), pmtValues[pmtIndex]);
                    strings.push_back(result);
                    cout << result << endl;

                    arrCounter++;
                    break;
                case 5:
                    /* Seconds*/
                    sprintf(result, "%s:%3d", pmtLabels[pmtIndex].c_str(), pmtValues[pmtIndex]);
                    strings.push_back(result);
                    cout << result << endl;

                    arrCounter++;
                    break;
                case 6:
                    /* Milliseconds */
                    sprintf(result, "%s:%3d", pmtLabels[pmtIndex].c_str(), pmtValues[pmtIndex]);
                    strings.push_back(result);
                    cout << result << endl;

                    arrCounter++;
                    break;
                }

                pmtIndex = (pmtIndex + 1) % 7;
            }
            pmtValid = !pmtValid;
        }
        else if (packet == 3)
        {
            if (hkValid)
            {
                hkValues[hkIndex] = ((sync[0] & 0xFF) << 8) | (sync[1] & 0xFF);
                switch (hkIndex)
                {
                case 0:
                    /* l SYNC Bytes; should be 0xCCCC */
                    sprintf(result, "%s:0x%X ", hkLabels[hkIndex].c_str(), hkValues[hkIndex]);
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 1:
                    /* m SEQ Bytes; 0-65535 */
                    sprintf(result, "%s:%04d ", hkLabels[hkIndex].c_str(), hkValues[hkIndex]);
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 2:
                    /* n vsense */
                    sprintf(result, "%s:%06.5f", hkLabels[hkIndex].c_str(),
                            intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 3:
                    /* o vrefint */
                    sprintf(result, "%s:%06.5f", hkLabels[hkIndex].c_str(),
                            intToVoltage(hkValues[hkIndex], 12, 3, 1.0));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 4:
                    /* p temp1 */
                    sprintf(result, "%s:%06.5f", hkLabels[hkIndex].c_str(),
                            tempsToCelsius(hkValues[hkIndex]));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 5:
                    /* q temp2 */
                    sprintf(result, "%s:%06.5f", hkLabels[hkIndex].c_str(),
                            tempsToCelsius(hkValues[hkIndex]));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 6:
                    /* r temp3 */
                    sprintf(result, "%s:%06.5f", hkLabels[hkIndex].c_str(),
                            tempsToCelsius(hkValues[hkIndex]));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 7:
                    /* s temp4 */
                    sprintf(result, "%s:%06.5f", hkLabels[hkIndex].c_str(),
                            tempsToCelsius(hkValues[hkIndex]));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 8:
                    /* t BUS_Vmon */
                    sprintf(result, "%s:%06.5f", hkLabels[hkIndex].c_str(),
                            intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 9:
                    /* u BUS_Imon */
                    sprintf(result, "%s:%06.5f", hkLabels[hkIndex].c_str(),
                            intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 10:
                    /* v 2v5_mon */
                    sprintf(result, "%s:%06.5f", hkLabels[hkIndex].c_str(),
                            intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 11:
                    /* w 3v3_mon */
                    sprintf(result, "%s:%06.5f", hkLabels[hkIndex].c_str(),
                            intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 12:
                    /* x 5v_mon */
                    sprintf(result, "%s:%06.5f", hkLabels[hkIndex].c_str(),
                            intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 13:
                    /* y n3v3_mon */
                    sprintf(result, "%s:%06.5f", hkLabels[hkIndex].c_str(),
                            intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 14:
                    /* z n5v_mon */
                    sprintf(result, "%s:%06.5f", hkLabels[hkIndex].c_str(),
                            intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 15:
                    /* A 15v_mon */
                    sprintf(result, "%s:%06.5f", hkLabels[hkIndex].c_str(),
                            intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 16:
                    /* B 5vref_mon */
                    sprintf(result, "%s:%06.5f", hkLabels[hkIndex].c_str(),
                            intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 17:
                    /* C n150v_mon */
                    sprintf(result, "%s:%06.5f", hkLabels[hkIndex].c_str(),
                            intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 18:
                    /* D n800v_mon */
                    sprintf(result, "%s:%06.5f", hkLabels[hkIndex].c_str(),
                            intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    arrCounter++;
                    break;
                case 19:
                    /* Hours*/
                    sprintf(result, "%s:%3d", hkLabels[hkIndex].c_str(), hkValues[hkIndex]);
                    strings.push_back(result);
                    cout << result << endl;
                    arrCounter++;
                    break;
                case 20:
                    /* Minutes*/
                    sprintf(result, "%s:%3d", hkLabels[hkIndex].c_str(), hkValues[hkIndex]);
                    strings.push_back(result);
                    cout << result << endl;
                    arrCounter++;
                    break;
                case 21:
                    /* Seconds */
                    sprintf(result, "%s:%3d", hkLabels[hkIndex].c_str(), hkValues[hkIndex]);
                    strings.push_back(result);
                    cout << result << endl;
                    arrCounter++;
                    break;
                case 22:
                    /* Millis */
                    sprintf(result, "%s:%3d", hkLabels[hkIndex].c_str(), hkValues[hkIndex]);
                    strings.push_back(result);
                    cout << result << endl;
                    arrCounter++;
                    break;
                }
                hkIndex = (hkIndex + 1) % 23;
            }
            hkValid = !hkValid;
        }
    }

    //    fclose(input);
    return strings;
}
