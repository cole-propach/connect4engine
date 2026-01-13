# Compiler and flags
CXX = g++
CXXFLAGS = -O2 -std=c++17 -Wall

# Debug flags
DEBUGFLAGS = -g -O0 -std=c++17 -Wall

# Target name
TARGET = main.exe

# Source files
SRCS = main.cpp
INCS = main.h

# Default rule
all: $(TARGET)

# Normal build
$(TARGET): $(SRCS) $(INCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

# Debug build
g: $(SRCS)
	$(CXX) $(DEBUGFLAGS) -o $(TARGET) $(SRCS)
	@echo "Debug build complete: $(TARGET)"

# Clean rule
clean:
	del /Q $(TARGET) $(TARGET)
