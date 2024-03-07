# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++11 -Wall
FLTKFLAGS = `fltk-config --cxxflags` `fltk-config --ldflags`

# Source files
SRCS = instrumentGUI.cpp

# Object files
BUILD_DIR = build
OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.o))

# Output executable
TARGET = instrumentGUI

# Clean
CLEAN = clean

# All
ALL = all

# Build target
$(ALL): $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(FLTKFLAGS) -o $(TARGET) $(OBJS)

# Compile source files
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(FLTKFLAGS) -c $< -o $@

# Clean target
$(CLEAN):
	rm -rf $(BUILD_DIR) $(TARGET)
