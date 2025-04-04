# Sample Makefile
# Adjust file names as needed, e.g. if you split them differently:
#   - Sequencer.cpp
#   - main.cpp

CXX = g++
CXXFLAGS = -std=c++20 -Wall -Werror -pedantic
LDFLAGS = -pthread

TARGET = SequencerDemo

SRCS = Sequencer.cpp main.cpp  # Or however your source is split
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp Sequencer.hpp
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET)
