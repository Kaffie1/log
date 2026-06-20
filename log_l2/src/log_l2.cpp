#include "log_l2.hpp"

#include "operation.hpp"

namespace naviai::log {

void LogL2::Start(const L2RecorderOptions& options) {
    L2RecorderOptions effective = options;
    effective.topics = ToL2TopicDescriptors(DefaultReplayRosTopics());
    l2_log::InitRecorder(effective);
}

}  // namespace naviai::log
