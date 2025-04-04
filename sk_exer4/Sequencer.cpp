#include "Sequencer.hpp"
#include <cstring>    // memset
#include <cerrno>
#include <unistd.h>   // usleep

Sequencer* Sequencer::gInstance = nullptr;

////////////////////////////////////////////
// Constructors / Destructors
////////////////////////////////////////////


Sequencer::Sequencer()
{
     // Make this instance globally accessible for static signal handler
    gInstance = this;
}

Sequencer::~Sequencer()
{
    // Ensure timer is torn down
    stopServices();
    // Unset the global pointer
    gInstance = nullptr;
}

////////////////////////////////////////////
// Public API
////////////////////////////////////////////

void Sequencer::addService(std::string name, std::function<void()> func, int priority, int cpuAffinity, int periodMs)
{
    auto svc = std::make_unique<Service>();
    svc->serviceFunc = std::move(func);
    svc->name = std::move(name);
    svc->priority = priority;
    svc->cpuAffinity = cpuAffinity;
    svc->periodMs = periodMs;

    // The jthread constructor spawns the thread immediately. We'll store it in the Service struct.
    svc->worker = std::jthread([svcPtr = svc.get()] {
        // Set thread affinity / priority
        setCurrentThreadAffinity(svcPtr->cpuAffinity);
        setCurrentThreadPriority(svcPtr->priority);

        // Keep running until told otherwise
        while (svcPtr->keepRunning)
        {
            // Wait for release
            svcPtr->releaseSem.acquire();

            if (!svcPtr->keepRunning) break;

            // Mark release time
            auto releaseTime = std::chrono::steady_clock::now();

            // Calculate release jitter vs. planned nextRelease
            auto relJitterNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                    releaseTime - svcPtr->nextRelease).count();
            svcPtr->stats.updateReleaseJitter(relJitterNs < 0 ? 0 : relJitterNs);

            // Run the service function
            auto startTime = std::chrono::steady_clock::now();
            svcPtr->serviceFunc();
            auto endTime = std::chrono::steady_clock::now();

            // Execution time
            auto execTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                  endTime - startTime).count();
            svcPtr->stats.updateExecTime(execTimeNs);

            // Check for deadline miss
            if (endTime > svcPtr->nextDeadline)
            {
                svcPtr->stats.missDeadline();
            }
        }
    });

    services.push_back(std::move(svc));
}

void Sequencer::startServices(int masterIntervalMs)
{
    // sort services by period, low to high
    std::sort(services.begin(), services.end(), [](const std::unique_ptr<Service>& a, const std::unique_ptr<Service>& b) {
        return a->periodMs < b->periodMs;
    });

    // Reset "nextRelease" for each service to "now"
    auto now = std::chrono::steady_clock::now();
    for (auto &svc : services)
    {
        svc->nextRelease = now; 
        // If you want each service to have a distinct nextDeadline = now + period, do:
        svc->nextDeadline = now + std::chrono::milliseconds(svc->periodMs);
    }

    // Setup signals and timer
    setupTimer(masterIntervalMs);

    // Also handle Ctrl+C gracefully
    struct sigaction saInt;
    memset(&saInt, 0, sizeof(saInt));
    saInt.sa_handler = Sequencer::sigintHandler;
    sigaction(SIGINT, &saInt, nullptr);
}

void Sequencer::stopServices()
{
    // Cancel timer
    teardownTimer();

    // Mark keepRunning = false, release all semaphores
    for (auto &svc : services)
    {
        svc->keepRunning = false;
        svc->releaseSem.release(); // unblock the thread
    }

    // Join all jthreads
    for (auto &svc : services)
    {
        if (svc->worker.joinable())
        {
            svc->worker.join();
        }
    }
}

