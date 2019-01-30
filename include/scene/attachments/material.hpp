#pragma once

#include "scene/attachment.hpp"

namespace goma {

struct Texture;

struct Material {
    std::string name;

	AttachmentIndex<Texture> texture;
};

}  // namespace goma
