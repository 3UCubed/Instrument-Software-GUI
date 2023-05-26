# IS-GUI
To install needed software on mac: `brew install fltk`

To compile: $g++ -std=c++11 \`fltk-config --cxxflags\` hello.cpp \`fltk-config --ldflags\` -o hello

To run: $./hello

If you get "Failed to open the serial port", run $ls /dev/cu.usbserial*

Update line 65 in hello.cpp with yours

