#pragma once

#include <condition_variable>
#include <cstddef>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "log_config.hpp"

namespace naviai::log_module {

struct ActiveFileState {
    std::filesystem::path path;
    std::ofstream stream;
    size_t current_size_bytes{0};
    std::string start_time;
    std::string end_time;
};

class FileNamingPolicy {
  public:
    static std::string FormatTimeNow();
    static bool TryParseStartTime(const std::filesystem::path& path,
                                  std::time_t* start_time);
    static std::string BuildFileName(const std::string& start_time,
                                     const std::string& end_time,
                                     const std::string& suffix = ".log",
                                     const std::string& prefix = {});
};

class RotationManager {
  public:
    explicit RotationManager(FileRotationPolicy policy);
    bool ShouldRotate(size_t current_size_bytes, size_t next_write_size) const;

  private:
    FileRotationPolicy policy_;
};

class CompressionWorker {
  public:
    CompressionWorker();
    ~CompressionWorker();
    void Enqueue(const std::filesystem::path& source_path) const;

  private:
    void WorkerLoop();
    static bool CompressFile(const std::filesystem::path& source_path);

    mutable bool running_{true};
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    mutable std::queue<std::filesystem::path> pending_paths_;
    mutable std::thread worker_;
};

class RetentionManager {
  public:
    explicit RetentionManager(size_t max_files);
    void Cleanup(const std::filesystem::path& directory,
                 const std::filesystem::path& active_file) const;

  private:
    size_t max_files_;
};

class FileLifecycleManager {
  public:
    FileLifecycleManager(FileRotationPolicy policy,
                         std::filesystem::path directory,
                         std::string file_prefix = {});
    std::ofstream& AcquireStream(size_t next_write_size);
    std::filesystem::path CurrentPath() const;
    void AdvanceSize(size_t bytes);
    void Flush();

  private:
    void EnsureDirectory() const;
    void OpenIfNeeded();
    void RotateIfNeeded(size_t next_write_size);
    void RenameActiveFile();
    void OpenNewActiveFile();

    FileRotationPolicy policy_;
    std::filesystem::path directory_;
    std::string file_prefix_;
    RotationManager rotation_manager_;
    CompressionWorker compression_worker_;
    RetentionManager retention_manager_;
    ActiveFileState active_file_;
};

}  // namespace naviai::log_module
