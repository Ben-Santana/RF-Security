CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
LIBS = -lrtlsdr -lSDL2 -lSDL2_ttf -lfftw3f -lm
TARGET = simple_sdr
SOURCES = main.cpp sdr.cpp gui.cpp protocol_analyzer.cpp
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
