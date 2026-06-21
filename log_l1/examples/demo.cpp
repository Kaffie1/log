#include "log_l1.hpp"

int main() {
    naviai::log::l1::L1SdkOptions options;
    options.root_dir = "/tmp/log_l1_demo";
    options.file_name = "system.log";
    options.host_name = "demo-host";
    options.async_mode = false;

    naviai::log::l1::L1::Init(options);
    naviai::log::l1::L1::Write({naviai::log::l1::MonitorModule::Health,
                                naviai::log::LogLevel::Info,
                                "watchdog heartbeat ok",
                                "heartbeat"});
    naviai::log::l1::L1::Shutdown();
    return 0;
}
