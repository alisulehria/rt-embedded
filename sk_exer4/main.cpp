#include "Sequencer.hpp"
#include <iostream>
#include <chrono>
#include <thread>

void fib10Work()
{
    const unsigned N = 47;
    unsigned fib0=0, fib1=1, fib=0;
    for (unsigned i=0; i<170000; i++)
    {
        for (unsigned j=0; j<N; j++)
        {
            fib = fib0 + fib1;
            fib0 = fib1;
            fib1 = fib;
        }
    }
}

void fib20Work()
{
    const unsigned N = 47;
    unsigned fib0=0, fib1=1, fib=0;
    for (unsigned i=0; i<340000; i++)
    {
        for (unsigned j=0; j<N; j++)
        {
            fib = fib0 + fib1;
            fib0 = fib1;
            fib1 = fib;
        }
    }
}

int main()
{
    std::cout << "Starting Sequencer Demo with POSIX Timer + SIGALRM\n";

    Sequencer seq;

    // addService(func, priority, cpuAffinity, periodMs)
    seq.addService("fib10Service",fib10Work, /*priority=*/99, /*cpuAffinity=*/0, /*periodMs=*/20);
    seq.addService("fib20Service",fib20Work, /*priority=*/98, /*cpuAffinity=*/1, /*periodMs=*/50);

    // Master alarm ticks every 10 ms, for example.
    seq.startServices(/*masterIntervalMs=*/10);


    // std::this_thread::sleep_for(std::chrono::seconds(10));
    // seq.stopServices();
    // seq.printStatistics();

    while(true) { std::this_thread::sleep_for(std::chrono::seconds(1)); }
    return 0;
}
