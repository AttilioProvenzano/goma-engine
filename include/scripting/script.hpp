#pragma once

#include "common/include.hpp"

namespace goma {

class Engine;

class Script {
  public:
    virtual void Update(Engine& engine, float delta_time) = 0;
};

}  // namespace goma
