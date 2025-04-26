/*
 * This is a C++ version of the canonical pthread service example. It intends
 * to abstract the service management functionality and sequencing for ease
 * of use. Much of the code is left to be implemented by the student.
 *
 * Build with g++ --std=c++23 -Wall -Werror -pedantic
 * Steve Rizor 3/16/2025
 */

#pragma once

#include <cstdint>
#include <functional>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>

class Service {
public:
    Service(std::function<void(void)> doService, uint8_t affinity, uint8_t priority, uint32_t period) :
        _doService(std::move(doService)),
        _running(true),
        _affinity(affinity),
        _priority(priority),
        _period(period)
    {
        _service = std::thread(&Service::_provideService, this);
    }

    ~Service() {
        stop();
        if (_service.joinable()) {
            _service.join();
        }
    }

    void stop() {
        _running.store(false);
        _cv.notify_all();
    }

    void release() {
        _cv.notify_one();
    }

private:
    std::function<void(void)> _doService;
    std::thread _service;
    std::atomic<bool> _running;
    std::condition_variable _cv;
    std::mutex _mutex;
    uint8_t _affinity;
    uint8_t _priority;
    uint32_t _period;

    void _initializeService() {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(_affinity, &cpuset);
        pthread_t thread = _service.native_handle();
        pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
        
        struct sched_param sch_params;
        sch_params.sched_priority = _priority;
        pthread_setschedparam(thread, SCHED_FIFO, &sch_params);
    }

    void _provideService() {
        _initializeService();
        while (_running.load()) {
            std::unique_lock<std::mutex> lock(_mutex);
            _cv.wait_for(lock, std::chrono::milliseconds(_period), [&]() { return !_running.load(); });
            if (_running.load()) {
                _doService();
            }
        }
    }
};

class Sequencer {
public:
    template<typename... Args>
    void addService(Args&&... args) {
        _services.emplace_back(std::make_unique<Service>(std::forward<Args>(args)...));
    }

    void startServices() {
        for (auto& service : _services) {
            service->release();
        }
    }

    void stopServices() {
        for (auto& service : _services) {
            service->stop();
        }
    }

    void waitForStopSignal() {
        std::cout << "Services running. Press Enter to stop...\n";
        std::cin.ignore(); // Wait for Enter key
        stopServices();
    }

private:
    std::vector<std::unique_ptr<Service>> _services;
};