void Sequencer::onAlarm()
{
    // This is called each time SIGALRM fires
    auto now = std::chrono::steady_clock::now();
    size_t count = services.size();

    for (size_t i=0; i < count; i++)  //auto &svc : services)
    {
        // initialize next starting service idx and svc object
        size_t idx = (nextServiceIndex + i) % count;
        Service* svc = services[idx].get();

        // Check if time to release
        if (now >= svc->nextRelease)
        {
            // Release the service
            svc->releaseSem.release();

            // Update nextRelease
            svc->nextRelease += std::chrono::milliseconds(svc->periodMs);
            // If we fell behind, we might need to keep pushing nextRelease forward
            while (now >= svc->nextRelease)
            {
                svc->nextRelease += std::chrono::milliseconds(svc->periodMs);
            }

            // Update nextDeadline
            svc->nextDeadline = svc->nextRelease
                                + std::chrono::milliseconds(svc->periodMs);
        }
        else {
            // when we find a svc that's not due, we save it as the next starting pt.
            nextServiceIndex = idx;
            break;
        }
    }
}

void Sequencer::printStatistics()
{
    std::cout << "\n===== Final Statistics =====\n";
    for (auto &svc : services)
    {
        auto &st = svc->stats;
        std::cout << svc->name << ":\n"
                  << "   ExecTime:   min=" << st.minExecNs.load() / 1e6 << " ms, "
                  << "max=" << st.maxExecNs.load() / 1e6 << " ms, "
                  << "avg=" << st.avgExecNs() / 1e6 << " ms\n"
                  << "   ExecJitter: min=" << st.minExecJitterNs.load() / 1e6 << " ms, "
                  << "max=" << st.maxExecJitterNs.load() / 1e6 << " ms, "
                  << "avg=" << st.avgExecJitterNs() / 1e6 << " ms\n"
                  << "   ReleaseJit: min=" << st.minReleaseJitterNs.load() / 1e6 << " ms, "
                  << "max=" << st.maxReleaseJitterNs.load() / 1e6 << " ms, "
                  << "avg=" << st.avgReleaseJitterNs() / 1e6 << " ms\n"
                  << "   Deadline Misses=" << st.deadlineMissCount.load() << "\n";
    }
    std::cout << "============================\n\n";
}

////////////////////////////////////////////
// Private / static
////////////////////////////////////////////

void Sequencer::setupTimer(int masterIntervalMs)
{
    // We use SIGALRM for the periodic timer
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = Sequencer::alarmHandler;
    sigaction(SIGALRM, &sa, nullptr);

    // Create POSIX timer
    sigevent sev{};
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGALRM;

    if (timer_create(CLOCK_REALTIME, &sev, &timerId) < 0)
    {
        std::cerr << "timer_create error: " << strerror(errno) << "\n";
        return;
    }

    // Start periodic timer
    itimerspec its{};
    // initial expiration
    its.it_value.tv_sec = masterIntervalMs / 1000;
    its.it_value.tv_nsec = (masterIntervalMs % 1000) * 1000000;
    // interval for periodic
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;

    if (timer_settime(timerId, 0, &its, nullptr) < 0)
    {
        std::cerr << "timer_settime error: " << strerror(errno) << "\n";
    }
}

void Sequencer::teardownTimer()
{
    if (timerId != nullptr)
    {
        timer_delete(timerId);
        timerId = nullptr;
    }
}

void Sequencer::alarmHandler(int signo)
{
    if (signo == SIGALRM && gInstance)
    {
        gInstance->onAlarm();
    }
}

void Sequencer::sigintHandler(int signo)
{
    if (signo == SIGINT && gInstance)
    {
        std::cout << "\nSIGINT received -> Stopping services...\n";
        gInstance->stopServices();
        // Pint final stats
        gInstance->printStatistics();
        // Then exit
        std::_Exit(EXIT_SUCCESS);
    }
}

void Sequencer::setCurrentThreadAffinity(int cpuCore)
{
    if (cpuCore < 0) return;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpuCore, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

void Sequencer::setCurrentThreadPriority(int priority)
{
    // SCHED_FIFO 
    sched_param sch_params;
    sch_params.sched_priority = priority;
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sch_params);
}
