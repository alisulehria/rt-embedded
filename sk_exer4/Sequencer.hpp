#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <csignal>
#include <semaphore>
#include <memory>
#include <mutex>
#include <condition_variable>

// For setting CPU affinity & priority
#include <pthread.h>
#include <sched.h>

////////////////////////////////////////////
// Real-Time Statistics
//////////////////////////////////////////
struct RTStatistics
{
    // Execution time stats
    std::atomic<long long> minExecNs{std::numeric_limits<long long>::max()};
    std::atomic<long long> maxExecNs{0};
    std::atomic<long long> totalExecNs{0};
    std::atomic<long long> count{0};

    // Release jitter stats
    std::atomic<long long> minReleaseJitterNs{std::numeric_limits<long long>::max()};
    std::atomic<long long> maxReleaseJitterNs{0};
    std::atomic<long long> totalReleaseJitterNs{0};
    std::atomic<long long> releaseCount{0};

    // execution jitter stats (delta b/w consecutive execution times)
    std::atomic<long long> minExecJitterNs{std::numeric_limits<long long>::max()};
    std::atomic<long long> maxExecJitterNs{0};
    std::atomic<long long> totalExecJitterNs{0};
    std::atomic<long long> execJitterCount{0};

    long long previousExecNs{0};

    // Deadline stats
    std::atomic<long long> deadlineMissCount{0};

    void updateExecTime(long long execNs)
    {
        // min
        auto prevMin = minExecNs.load();
        while (execNs < prevMin && !minExecNs.compare_exchange_weak(prevMin, execNs))
            ; // loop

        // max
        auto prevMax = maxExecNs.load();
        while (execNs > prevMax && !maxExecNs.compare_exchange_weak(prevMax, execNs))
            ; // loop

        totalExecNs += execNs;
        count++;

        //exec jitter calc after first execution is over
        if (count > 1) {
            long long execJitter = (execNs > previousExecNs) ? (execNs - previousExecNs) : (previousExecNs - execNs);
            auto prevMinJitter = minExecJitterNs.load();
            while (execJitter < prevMinJitter && !minExecJitterNs.compare_exchange_weak(prevMinJitter, execJitter))
                ;
            auto prevMaxJitter = maxExecJitterNs.load();
            while (execJitter > prevMaxJitter && !maxExecJitterNs.compare_exchange_weak(prevMaxJitter, execJitter))
                ;
            totalExecJitterNs += execJitter;
            execJitterCount++;
        }
        previousExecNs = execNs;
    }

    void updateReleaseJitter(long long jitterNs)
    {
        if (jitterNs < 0) return; // handle negative weirdness

        auto prevMin = minReleaseJitterNs.load();
        while (jitterNs < prevMin && !minReleaseJitterNs.compare_exchange_weak(prevMin, jitterNs))
            ; // loop

        auto prevMax = maxReleaseJitterNs.load();
        while (jitterNs > prevMax && !maxReleaseJitterNs.compare_exchange_weak(prevMax, jitterNs))
            ; // loop

        totalReleaseJitterNs += jitterNs;
        releaseCount++;
    }

    void missDeadline() { deadlineMissCount++; }

    // Helpers to get final stats
    double avgExecNs() const
    {
        long long c = count.load();
        return c == 0 ? 0.0 : double(totalExecNs.load()) / double(c);
    }
    double avgReleaseJitterNs() const
    {
        long long c = releaseCount.load();
        return c == 0 ? 0.0 : double(totalReleaseJitterNs.load()) / double(c);
    }
    double avgExecJitterNs() const
    {
        long long c = execJitterCount.load();
        return c == 0 ? 0.0 : double(totalExecJitterNs.load()) / double(c);
    }
};

////////////////////////////////////////////
// Service Configuration
////////////////////////////////////////////
struct Service
{
    std::function<void()> serviceFunc;
    int priority;       // e.g. 98, 99 for RT
    int cpuAffinity;    // which CPU core to run on, or -1 for no affinity
    std::string name; 
    int periodMs;       // how often (in ms) to release
    bool keepRunning{true};

    // Use a counting semaphore for release signals
    std::counting_semaphore<1> releaseSem{0};

    // jthread for the service
    std::jthread worker;

    // Real-time stats
    RTStatistics stats;

    // For release/deadline tracking
    std::chrono::steady_clock::time_point nextRelease;
    std::chrono::steady_clock::time_point nextDeadline;
};

////////////////////////////////////////////
// Sequencer Class
////////////////////////////////////////////
class Sequencer
{
public:
    Sequencer();
    ~Sequencer();

    // Adds a service. 
    //  func = user code to run
    //  priority = e.g. 98 or 99 (SCHED_FIFO)
    //  cpuAffinity = e.g. 0 for CPU0, or -1 to disable
    //  periodMs = desired period in milliseconds
void addService(std::string name, std::function<void()> func, int priority, int cpuAffinity, int periodMs);

    // Start all services with an underlying POSIX timer that ticks at `masterIntervalMs`
    // and calls onAlarm() each time. onAlarm() will handle releasing services.
    void startServices(int masterIntervalMs);

    // Gracefully stop all services and cancel the timer
    void stopServices();

    // Called from the signal handler when SIGALRM fires
    // This checks if it’s time to release each service
    void onAlarm();

    // Print final stats
    void printStatistics();

private:
    // We store all Service objects
    std::vector<std::unique_ptr<Service>> services;

    // For POSIX timer
    timer_t timerId{nullptr};

    // This is used in the static signal handler
    static Sequencer* gInstance;

    // Setup the real-time timer for SIGALRM
    void setupTimer(int masterIntervalMs);

    // Cancel the timer
    void teardownTimer();

    // Called once by signal handler
    static void alarmHandler(int signo);

    // We also want a clean shutdown on CTRL+C
    static void sigintHandler(int signo);

    // Utility: set the affinity & priority for the calling thread
    static void setCurrentThreadAffinity(int cpuCore);
    static void setCurrentThreadPriority(int priority);

    //remember current position in sorted services list (vector?)
    size_t nextServiceIndex{0};
};

