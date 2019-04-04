#include "input/input_system.hpp"

namespace goma {

InputSystem::InputSystem(const Platform& platform) : platform_(platform) {}

result<void> InputSystem::AcquireFrameInput() {
    last_frame_input_ = frame_input_;
    frame_input_ = platform_.GetInputState();

    return outcome::success();
};

}  // namespace goma
