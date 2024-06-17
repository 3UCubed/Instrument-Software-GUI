# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++11 -Wall
FLTK_CXXFLAGS = `fltk-config --cxxflags`
FLTK_LDFLAGS = `fltk-config --ldflags`

# Source files
SRCS = src/instrumentGUI.cpp src/doubleBuffer.cpp src/logger.cpp

# Object files
BUILD_DIR = build
OBJS = $(SRCS:src/%.cpp=$(BUILD_DIR)/%.o)

# Output executable
TARGET = instrumentGUI

# Clean
CLEAN = clean

# All
ALL = all

# Build target
$(ALL): $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(FLTK_LDFLAGS) -o $(TARGET)

# Compile source files
$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(FLTK_CXXFLAGS) -c $< -o $@

# Clean target
$(CLEAN):
	rm -rf $(BUILD_DIR) $(TARGET)
