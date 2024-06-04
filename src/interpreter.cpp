/**
 * @file interpreter.cpp
 * @author Jared Morrison, Jared King
 * @version 1.0.0-beta
 * @section DESCRIPTION
 *
 * Interprets received serial data for the GUI.
 */

#include <cstdio>
#include <iostream>
#include <vector>
#include <fstream>

using namespace std;

/**
 * @brief Convert temperature value from sensor to Celsius.
 *
 * @param val The raw temperature value from the sensor.
 * @return The temperature value in Celsius.
 */
double tempsToCelsius(int val)
{
    // Convert to 2's complement, since temperature can be negative
    if (val > 0x7FF)
    {
        val |= 0xF000;
    }

    // Convert to float temperature value (Celsius)
    float temp_c = val * 0.0625;

    // Convert temperature to decimal value
    temp_c *= 100;

    // Convert float temperature to string
    std::string temp_str = std::to_string(static_cast<unsigned int>(temp_c) / 100) + "." +
                           std::to_string(static_cast<unsigned int>(temp_c) % 100);

    // Convert string temperature to double
    double convertedTemp = std::stod(temp_str);

    return convertedTemp;
}

/**
 * @brief Convert an integer value to voltage.
 *
 * @param value The integer value to be converted.
 * @param resolution The resolution of the value (12 or 16).
 * @param ref The reference voltage.
 * @param mult The multiplier factor.
 * @return The voltage value.
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
 * @brief Convert an integer value to temperature in Celsius.
 *
 * @param value The integer value to be converted.
 * @param resolution The resolution of the value (12 or 16).
 * @param ref The reference value.
 * @return The temperature value in Celsius.
 */
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

/**
 * @brief Interpret data from a binary input file.
 *
 * Reads data from a binary input file and interprets it based on synchronization bytes.
 * Parses ERPA, PMT, and HK data packets and extracts relevant information.
 *
 * @param inputStr The file path of the binary input file.
 * @return A vector of strings containing interpreted data.
 *
 * \warning
 * This is truly the worst way one could possibly read serial data and interpret it.
 * I had no idea what I was doing when I first wrote this, needs to be redone correctly
 * if there's time.
 */
