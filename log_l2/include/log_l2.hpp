#pragma once

#include "log_l2_types.hpp"
#include "ros_topic.hpp"

namespace naviai::log {

class LogL2 {
  public:
    void Start(const L2RecorderOptions& options = {});
};

}  // namespace naviai::log
