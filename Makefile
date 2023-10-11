# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++11 -Wall
FLTKFLAGS = `fltk-config --cxxflags` `fltk-config --ldflags`

# Source files
SRCS = instrumentGUI.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Output executable
TARGET = instrumentGUI

# Build target
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(FLTKFLAGS) -o $(TARGET) $(OBJS)

# Compile source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(FLTKFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET)
