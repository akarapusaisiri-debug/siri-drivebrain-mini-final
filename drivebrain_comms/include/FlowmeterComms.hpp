#pragma once
#include <atomic>
#include <cstdint>
#include <thread>
#include <DriverBus.hpp>

class FlowmeterComms {
public:
    FlowmeterComms();
    ~FlowmeterComms();
    uint64_t latest_pulse_count() const;

private:
    void _read_gpio_loop();
    void _handle_loop();
    void _push_pulse(uint64_t timestamp_ns);
    bool _should_stop();

    core::ThreadSafeDeque<uint64_t> _queue;
    std::atomic<uint64_t> _count{0};
    bool _stop = false;
    std::thread _reader_thread;
    std::thread _handler_thread;
};
