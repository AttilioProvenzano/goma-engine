#pragma once

namespace goma {

struct Texture {
    std::string name;

    uint32_t width;
    uint32_t height;
    std::vector<uint8_t> data;

    bool compressed = false;
    std::string extension = "";
};

}  // namespace goma
