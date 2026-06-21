#include "log_agent/include/log_agent.hpp"
#include "log_service/include/log_service_naming.hpp"

#ifdef LOG_CLI_ENABLE_L2_PACKAGE
#include "log_l2/include/operation.hpp"
#endif

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <ctime>
#include <unordered_map>
#include <vector>

namespace {

constexpr std::int64_t kUtcPlus8OffsetSeconds = 8LL * 60LL * 60LL;

struct ParsedArgs {
    std::string command;
    std::unordered_map<std::string, std::string> options;
};

void PrintUsage() {
    std::cout
        << "usage:\n"
        << "  log_cli scan --root <dir>\n"
        << "  log_cli compress --root <dir>\n"
        << "  log_cli recover --root <dir>\n"
        << "  log_cli cleanup --root <dir> [--dry-run]\n"
        << "  log_cli drain --root <dir>\n"
        << "  log_cli inspect --root <dir>\n"
        << "  log_cli query --root <dir> [--file-type <type>] [--suffix <suffix>] \n"
        << "                [--module <name>] [--level <level>] [--start <us|yymmdd_HHMMSS>] [--end <us|yymmdd_HHMMSS>]\n"
        << "  log_cli package --root <dir> --package-root <dir> [--file-type <type>] \n"
        << "                  [--suffix <suffix>] [--module <name>] [--level <level>] \n"
        << "                  [--start <us|yymmdd_HHMMSS>] [--end <us|yymmdd_HHMMSS>] [--duration <seconds>]\n";
#ifdef LOG_CLI_ENABLE_L2_PACKAGE
    std::cout
        << "  log_cli l2-package --root <dir> --start <us|yymmdd_HHMMSS> "
           "(--end <us|yymmdd_HHMMSS> | --duration <seconds>) [--output <path>]\n";
#endif
}

ParsedArgs ParseArgs(int argc, char** argv) {
    if (argc < 2) {
        throw std::invalid_argument("missing command");
    }

    ParsedArgs parsed;
    parsed.command = argv[1];
    for (int index = 2; index < argc; ++index) {
        std::string token = argv[index];
        if (token.rfind("--", 0) != 0) {
            throw std::invalid_argument("unexpected argument: " + token);
        }
        token.erase(0, 2);
        if (token.empty()) {
            throw std::invalid_argument("empty option name");
        }
        if (index + 1 < argc && std::string(argv[index + 1]).rfind("--", 0) != 0) {
            parsed.options[token] = argv[++index];
        } else {
            parsed.options[token] = "true";
        }
    }
    return parsed;
}

std::string RequireOption(const ParsedArgs& args, const std::string& key) {
    const auto found = args.options.find(key);
    if (found == args.options.end() || found->second.empty()) {
        throw std::invalid_argument("missing required option --" + key);
    }
    return found->second;
}

std::optional<std::string> FindOption(const ParsedArgs& args, const std::string& key) {
    const auto found = args.options.find(key);
    if (found == args.options.end() || found->second.empty()) {
        return std::nullopt;
    }
    return found->second;
}

std::int64_t ParseTimeValue(const std::string& value) {
    if (value.find('_') == std::string::npos) {
        return std::stoll(value);
    }

    std::tm tm_value {};
    if (value.size() == 13 && value[6] == '_') {
        tm_value.tm_year = 100 + std::stoi(value.substr(0, 2));
        tm_value.tm_mon = std::stoi(value.substr(2, 2)) - 1;
        tm_value.tm_mday = std::stoi(value.substr(4, 2));
        tm_value.tm_hour = std::stoi(value.substr(7, 2));
        tm_value.tm_min = std::stoi(value.substr(9, 2));
        tm_value.tm_sec = std::stoi(value.substr(11, 2));
#if defined(_WIN32)
        const auto utc_seconds = _mkgmtime(&tm_value);
#else
        const auto utc_seconds = timegm(&tm_value);
#endif
        return static_cast<std::int64_t>(utc_seconds - kUtcPlus8OffsetSeconds) *
               1000000;
    }

    if (value.size() == 15 && value[8] == '_') {
        tm_value.tm_year = std::stoi(value.substr(0, 4)) - 1900;
        tm_value.tm_mon = std::stoi(value.substr(4, 2)) - 1;
        tm_value.tm_mday = std::stoi(value.substr(6, 2));
        tm_value.tm_hour = std::stoi(value.substr(9, 2));
        tm_value.tm_min = std::stoi(value.substr(11, 2));
        tm_value.tm_sec = std::stoi(value.substr(13, 2));
#if defined(_WIN32)
        const auto utc_seconds = _mkgmtime(&tm_value);
#else
        const auto utc_seconds = timegm(&tm_value);
#endif
        return static_cast<std::int64_t>(utc_seconds - kUtcPlus8OffsetSeconds) *
               1000000;
    }

    throw std::invalid_argument(
        "invalid time value: " + value +
        ", expected microseconds or yymmdd_HHMMSS");
}

std::int64_t ParseInt64Option(const ParsedArgs& args,
                              const std::string& key,
                              std::int64_t default_value = 0) {
    const auto value = FindOption(args, key);
    if (!value.has_value()) {
        return default_value;
    }
    return ParseTimeValue(*value);
}

bool HasFlag(const ParsedArgs& args, const std::string& key) {
    const auto found = args.options.find(key);
    return found != args.options.end() && found->second == "true";
}

naviai::log::QueryCondition BuildQueryCondition(const ParsedArgs& args) {
    naviai::log::QueryCondition condition;
    condition.start_time_us = ParseInt64Option(args, "start", 0);
    const auto end_value = FindOption(args, "end");
    const auto duration_value = FindOption(args, "duration");
    if (end_value.has_value() && duration_value.has_value()) {
        throw std::invalid_argument("--end and --duration cannot both be set");
    }
    if (end_value.has_value()) {
        condition.end_time_us = ParseTimeValue(*end_value);
    } else if (duration_value.has_value()) {
        const auto duration_seconds = std::stoll(*duration_value);
        if (duration_seconds <= 0) {
            throw std::invalid_argument("--duration must be positive seconds");
        }
        if (condition.start_time_us <= 0) {
            throw std::invalid_argument("--start must be set when --duration is used");
        }
        condition.end_time_us =
            condition.start_time_us + duration_seconds * 1000000LL;
    }
    condition.file_type = FindOption(args, "file-type").value_or("");
    condition.module_name = FindOption(args, "module").value_or("");
    condition.log_level = FindOption(args, "level").value_or("");
    condition.file_suffix = FindOption(args, "suffix").value_or("");
    return condition;
}

void PrintAgentResult(const naviai::log::LogAgentResult& result) {
    std::cout << "success=" << (result.success ? "true" : "false") << '\n'
              << "message=" << result.message << '\n'
              << "affected_files=" << result.affected_files << '\n';
}

void PrintQueryResult(const naviai::log::QueryResult& result) {
    std::cout << "success=" << (result.success ? "true" : "false") << '\n'
              << "message=" << result.message << '\n'
              << "total_files=" << result.total_files << '\n';
    for (const auto& file : result.files) {
        std::cout << file.string() << '\n';
    }
}

void PrintPackageTask(const naviai::log::PackageTask& task) {
    std::cout << "task_state=" << task.task_state << '\n'
              << "message=" << task.message << '\n'
              << "task_id=" << task.task_id << '\n'
              << "output_path=" << task.output_path.string() << '\n'
              << "manifest_path=" << task.manifest_path.string() << '\n'
              << "source_file_count=" << task.source_files.size() << '\n';
    for (const auto& file : task.source_files) {
        std::cout << file.string() << '\n';
    }
}

void PrintAgentState(const naviai::log::LogAgentState& state) {
    std::cout << "root_dir=" << state.root_dir.string() << '\n'
              << "total_files=" << state.stats.total_files << '\n'
              << "active_files=" << state.stats.active_files << '\n'
              << "sealed_files=" << state.stats.sealed_files << '\n'
              << "compressed_files=" << state.stats.compressed_files << '\n'
              << "abnormal_files=" << state.stats.abnormal_files << '\n'
              << "total_size_bytes=" << state.stats.total_size_bytes << '\n';
    for (const auto& file : state.files) {
        std::cout << file.path.string() << " state=" << file.file_state
                  << " type=" << file.file_type
                  << " start=" << file.start_time_us
                  << " end=" << file.end_time_us
                  << " size=" << file.size_bytes << '\n';
    }
}

int RunAgentCommand(const ParsedArgs& args) {
    const auto root_dir = RequireOption(args, "root");
    naviai::log::LogAgent agent(root_dir);

    naviai::log::LogAgentResult result;
    if (args.command == "scan") {
        result = agent.ScanNow();
    } else if (args.command == "compress") {
        result = agent.CompressNow();
    } else if (args.command == "recover") {
        result = agent.RecoverNow();
    } else if (args.command == "cleanup") {
        result = agent.CleanupNow(HasFlag(args, "dry-run"));
    } else if (args.command == "drain") {
        result = agent.DrainNow();
    } else if (args.command == "inspect") {
        PrintAgentState(agent.GetState());
        return 0;
    } else {
        throw std::invalid_argument("unsupported agent command: " + args.command);
    }

    PrintAgentResult(result);
    return result.success ? 0 : 1;
}

int RunQueryCommand(const ParsedArgs& args) {
    const auto root_dir = RequireOption(args, "root");
    naviai::log::LogService service(root_dir);
    const auto result = service.QueryLogs(BuildQueryCondition(args));
    PrintQueryResult(result);
    return result.success ? 0 : 1;
}

int RunPackageCommand(const ParsedArgs& args) {
    const auto root_dir = RequireOption(args, "root");
    const auto package_root = RequireOption(args, "package-root");
    naviai::log::LogService service(root_dir);
    const auto task =
        service.PackageLogs(BuildQueryCondition(args), std::filesystem::path(package_root));
    PrintPackageTask(task);
    return task.task_state == "completed" ? 0 : 1;
}

int RunL2PackageCommand(const ParsedArgs& args) {
#ifdef LOG_CLI_ENABLE_L2_PACKAGE
    naviai::log::L2PackageOptions options;
    options.root_dir = RequireOption(args, "root");
    options.start_time_us = ParseInt64Option(args, "start");
    options.output_path = FindOption(args, "output").value_or("");
    const auto end_value = FindOption(args, "end");
    const auto duration_value = FindOption(args, "duration");
    if (end_value.has_value() == duration_value.has_value()) {
        throw std::invalid_argument(
            "l2-package requires exactly one of --end or --duration");
    }
    if (end_value.has_value()) {
        options.end_time_us = ParseTimeValue(*end_value);
    } else {
        const auto duration_seconds = std::stoll(*duration_value);
        if (duration_seconds <= 0) {
            throw std::invalid_argument("--duration must be positive seconds");
        }
        options.end_time_us =
            options.start_time_us + duration_seconds * 1000000LL;
    }
    if (options.start_time_us <= 0 || options.end_time_us <= 0) {
        throw std::invalid_argument("--start and --end/duration must be positive");
    }

    const auto archive_path = naviai::log::l2_log::PackageRecords(options);
    std::cout << "archive_path=" << archive_path << '\n';
    return 0;
#else
    (void)args;
    throw std::invalid_argument("l2-package is unavailable in this build");
#endif
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto args = ParseArgs(argc, argv);
        if (args.command == "scan" || args.command == "compress" ||
            args.command == "recover" || args.command == "cleanup" ||
            args.command == "drain" || args.command == "inspect") {
            return RunAgentCommand(args);
        }
        if (args.command == "query") {
            return RunQueryCommand(args);
        }
        if (args.command == "package") {
            return RunPackageCommand(args);
        }
        if (args.command == "l2-package") {
            return RunL2PackageCommand(args);
        }
        if (args.command == "help" || args.command == "--help") {
            PrintUsage();
            return 0;
        }
        throw std::invalid_argument("unsupported command: " + args.command);
    } catch (const std::exception& ex) {
        std::cerr << "log_cli error: " << ex.what() << '\n';
        PrintUsage();
        return 2;
    }
}
