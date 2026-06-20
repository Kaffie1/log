#pragma once

#include <memory>
#include <string>

namespace naviai::log {

class L2Ros1Recorder {
  public:
    L2Ros1Recorder();
    ~L2Ros1Recorder();

    int Run(int argc, char** argv,
            const std::string& node_name = "l2_ros1_recorder");

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace naviai::log
