#include "file_lifecycle.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <vector>

#include <zlib.h>

namespace naviai::log_module {

namespace {

constexpr std::time_t kRetentionWindowSeconds = 48LL * 60LL * 60LL;

std::string StripCompressionAndExtension(std::string name) {
    if (name.size() > 3 && name.substr(name.size() - 3) == ".gz") {
        name.resize(name.size() - 3);
    }
    const auto dot_pos = name.rfind('.');
    if (dot_pos != std::string::npos) {
        name.resize(dot_pos);
    }
    return name;
}

}  // namespace

std::string FileNamingPolicy::FormatTimeNow() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    localtime_r(&time, &tm_buf);

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
    return oss.str();
}

bool FileNamingPolicy::TryParseStartTime(const std::filesystem::path& path,
                                         std::time_t* start_time) {
    if (start_time == nullptr) {
        return false;
    }

    std::string stem = StripCompressionAndExtension(path.filename().string());
    const auto dash_pos = stem.find('-');
    if (dash_pos == std::string::npos) {
        return false;
    }

    std::string start_text = stem.substr(0, dash_pos);
    const auto underscore_pos = start_text.rfind('_');
    if (underscore_pos != std::string::npos &&
        underscore_pos >= 15) {
        start_text = start_text.substr(underscore_pos - 15, 15);
    }

    std::tm tm_buf{};
    std::istringstream stream(start_text);
    stream >> std::get_time(&tm_buf, "%Y%m%d_%H%M%S");
    if (stream.fail()) {
        return false;
    }
    *start_time = std::mktime(&tm_buf);
    return *start_time > 0;
}

bool IsEpochYearStartTime(std::time_t start_time) {
    if (start_time <= 0) {
        return false;
    }
    std::tm tm_buf{};
    localtime_r(&start_time, &tm_buf);
    return (tm_buf.tm_year + 1900) == 1970;
}

std::string FileNamingPolicy::BuildFileName(const std::string& start_time,
                                            const std::string& end_time,
                                            const std::string& suffix,
                                            const std::string& prefix) {
    if (prefix.empty()) {
        return start_time + "-" + end_time + suffix;
    }
    return prefix + "_" + start_time + "-" + end_time + suffix;
}

RotationManager::RotationManager(FileRotationPolicy policy)
    : policy_(policy) {}

bool RotationManager::ShouldRotate(size_t current_size_bytes,
                                   size_t next_write_size) const {
    return current_size_bytes > 0 &&
           current_size_bytes + next_write_size > policy_.max_file_size_bytes;
}

CompressionWorker::CompressionWorker()
    : worker_([this]() { WorkerLoop(); }) {}

CompressionWorker::~CompressionWorker() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void CompressionWorker::Enqueue(const std::filesystem::path& source_path) const {
    if (source_path.empty()) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_paths_.push(source_path);
    }
    cv_.notify_one();
}

void CompressionWorker::WorkerLoop() {
    while (true) {
        std::filesystem::path source_path;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return !running_ || !pending_paths_.empty(); });
            if (!running_ && pending_paths_.empty()) {
                return;
            }
            source_path = std::move(pending_paths_.front());
            pending_paths_.pop();
        }
        CompressFile(source_path);
    }
}

