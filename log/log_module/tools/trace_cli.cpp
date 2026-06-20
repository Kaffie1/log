#include <iostream>
#include <string>

#include "query_service.hpp"

int main(int argc, char** argv) {
    naviai::log_module::QueryFilter filter;
    if (argc > 1) {
        filter.root_dir = argv[1];
    }
    if (argc > 2) {
        filter.trace_id = argv[2];
    }
    if (argc > 3) {
        filter.timestamp_from_us = std::stoll(argv[3]);
    }
    if (argc > 4) {
        filter.timestamp_to_us = std::stoll(argv[4]);
    }

    const auto lines = naviai::log_module::TraceQueryService().QueryTrace(filter);
    for (const auto& line : lines) {
        std::cout << line << '\n';
    }
    return 0;
}
