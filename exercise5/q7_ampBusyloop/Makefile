CXX = g++
CXXFLAGS = -std=c++17 -Wall -Werror -pedantic

all: Sequencer

Sequencer: Sequencer.o
	$(CXX) -o Sequencer Sequencer.o -lpthread

Sequencer.o: Sequencer.cpp Sequencer.hpp
	$(CXX) $(CXXFLAGS) -c Sequencer.cpp

clean:
	rm -f Sequencer Sequencer.o