#include "l2_log/include/ros1_recorder.hpp"

int main(int argc, char** argv) {
    naviai::log_module::L2Ros1Recorder recorder;
    return recorder.Run(argc, argv);
}
