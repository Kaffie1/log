#pragma once

#include <cstdint>

extern "C" {

int naviai_log_l3_init(const char* level_name,
                       const char* root_dir,
                       const char* robot_sn);
int naviai_log_l3_write(const char* module_name,
                        const char* level_name,
                        const char* payload,
                        std::int64_t timestamp_us);
int naviai_log_l3_set_level(const char* level_name);
int naviai_log_l3_set_module_level(const char* module_name, const char* level_name);
void naviai_log_l3_flush();
void naviai_log_l3_shutdown();
const char* naviai_log_l3_last_error();

}
