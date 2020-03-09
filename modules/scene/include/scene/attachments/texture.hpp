#pragma once

#include "common/include.hpp"

namespace goma {

class Image;

struct Texture {
    std::string path;

    uint32_t width;
    uint32_t height;
    std::vector<uint8_t> data;
    bool compressed{false};

    Image* image = nullptr;
};

}  // namespace goma
