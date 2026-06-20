#include "sink_assembler.hpp"
#include "error_handler.hpp"

#include <filesystem>
#include <stdexcept>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace naviai::log {
namespace {

std::filesystem::path AppendBeforeExtension(const std::filesystem::path& path,
                                            const std::string& suffix) {
    const auto stem = path.stem().string();
    const auto ext = path.extension().string();
    return path.parent_path() / (stem + suffix + ext);
}

}  // namespace

std::vector<spdlog::sink_ptr> SinkAssembler::Build(const LoggerConfig& config) {
    if (config.enable_basic_file_sink || config.enable_rotating_file_sink) {
        if (config.file_name.empty()) {
            throw std::runtime_error("log file name must not be empty");
        }
    }
    if (!config.enable_basic_file_sink && !config.enable_rotating_file_sink &&
        !config.enable_console_sink) {
        throw std::runtime_error("at least one sink must be enabled");
    }
    if (config.enable_rotating_file_sink && config.max_file_size_bytes == 0) {
        throw std::runtime_error("max_file_size_bytes must be greater than 0");
    }
    if (config.enable_rotating_file_sink && config.max_files == 0) {
        throw std::runtime_error("max_files must be greater than 0");
    }

    std::vector<spdlog::sink_ptr> sinks;
    if (config.enable_basic_file_sink) {
        sinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            BuildBasicFilePath(config), false));
    }
    if (config.enable_rotating_file_sink) {
        sinks.emplace_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            BuildRotatingFilePath(config),
            config.max_file_size_bytes,
            config.max_files,
            false));
    }
    if (config.enable_console_sink) {
        sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }
    ApplyErrorHandler(sinks);
    return sinks;
}

void SinkAssembler::ApplyErrorHandler(const std::vector<spdlog::sink_ptr>& sinks) {
    (void)sinks;
}

std::string SinkAssembler::BuildBasicFilePath(const LoggerConfig& config) {
    const std::filesystem::path path =
        std::filesystem::path(config.root_dir) / config.file_name;
    if (config.enable_rotating_file_sink) {
        return AppendBeforeExtension(path, ".basic").string();
    }
    return path.string();
}

std::string SinkAssembler::BuildRotatingFilePath(const LoggerConfig& config) {
    const std::filesystem::path path =
        std::filesystem::path(config.root_dir) / config.file_name;
    if (config.enable_basic_file_sink) {
        return AppendBeforeExtension(path, ".rotating").string();
    }
    return path.string();
}

}  // namespace naviai::log
