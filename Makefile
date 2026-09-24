# HW1-OpenAL-Game
# Requires libopenal-dev:  sudo apt-get install libopenal-dev

CXX      ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -O2
LDLIBS   := -lopenal -lpthread

TARGET := main
SRCS   := main.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS) $(LDLIBS)

# Run from the repo root: the sound paths in main.cpp are relative to it.
run: $(TARGET)
	./$(TARGET)

clean:
	$(RM) $(TARGET)

.PHONY: all run clean
