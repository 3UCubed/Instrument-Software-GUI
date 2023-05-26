# IS-GUI
To install needed software on mac: `brew install fltk`

You need to have FTDI connected and be running code found in https://github.com/3UCubed/Instrument-Software/tree/non-experimental-firmware/Firmware

To compile: $g++ -std=c++11 \`fltk-config --cxxflags\` hello.cpp \`fltk-config --ldflags\` -o hello

To run: $./hello

If you get "Failed to open the serial port", run $ls /dev/cu.usbserial* and update line 65 in hello.cpp with yours

