#pragma once

#define VK_NO_PROTOTYPES
#include "VEZ.h"

#include <vector>

namespace goma {

struct Pipeline {
    VezPipeline vez = VK_NULL_HANDLE;

	Pipeline(VezPipeline vez_) : vez(vez_) {}
};

}  // namespace goma
