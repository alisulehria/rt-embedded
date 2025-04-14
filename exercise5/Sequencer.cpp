/*
 * This is a C++ version of the canonical pthread service example. It intends
 * to abstract the service management functionality and sequencing for ease
 * of use. Much of the code is left to be implemented by the student.
 *
 * Build with g++ --std=c++23 -Wall -Werror -pedantic
 * Steve Rizor 3/16/2025
 * Edited by : Sriya Garde
 */
#include <cstdint>
#include <cstdio>
#include "Sequencer.hpp"

static int state =0;

void service2() {
    //std::puts("this is service 2 implemented as a function\n");
    // system("sudo pinctrl get 17");
    if(state ==0)
    {
		system("sudo pinctrl set 17 dh");
		state=1;
	}
	else if(state==1)
	{
		system("sudo pinctrl set 17 dl");
		state =0;
	}
}

int main() {
    Sequencer sequencer{};
    
    /*sequencer.addService([]() {
        std::puts("this is service 1 implemented in a lambda expression\n");
    }, 1, 99, 500); // 500 ms period
	*/
	
	system("sudo pinctrl set 17 op dl");
    sequencer.addService(service2, 1, 98, 100); // 1000 ms period

    sequencer.startServices();
    
    // Wait for Enter key to stop services
    sequencer.waitForStopSignal();
    
    std::cout << "All services stopped. Exiting...\n";
    return 0;
}
