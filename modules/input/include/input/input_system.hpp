#pragma once

#include "platform/platform.hpp"
#include "input/input.hpp"

#include "common/include.hpp"

namespace goma {

class InputSystem {
  public:
    InputSystem(const Platform& platform);

    result<void> AcquireFrameInput();

    InputState GetFrameInput() { return frame_input_; };
    InputState GetLastFrameInput() { return last_frame_input_; };

  private:
    const Platform& platform_;

    InputState frame_input_{};
    InputState last_frame_input_{};
};

}  // namespace goma
