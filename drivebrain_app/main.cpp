#include <FlowmeterComms.hpp>
#include <FoxgloveServer.hpp>
#include <MCAPLogger.hpp>
#include <drivebrain_msgs.pb.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>
#include <fmt/chrono.h>
#include <spdlog/spdlog.h>

std::atomic<bool> running{true};

void sig_handler(int signal) {
    if(signal == SIGINT || signal == SIGTERM) {
      spdlog::warn("Interrupted, stopping mini-drivebrain");
      running = false;
    }
}

int main() {
  std::signal(SIGINT, sig_handler);
  std::signal(SIGTERM, sig_handler);

  core::MCAPLogger::create(mcap::McapWriterOptions(""));
  core::FoxgloveServer::create();

  core::MCAPLogger::instance().open_new_mcap();
  core::MCAPLogger::instance().init_logging();

  spdlog::info("Constructed logging singletons");

  {
    FlowmeterComms flowmeter;

    auto loop_time = 0.004;
    std::chrono::microseconds loop_time_ms((int) (loop_time * 1000000.0f));
    auto next_tick = std::chrono::steady_clock::now();
    uint64_t tick_count = 0;
    while(running) {

      next_tick += loop_time_ms;
      tick_count++;
      if (tick_count % 250 == 0) {
        auto msg = std::make_shared<drivebrain_msgs::FlowmeterData>();
        msg->set_pulse_count(flowmeter.latest_pulse_count());
        msg->set_timestamp_ns(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        core::MCAPLogger::instance().log_msg(msg);
        core::FoxgloveServer::instance().send_live_telem_msg(msg);
      }
      auto now = std::chrono::steady_clock::now();
      if(now > next_tick) {
        spdlog::warn("Loop overrun by {}", now-next_tick);
        next_tick = now;
      }

      std::this_thread::sleep_until(next_tick);
    }
  }

  core::MCAPLogger::instance().destroy();
  core::FoxgloveServer::instance().destroy();
  spdlog::info("Logging singletons safely destroyed");

  return 0;
}
