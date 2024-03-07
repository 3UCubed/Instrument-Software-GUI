# IS-GUI

## Standard procedure for updating the main branch ##
This main branch will now act as the production branch for both repositories. So now the standard operating procedure is that if you need to just test the most up to date firmware and GUI on the F0 use the main branch for both. In the future, if we need to add a feature that will go into production, you will create a feature branch from the main branch and merge the feature back into main when the feature is complete (and you have ensured that this doesn't break the production branch). The process I️ follow for this, in order to not break production, is to:

i). have the most up to date main branch and your feature branch
ii). create a branch off of main called (merge-featureX-to-main)
iii). git merge your feature branch into merge-featureX-to-main
iv). resolve any merge conflicts
v). git merge merge-featureX-to-main into the production branch (once you have resolved any issues)
vi). finally push your local main branch changes to the remote main branch

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
