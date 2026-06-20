#include "ros1_recorder.hpp"

int main(int argc, char** argv) {
    naviai::log::L2Ros1Recorder recorder;
    return recorder.Run(argc, argv);
}
