# Compiler and flags
CXX = g++
CXXFLAGS = -O2 -std=c++17 -Wall

# Target name
TARGET = main.exe

# Source files
SRCS = main.cpp

# Default rule
all: $(TARGET)

# Link and compile
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

# Clean rule
clean:
	del /Q $(TARGET)
