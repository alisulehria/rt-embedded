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
#include <iostream>
#include <chrono>
#include <limits>
#include "Sequencer.hpp"

void service_toggleGPIO() {
    static int state = 0;
    int result;

    static uint64_t min_time = std::numeric_limits<uint64_t>::max();
    static uint64_t max_time = 0;
    static uint64_t total_time = 0;
    static size_t count = 0;

    auto start = std::chrono::high_resolution_clock::now();

    // GPIO toggle logic
    if (state == 0) {
        result = system("sudo pinctrl set 17 dh");
        std::cout << "GPIO low" << std::endl;
        state = 1;
    } else {
        result = system("sudo pinctrl set 17 dl");
        std::cout << "GPIO high" << std::endl;
        state = 0;
    }

    if (result != 0) {
        std::cerr << "Error toggling GPIO pin 17" << std::endl;
    }

    auto end = std::chrono::high_resolution_clock::now();
    uint64_t exec_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    ++count;
    total_time += exec_time;
    if (exec_time < min_time) min_time = exec_time;
    if (exec_time > max_time) max_time = exec_time;

    // Periodic print
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
    
    sequencer.addService(service_toggleGPIO, 1, 98, 100); 
    
    sequencer.startServices();
    
    sequencer.waitForStopSignal();
    
    std::cout << "All services stopped. Exiting...\n";
    return 0;
}
