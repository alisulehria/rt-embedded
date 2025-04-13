#include "Sequencer.hpp"
#include <iostream>
#include <chrono>
#include <thread>

static int state = 0;
/*
void fib10Work()
{
    const unsigned N = 47;
    unsigned fib0=0, fib1=1, fib=0;
    for (unsigned i=0; i<22000; i++)
    {
        for (unsigned j=0; j<N; j++)
        {
            fib = fib0 + fib1;
            fib0 = fib1;
            fib1 = fib;
        }
    }
} */

void switchState()
{
    /*/
    const unsigned N = 47;
    unsigned fib0=0, fib1=1, fib=0;
    for (unsigned i=0; i<56000; i++)
    {
        for (unsigned j=0; j<N; j++)
        {
            fib = fib0 + fib1;
            fib0 = fib1;
            fib1 = fib;
        }
    } */

    if(state == 0){
        system("sudo pinctrl set 17 dh");
        state = 1;
    }
    if(state == 1){
        system("sudo pinctrl set 17 dl");
        state = 0;
    }
}

int main()
{

    std::cout << "Starting Sequencer Demo with POSIX Timer + SIGALRM\n";

    Sequencer seq;

    // addService(func, priority, cpuAffinity, periodMs)
    seq.addService("fib10Service",switchState, /*priority=*/99, /*cpuAffinity=*/0, /*periodMs=*/100);
    //seq.addService("fib20Service",fib20Work, /*priority=*/98, /*cpuAffinity=*/1, /*periodMs=*/50);

    system("sudo pinctrl 17 op dl");

    // Master alarm ticks every 10 ms, for example.
    seq.startServices(/*masterIntervalMs=*/10);
 

    // std::this_thread::sleep_for(std::chrono::seconds(10));
    // seq.stopServices();
    // seq.printStatistics();

    while(true) { std::this_thread::sleep_for(std::chrono::seconds(1)); }
    return 0;
}
