#include "log_agent.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace {

std::size_t CountCompressedFiles(const naviai::log::LogAgentState& state) {
    std::size_t count = 0;
    for (const auto& file : state.files) {
        if (file.path.extension() == ".gz") {
            ++count;
        }
    }
    return count;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string root_dir =
        argc > 1 ? argv[1] : "/var/log/robot/l2";

    naviai::log::FileGovernPolicy policy;
    policy.scan_interval_ms = 500;
    policy.cleanup_interval_ms = 60 * 1000;
    policy.delete_raw_after_compress = true;

    naviai::log::LogAgent agent(root_dir, policy);
    const auto start_result = agent.Start();
    std::cout << "start: " << start_result.success << " " << start_result.message
              << '\n';

    std::this_thread::sleep_for(std::chrono::seconds(2));

    const auto compress_result = agent.CompressNow();
    std::cout << "compress: " << compress_result.success << " "
              << compress_result.message << " affected="
              << compress_result.affected_files << '\n';

    const auto state = agent.GetState();
    std::cout << "files_total: " << state.stats.total_files << '\n';
    std::cout << "sealed_files: " << state.stats.sealed_files << '\n';
    std::cout << "compressed_files: " << state.stats.compressed_files << '\n';
    std::cout << "gz_files: " << CountCompressedFiles(state) << '\n';

    const auto stop_result = agent.Stop(true);
    std::cout << "stop: " << stop_result.success << " " << stop_result.message
              << '\n';
    return start_result.success ? 0 : 1;
}
