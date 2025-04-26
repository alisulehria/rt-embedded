/*
 * This is a C++ version of the canonical pthread service example. It intends
 * to abstract the service management functionality and sequencing for ease
 * of use. Much of the code is left to be implemented by the student.
 *
 * Build with g++ --std=c++23 -Wall -Werror -pedantic
 * Steve Rizor 3/16/2025
 */
 
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include "Sequencer.hpp"

std::atomic<bool> runBusyLoop{true};
#define GPIO27_INPUT 539

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

int readGPIOValue(int gpio) {
    std::string path = "/sys/class/gpio/gpio" + std::to_string(gpio) + "/value";
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to read GPIO " << gpio << std::endl;
        return -1;
    }

    std::string value;
    std::getline(file, value);
    file.close();

    return (value == "1") ? 1 : 0;
}

void busyLoopToggleService() {
    static int last_input_state = -1;
    static int output_state = 0;

    while (runBusyLoop.load()) {
        int input_state = readGPIOValue(GPIO27_INPUT);
        if (input_state == -1) continue;

        if (input_state != last_input_state) {
            last_input_state = input_state;

            std::string cmd;
            if (output_state == 0) {
                cmd = "sudo pinctrl set 17 dh";
                output_state = 1;
                std::cout << "Toggling output: HIGH\n";
            } else {
                cmd = "sudo pinctrl set 17 dl";
                output_state = 0;
                std::cout << "Toggling output: LOW\n";
            }
            int result = system(cmd.c_str());
            if (result != 0) {
                std::cerr << "Error toggling GPIO 17\n";
            }
        }

        // usleep(100); 
    }
}

int main() {
    Sequencer sequencer{};

    // Initialize GPIO 17 as output, low
    int init = system("sudo pinctrl set 17 op dl");
    if (init != 0) {
        std::cerr << "Failed to initialize GPIO 17\n";
    }
    
    exportGPIO(GPIO27_INPUT);
    setGPIODirection(GPIO27_INPUT, "in");
    sequencer.addService(busyLoopToggleService, 1, 98, 100);

    sequencer.startServices();
    sequencer.waitForStopSignal();
    
    runBusyLoop.store(false);
    std::cout << "All services stopped. Exiting...\n";
    return 0;
}
