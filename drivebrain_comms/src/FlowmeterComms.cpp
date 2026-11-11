#include <FlowmeterComms.hpp>

#include <deque>
#include <fcntl.h>
#include <linux/gpio.h>
#include <mutex>
#include <poll.h>
#include <spdlog/spdlog.h>
#include <sys/ioctl.h>
#include <unistd.h>

static constexpr const char* GPIO_CHIP = "/dev/gpiochip0";
static constexpr unsigned int GPIO_LINE = 12;

FlowmeterComms::FlowmeterComms() {
    _reader_thread = std::thread(&FlowmeterComms::_read_gpio_loop, this);
    _handler_thread = std::thread(&FlowmeterComms::_handle_loop, this);
}

FlowmeterComms::~FlowmeterComms() {
    {
        std::lock_guard<std::mutex> lock(_queue.mtx);
        _stop = true;
    }
    _queue.cv.notify_all();
    _reader_thread.join();
    _handler_thread.join();
}

uint64_t FlowmeterComms::latest_pulse_count() const {
    return _count;
}

bool FlowmeterComms::_should_stop() {
    std::lock_guard<std::mutex> lock(_queue.mtx);
    return _stop;
}

void FlowmeterComms::_push_pulse(uint64_t timestamp_ns) {
    {
        std::lock_guard<std::mutex> lock(_queue.mtx);
        _queue.deque.push_back(timestamp_ns);
    }
    _queue.cv.notify_one();
}

void FlowmeterComms::_read_gpio_loop() {
    int chip_fd = open(GPIO_CHIP, O_RDONLY);
    if (chip_fd < 0) {
        spdlog::error("Could not open {}", GPIO_CHIP);
        return;
    }

    gpio_v2_line_request request{};
    request.offsets[0] = GPIO_LINE;
    request.num_lines = 1;
    request.config.flags = GPIO_V2_LINE_FLAG_INPUT | GPIO_V2_LINE_FLAG_EDGE_RISING |
                           GPIO_V2_LINE_FLAG_BIAS_PULL_UP;
    int result = ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &request);
    close(chip_fd);
    if (result < 0) {
        spdlog::error("Could not request line {} on {}", GPIO_LINE, GPIO_CHIP);
        return;
    }

    pollfd pin{request.fd, POLLIN, 0};
    while (!_should_stop()) {
        if (poll(&pin, 1, 100) <= 0) continue;
        gpio_v2_line_event event;
        if (read(request.fd, &event, sizeof(event)) != sizeof(event)) continue;
        _push_pulse(event.timestamp_ns);
    }
    close(request.fd);
}

void FlowmeterComms::_handle_loop() {
    std::deque<uint64_t> local;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(_queue.mtx);
            _queue.cv.wait(lock, [this] { return !_queue.deque.empty() || _stop; });
            if (_stop && _queue.deque.empty()) break;
            local.swap(_queue.deque);
        }
        _count += local.size();
        local.clear();
    }
}
