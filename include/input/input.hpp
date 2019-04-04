#pragma once

#include "common/include.hpp"

namespace goma {

enum class KeyInput { W, A, S, D, Up, Down, Left, Right };

struct InputState {
    std::set<KeyInput> keypresses;
};

}  // namespace goma
