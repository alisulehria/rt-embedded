/*
 * This is a C++ version of the canonical pthread service example. It intends
 * to abstract the service management functionality and sequencing for ease
 * of use. Much of the code is left to be implemented by the student.
 *
 * Build with g++ --std=c++23 -Wall -Werror -pedantic
 * Steve Rizor 3/16/2025
 */
#include <cstdint>
#include <cstdio>
#include "Sequencer.hpp"

#include <fstream>
#include <iostream>
#include <unistd.h> 
#include <string>

#include <chrono>
#include <limits>

#define GPIO_17 529

void exportGPIO(int gpio) {
    std::ofstream exportFile("/sys/class/gpio/export");
    if (exportFile.is_open()) {
        exportFile << gpio;
        exportFile.close();
	std::cout<<" Exported file "<<std::endl;

    } else {
        std::cerr << "Error: Unable to export GPIO" << gpio << std::endl;
    }
}

void setGPIODirection(int gpio, const std::string& direction) {
    std::ofstream dirFile("/sys/class/gpio/gpio" + std::to_string(gpio) + "/direction");
    if (dirFile.is_open()) {
        dirFile << direction;
        dirFile.close();
	std::cout<<" set output Direction  "<<std::endl;
    } else {
        std::cerr << "Error: Unable to set direction for GPIO" << gpio << std::endl;
    }
}

void writeGPIOValue(int gpio, int value) {
    std::ofstream valueFile("/sys/class/gpio/gpio" + std::to_string(gpio) + "/value");
    if (valueFile.is_open()) {
        valueFile << value;
        valueFile.close();
	std::cout<<" GPIO value: "<<value<<std::endl;
    } else {
        std::cerr << "Error: Unable to write value for GPIO" << gpio << std::endl;
    }
}

void toggling_service() {
    static uint64_t min_time = std::numeric_limits<uint64_t>::max();
    static uint64_t max_time = 0;
    static uint64_t total_time = 0;
    static size_t count = 0;

    auto start = std::chrono::high_resolution_clock::now();

    static bool initialized = false;
    static bool stateLow = true;
    static const int gpio = GPIO_17;

    if (!initialized) {
        exportGPIO(gpio);
        setGPIODirection(gpio, "out");
        initialized = true;
    }

    writeGPIOValue(gpio, stateLow ? 1 : 0);
    stateLow = !stateLow;

    auto end = std::chrono::high_resolution_clock::now();
    uint64_t exec_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    ++count;
    total_time += exec_time;
    if (exec_time < min_time) min_time = exec_time;
    if (exec_time > max_time) max_time = exec_time;

    if (count % 10 == 0) {
        std::cout << "--- Timing Stats (µs) after " << count << " runs ---\n";
        std::cout << "Min: " << min_time << " | Max: " << max_time
                  << " | Avg: " << (total_time / count)
                  << " | Jitter: " << (max_time - min_time) << "\n";
    }
}

int main() {
    Sequencer sequencer{};
	
    int initalState = system("sudo pinctrl set 17 op dl");
    if (initalState != 0) {
        std::cerr << "Error toggling GPIO pin 17" << std::endl;
    }
    
    sequencer.addService(toggling_service, 1, 98, 100); 
    
    sequencer.startServices();
    
    sequencer.waitForStopSignal();
    
    std::cout << "All services stopped. Exiting...\n";
    return 0;
}