bool CompressionWorker::CompressFile(const std::filesystem::path& source_path) {
    std::error_code ec;
    if (!std::filesystem::exists(source_path, ec) ||
        source_path.extension() == ".gz") {
        return false;
    }

    std::ifstream input(source_path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    const auto gz_path = source_path.string() + ".gz";
    gzFile output = gzopen(gz_path.c_str(), "wb");
    if (output == nullptr) {
        return false;
    }

    char buffer[8192];
    while (input.good()) {
        input.read(buffer, sizeof(buffer));
        const auto bytes = static_cast<unsigned int>(input.gcount());
        if (bytes > 0 && gzwrite(output, buffer, bytes) != static_cast<int>(bytes)) {
            gzclose(output);
            std::filesystem::remove(gz_path, ec);
            return false;
        }
    }

    gzclose(output);
    input.close();
    std::filesystem::remove(source_path, ec);
    return !ec;
}

RetentionManager::RetentionManager(size_t max_files) : max_files_(max_files) {}

void RetentionManager::Cleanup(const std::filesystem::path& directory,
                               const std::filesystem::path& active_file) const {
    std::error_code ec;
    std::filesystem::directory_iterator iter(directory, ec);
    if (ec) {
        return;
    }

    std::vector<std::filesystem::path> files;
    const std::time_t now =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const std::time_t cutoff = now - kRetentionWindowSeconds;
    for (const auto& entry : iter) {
        std::error_code entry_ec;
        if (entry.is_regular_file(entry_ec)) {
            files.push_back(entry.path());
        }
    }

    std::sort(files.begin(), files.end());

    for (auto it = files.begin(); it != files.end();) {
        if (*it == active_file) {
            ++it;
            continue;
        }
        std::time_t start_time = 0;
        if (!FileNamingPolicy::TryParseStartTime(*it, &start_time) ||
            IsEpochYearStartTime(start_time) ||
            start_time > cutoff) {
            ++it;
            continue;
        }
        std::filesystem::remove(*it, ec);
        it = files.erase(it);
    }

    while (files.size() > max_files_) {
        const auto oldest = files.front();
        if (oldest == active_file) {
            files.erase(files.begin());
            continue;
        }
        std::filesystem::remove(oldest, ec);
        files.erase(files.begin());
    }
}

FileLifecycleManager::FileLifecycleManager(FileRotationPolicy policy,
                                           std::filesystem::path directory,
                                           std::string file_prefix)
    : policy_(policy),
      directory_(std::move(directory)),
      file_prefix_(std::move(file_prefix)),
      rotation_manager_(policy_),
      retention_manager_(policy_.max_files) {}

std::ofstream& FileLifecycleManager::AcquireStream(size_t next_write_size) {
    EnsureDirectory();
    OpenIfNeeded();
    RotateIfNeeded(next_write_size);
    return active_file_.stream;
}

std::filesystem::path FileLifecycleManager::CurrentPath() const {
    return active_file_.path;
}

void FileLifecycleManager::AdvanceSize(size_t bytes) {
    active_file_.current_size_bytes += bytes;
}

void FileLifecycleManager::Flush() {
    if (active_file_.stream.is_open()) {
        active_file_.stream.flush();
        RenameActiveFile();
    }
}

void FileLifecycleManager::EnsureDirectory() const {
    std::error_code ec;
    std::filesystem::create_directories(directory_, ec);
}

void FileLifecycleManager::OpenIfNeeded() {
    if (active_file_.stream.is_open()) {
        return;
    }
    OpenNewActiveFile();
}

void FileLifecycleManager::RotateIfNeeded(size_t next_write_size) {
    if (!rotation_manager_.ShouldRotate(active_file_.current_size_bytes,
                                        next_write_size)) {
        return;
    }

    const auto previous_path = active_file_.path;
    Flush();
    active_file_.stream.close();
    OpenNewActiveFile();
    compression_worker_.Enqueue(previous_path);
    retention_manager_.Cleanup(directory_, active_file_.path);
}

void FileLifecycleManager::RenameActiveFile() {
    const auto new_end_time = FileNamingPolicy::FormatTimeNow();
    if (new_end_time == active_file_.end_time || active_file_.path.empty()) {
        return;
    }

    const auto new_path =
        directory_ / FileNamingPolicy::BuildFileName(active_file_.start_time,
                                                     new_end_time, ".log",
                                                     file_prefix_);
    std::error_code ec;
    std::filesystem::rename(active_file_.path, new_path, ec);
    if (!ec) {
        active_file_.path = new_path;
        active_file_.end_time = new_end_time;
    }
}

void FileLifecycleManager::OpenNewActiveFile() {
    active_file_.start_time = FileNamingPolicy::FormatTimeNow();
    active_file_.end_time = active_file_.start_time;
    active_file_.path =
        directory_ / FileNamingPolicy::BuildFileName(active_file_.start_time,
                                                     active_file_.end_time, ".log",
                                                     file_prefix_);
    active_file_.stream.open(active_file_.path, std::ios::out | std::ios::app);
    std::error_code ec;
    active_file_.current_size_bytes =
        std::filesystem::exists(active_file_.path, ec)
            ? static_cast<size_t>(std::filesystem::file_size(active_file_.path, ec))
            : 0U;
}

}  // namespace naviai::log_module
