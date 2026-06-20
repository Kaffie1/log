#include "log_l2/include/operation.hpp"

#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 4 || argc > 5) {
        std::cerr << "usage: l2_package_cli <root_dir> <start_time_us> <end_time_us> "
                     "[output_path]\n";
        return 2;
    }

    try {
        naviai::log::L2PackageOptions options;
        options.root_dir = argv[1];
        options.start_time_us = std::stoll(argv[2]);
        options.end_time_us = std::stoll(argv[3]);
        if (argc >= 5) {
            options.output_path = argv[4];
        }

        const auto archive_path = naviai::log::l2_log::PackageRecords(options);
        std::cout << "archive_path=" << archive_path << '\n';
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "package_failed=" << ex.what() << '\n';
        return 1;
    }
}
