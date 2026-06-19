CXX = g++
CXXFLAGS = -Wall -O2 -std=c++17
TARGET = brutepin-cpp

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)

install: $(TARGET)
	cp $(TARGET) ../

.PHONY: all clean install
