#pragma once

#include "renderer/handles.hpp"

#include "common/include.hpp"

namespace goma {

struct Texture {
    std::string path;

    uint32_t width;
    uint32_t height;
    std::vector<uint8_t> data;

    bool compressed = false;

    std::shared_ptr<Image> image;
};

}  // namespace goma
