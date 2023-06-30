# IS-GUI

### QUICK START
To install needed software on mac: `brew install fltk`

You need to have FTDI connected and be running code found in https://github.com/3UCubed/Instrument-Software/tree/non-experimental-firmware/Firmware

To compile: $g++ -std=c++11 \`fltk-config --cxxflags\` hello.cpp \`fltk-config --ldflags\` -o hello

To run: $./hello

If you get "Failed to open the serial port", run $ls /dev/cu.usbserial* and update line 65 in hello.cpp with yours

#


### DETAILED INSTRUCTIONS
#### Mac OS
* Install FLTK library: `brew install fltk`
* Connect ST-LINK to STM32F0 board
* Connect UART: Black on GND, Yellow on PA9, Orange on PA10
* Create two new folders on desktop, Firmware and GUI
* In terminal, navigate to your new Firmware folder
* Run command in terminal: git clone https://github.com/3UCubed/Instrument-Software.git
* Run command in same terminal: git checkout -b non-experimental-firmware origin/non-experimental-firmware
* In terminal, navigate to your new GUI folder
* Run command in terminal: git clone https://github.com/3UCubed/Instrument-Software-GUI.git
* Open your new Firmware folder with the cloned repository in the STM32CubeIDE and run it once
* CubeIDE can now be closed
* Open a new terminal and navigate to your new GUI folder
* Compile with: $g++ -std=c++11 \`fltk-config --cxxflags\` hello.cpp \`fltk-config --ldflags\` -o hello
* Run with: $./hello

#### Linux
* Install FLTK library with command: git clone https://github.com/fltk/fltk.git
* Follow readme at this link to configure and build FLTK for the first time on unix/linux: https://github.com/fltk/fltk/blob/master/README.Unix.txt
* TODO: CREATE NEW BRANCH IN GUI REPO FOR LINUX BUILD


### NOTE
You can not turn on the other GPIO's unless PB5 (tied to SYS_ON) is toggled on. This is by design and purposeful.
