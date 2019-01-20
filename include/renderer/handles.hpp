#pragma once

#define VK_NO_PROTOTYPES
#include "VEZ.h"

namespace goma {

typedef std::vector<uint32_t> PipelineHash;

struct Pipeline {
    VezPipeline vez = VK_NULL_HANDLE;

	Pipeline(VezPipeline vez_) : vez(vez_) {}
};

}  // namespace goma