vector<string> interpret(const string &inputStr)
{

    vector<string> strings;
    //  char strings[1000][1000];
    char result[1000];
    std::ifstream inputFile(inputStr, std::ios::binary);

    if (!inputFile)
    {
        printf("ERROR OPENING FILE!");
        exit(-1);
    }
    char byte = 0;
    char sync[2];
    int packet = 0;

    string erpaLabels[13] = {"a", "b", "d", "e", "g", "1", "2", "3", "4", "5", "6", "7", "8"};
    int erpaValues[13];
    int erpaIndex = 0;
    int erpaValid = 0;

    string pmtLabels[11] = {"i", "j", "k", "1", "2", "3", "4", "5", "6", "7", "8"};
    int pmtValues[11];
    int pmtIndex = 0;
    int pmtValid = 0;

    string hkLabels[27] = {"l", "m", "n", "o", "p",
                           "q", "r", "s", "t",
                           "u", "v", "w", "x", "y", "z", "A", "B", "C", "D", "1", "2", "3", "4", "5", "6", "7", "8"};
    int hkValues[27];
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
                    snprintf(result, sizeof(result), "%s:0x%X", erpaLabels[erpaIndex].c_str(), erpaValues[erpaIndex]);
                    strings.push_back(result);
                    break;
                case 1:
                    /* SEQ Bytes; 0-65535 */
                    snprintf(result, sizeof(result), "%s:%04d", erpaLabels[erpaIndex].c_str(), erpaValues[erpaIndex]);
                    strings.push_back(result);
                    break;
                case 2:
                    /* SWP Monitored */
                    snprintf(result, sizeof(result), "%s:%06.5f", erpaLabels[erpaIndex].c_str(),
                             intToVoltage(erpaValues[erpaIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    break;
                case 3:
                    /* TEMP Op-Amp1 */
                    snprintf(result, sizeof(result), "%s:%06.5f", erpaLabels[erpaIndex].c_str(),
                             intToVoltage(erpaValues[erpaIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    break;
                case 4:
                    /* ERPA eADC Bytes; Interpreted as Volts */
                    snprintf(result, sizeof(result), "%s:%08.7f", erpaLabels[erpaIndex].c_str(),
                             intToVoltage(erpaValues[erpaIndex], 16, 5, 1.0));
                    strings.push_back(result);
                    erpaValid = false;
                    break;
                case 5:
                    /* Year YY*/
                    snprintf(result, sizeof(result), "%s:%02d", erpaLabels[erpaIndex].c_str(), byte);
                    strings.push_back(result);
                    cout << result << endl;
                    erpaValid = false;
                    break;
                case 6:
                    /* Month MM */
                    snprintf(result, sizeof(result), "%s:%02d", erpaLabels[erpaIndex].c_str(), byte);
                    strings.push_back(result);
                    cout << result << endl;
                    erpaValid = false;
                    break;
                case 7:
                    /* Day DD*/
                    snprintf(result, sizeof(result), "%s:%02d", erpaLabels[erpaIndex].c_str(), byte);
                    strings.push_back(result);
                    cout << result << endl;
                    erpaValid = false;
                    break;
                case 8:
                    /* Hour HH */
                    snprintf(result, sizeof(result), "%s:%02d", erpaLabels[erpaIndex].c_str(), byte);
                    strings.push_back(result);
                    cout << result << endl;
                    erpaValid = false;
                    break;
                case 9:
                    /* Minute MM */
                    snprintf(result, sizeof(result), "%s:%02d", erpaLabels[erpaIndex].c_str(), byte);
                    strings.push_back(result);
                    cout << result << endl;
                    erpaValid = false;
                    break;
                case 10:
                    /* Second SS */
                    snprintf(result, sizeof(result), "%s:%02d", erpaLabels[erpaIndex].c_str(), byte);
                    strings.push_back(result);
                    cout << result << endl;
                    erpaValid = false;
                    break;
                case 11:
                    /* Millisecond S */
                    // MSB
                    erpaValid = false;
                    break;
                case 12:
                    /* Millisecond S */
                    // LSB
                    snprintf(result, sizeof(result), "%s:%003d", erpaLabels[erpaIndex].c_str(), erpaValues[erpaIndex]);
                    strings.push_back(result);
                    cout << result << endl;
                    break;
                }
                erpaIndex = (erpaIndex + 1) % 13;
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
                    snprintf(result, sizeof(result), "%s:0x%X", pmtLabels[pmtIndex].c_str(), pmtValues[pmtIndex]);
                    strings.push_back(result);
                    break;
                case 1:
                    /* SYNC Bytes; 0-65535 */
                    snprintf(result, sizeof(result), "%s:%04d", pmtLabels[pmtIndex].c_str(), pmtValues[pmtIndex]);
                    strings.push_back(result);
                    break;
                case 2:
                    /* PMT eADC Bytes; Interpreted as Volts */
                    snprintf(result, sizeof(result), "%s:%08.7f", pmtLabels[pmtIndex].c_str(),
                             intToVoltage(pmtValues[pmtIndex], 16, 5, 1.0));
                    strings.push_back(result);
                    pmtValid = false;
                    break;
                case 3:
                    /* Year YY*/
                    snprintf(result, sizeof(result), "%s:%02d", pmtLabels[pmtIndex].c_str(), byte);
                    strings.push_back(result);
                    cout << result << endl;
                    pmtValid = false;
                    break;
                case 4:
                    /* Month MM */
                    snprintf(result, sizeof(result), "%s:%02d", pmtLabels[pmtIndex].c_str(), byte);
                    strings.push_back(result);
                    cout << result << endl;
                    pmtValid = false;
                    break;
                case 5:
                    /* Day DD*/
                    snprintf(result, sizeof(result), "%s:%02d", pmtLabels[pmtIndex].c_str(), byte);
                    strings.push_back(result);
                    cout << result << endl;
                    pmtValid = false;
                    break;
                case 6:
                    /* Hour HH */
                    snprintf(result, sizeof(result), "%s:%02d", pmtLabels[pmtIndex].c_str(), byte);
                    strings.push_back(result);
                    cout << result << endl;
                    pmtValid = false;
                    break;
                case 7:
                    /* Minute MM */
                    snprintf(result, sizeof(result), "%s:%02d", pmtLabels[pmtIndex].c_str(), byte);
                    strings.push_back(result);
                    cout << result << endl;
                    pmtValid = false;
                    break;
                case 8:
                    /* Second SS */
                    snprintf(result, sizeof(result), "%s:%02d", pmtLabels[pmtIndex].c_str(), byte);
                    strings.push_back(result);
                    cout << result << endl;
                    pmtValid = false;
                    break;
                case 9:
                    /* Millisecond S */
                    // MSB
                    pmtValid = false;
                    break;
                case 10:
                    /* Millisecond S */
                    // LSB
                    snprintf(result, sizeof(result), "%s:%003d", pmtLabels[pmtIndex].c_str(), pmtValues[pmtIndex]);
                    strings.push_back(result);
                    cout << result << endl;
                    break;
                }

                pmtIndex = (pmtIndex + 1) % 11;
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
                    snprintf(result, sizeof(result), "%s:0x%X ", hkLabels[hkIndex].c_str(), hkValues[hkIndex]);
                    strings.push_back(result);
                    break;
                case 1:
                    /* m SEQ Bytes; 0-65535 */
                    snprintf(result, sizeof(result), "%s:%04d ", hkLabels[hkIndex].c_str(), hkValues[hkIndex]);
                    strings.push_back(result);
                    break;
                case 2:
                    /* n vsense */
                    snprintf(result, sizeof(result), "%s:%06.5f", hkLabels[hkIndex].c_str(),
                             intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    break;
                case 3:
                    /* o vrefint */
                    snprintf(result, sizeof(result), "%s:%06.5f", hkLabels[hkIndex].c_str(),
                             intToVoltage(hkValues[hkIndex], 12, 3, 1.0));
                    strings.push_back(result);
                    break;
                case 4:
                    /* p temp1 */
                    snprintf(result, sizeof(result), "%s:%06.5f", hkLabels[hkIndex].c_str(),
                             tempsToCelsius(hkValues[hkIndex]));
                    strings.push_back(result);
                    break;
                case 5:
                    /* q temp2 */
                    snprintf(result, sizeof(result), "%s:%06.5f", hkLabels[hkIndex].c_str(),
                             tempsToCelsius(hkValues[hkIndex]));
                    strings.push_back(result);
                    break;
                case 6:
                    /* r temp3 */
                    snprintf(result, sizeof(result), "%s:%06.5f", hkLabels[hkIndex].c_str(),
                             tempsToCelsius(hkValues[hkIndex]));
                    strings.push_back(result);
                    break;
                case 7:
                    /* s temp4 */
                    snprintf(result, sizeof(result), "%s:%06.5f", hkLabels[hkIndex].c_str(),
                             tempsToCelsius(hkValues[hkIndex]));
                    strings.push_back(result);
                    break;
                case 8:
                    /* t BUS_Vmon */
                    snprintf(result, sizeof(result), "%s:%06.5f", hkLabels[hkIndex].c_str(),
                             intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    break;
                case 9:
                    /* u BUS_Imon */
                    snprintf(result, sizeof(result), "%s:%06.5f", hkLabels[hkIndex].c_str(),
                             intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    break;
                case 10:
                    /* v 2v5_mon */
                    snprintf(result, sizeof(result), "%s:%06.5f", hkLabels[hkIndex].c_str(),
                             intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    break;
                case 11:
                    /* w 3v3_mon */
                    snprintf(result, sizeof(result), "%s:%06.5f", hkLabels[hkIndex].c_str(),
                             intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    break;
                case 12:
                    /* x 5v_mon */
                    snprintf(result, sizeof(result), "%s:%06.5f", hkLabels[hkIndex].c_str(),
                             intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    break;
                case 13:
                    /* y n3v3_mon */
                    snprintf(result, sizeof(result), "%s:%06.5f", hkLabels[hkIndex].c_str(),
                             intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    break;
                case 14:
                    /* z n5v_mon */
                    snprintf(result, sizeof(result), "%s:%06.5f", hkLabels[hkIndex].c_str(),
                             intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    break;
                case 15:
                    /* A 15v_mon */
                    snprintf(result, sizeof(result), "%s:%06.5f", hkLabels[hkIndex].c_str(),
                             intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    break;
                case 16:
                    /* B 5vref_mon */
                    snprintf(result, sizeof(result), "%s:%06.5f", hkLabels[hkIndex].c_str(),
                             intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    break;
                case 17:
                    /* C n150v_mon */
                    snprintf(result, sizeof(result), "%s:%06.5f", hkLabels[hkIndex].c_str(),
                             intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    break;
                case 18:
                    /* D n800v_mon */
                    snprintf(result, sizeof(result), "%s:%06.5f", hkLabels[hkIndex].c_str(),
                             intToVoltage(hkValues[hkIndex], 12, 3.3, 1.0));
                    strings.push_back(result);
                    hkValid = false;
                    break;
                case 19:
                    /* Year YY*/
                    snprintf(result, sizeof(result), "%s:%02d", hkLabels[hkIndex].c_str(), byte);
                    strings.push_back(result);
                    cout << result << endl;
                    hkValid = false;
                    break;
                case 20:
                    /* Month MM */
                    snprintf(result, sizeof(result), "%s:%02d", hkLabels[hkIndex].c_str(), byte);
                    strings.push_back(result);
                    cout << result << endl;
                    hkValid = false;
                    break;
                case 21:
                    /* Day DD*/
                    snprintf(result, sizeof(result), "%s:%02d", hkLabels[hkIndex].c_str(), byte);
                    strings.push_back(result);
                    cout << result << endl;
                    hkValid = false;
                    break;
                case 22:
                    /* Hour HH */
                    snprintf(result, sizeof(result), "%s:%02d", hkLabels[hkIndex].c_str(), byte);
                    strings.push_back(result);
                    cout << result << endl;
                    hkValid = false;
                    break;
                case 23:
                    /* Minute MM */
                    snprintf(result, sizeof(result), "%s:%02d", hkLabels[hkIndex].c_str(), byte);
                    strings.push_back(result);
                    cout << result << endl;
                    hkValid = false;
                    break;
                case 24:
                    /* Second SS */
                    snprintf(result, sizeof(result), "%s:%02d", hkLabels[hkIndex].c_str(), byte);
                    strings.push_back(result);
                    cout << result << endl;
                    hkValid = false;
                    break;
                case 25:
                    /* Millisecond S */
                    // MSB
                    hkValid = false;
                    break;
                case 26:
                    /* Millisecond S */
                    // LSB
                    snprintf(result, sizeof(result), "%s:%003d", hkLabels[hkIndex].c_str(), hkValues[hkIndex]);
                    strings.push_back(result);
                    cout << result << endl;
                    break;
                }
                hkIndex = (hkIndex + 1) % 27;
            }
            hkValid = !hkValid;
        }
    }

    //    fclose(input);
    return strings;
}
