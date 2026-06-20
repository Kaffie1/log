#include "log_agent.hpp"

#include <fstream>
#include <system_error>

#include <zlib.h>

namespace naviai::log {
namespace {

constexpr char kCompressedSuffix[] = ".gz";
constexpr char kMetaSuffix[] = ".meta";
constexpr char kSealedState[] = "sealed";

}  // namespace

LogAgentResult LogAgent::RunCompressLocked() {
    std::size_t compressed = 0;
    std::size_t pending = 0;

    for (const auto& file : state_.files) {
        if (file.file_state != kSealedState) {
            continue;
        }
        if (!ShouldCompressFile(file)) {
            continue;
        }

        auto& failure_count = compress_failures_[file.path.string()];
        if (failure_count >= policy_.compress_retry_limit) {
            continue;
        }

        ++pending;
        if (CompressFileToGzip(file.path, policy_.delete_raw_after_compress)) {
            compress_failures_.erase(file.path.string());
            ++compressed;
            continue;
        }

        ++failure_count;
    }

    state_.schedule_state.pending_compress_tasks =
        pending > compressed ? pending - compressed : 0;
    RunScanLocked();
    return {true, "compression pass completed", compressed};
}

bool LogAgent::ShouldCompressFile(const LogFileEntry& file) {
    if (file.path.extension() == kCompressedSuffix) {
        return false;
    }
    return file.path.extension() != kMetaSuffix;
}

bool LogAgent::CompressFileToGzip(const std::filesystem::path& source_path,
                                  bool delete_raw_after_compress) {
    std::error_code ec;
    if (!std::filesystem::exists(source_path, ec) ||
        source_path.extension() == kCompressedSuffix) {
        return false;
    }

    std::ifstream input(source_path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    const auto gzip_path = source_path.string() + kCompressedSuffix;
    gzFile output = gzopen(gzip_path.c_str(), "wb");
    if (output == nullptr) {
        return false;
    }

    char buffer[8192];
    bool success = true;
    while (input.good()) {
        input.read(buffer, sizeof(buffer));
        const auto count = static_cast<unsigned int>(input.gcount());
        if (count > 0 && gzwrite(output, buffer, count) != static_cast<int>(count)) {
            success = false;
            break;
        }
    }

    gzclose(output);
    input.close();
    if (!success) {
        std::filesystem::remove(gzip_path, ec);
        return false;
    }
    if (delete_raw_after_compress) {
        std::filesystem::remove(source_path, ec);
    }
    return true;
}

}  // namespace naviai::log
