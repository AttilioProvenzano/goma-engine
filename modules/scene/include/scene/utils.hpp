#pragma once

#include "common/include.hpp"

namespace goma {

enum class VertexAttribute;
using VertexLayout = std::vector<VertexAttribute>;

namespace utils {

size_t GetSize(VertexAttribute attribute);

size_t GetStride(const VertexLayout& layout);

size_t GetOffset(const VertexLayout& layout, VertexAttribute attribute);

}  // namespace utils

}  // namespace goma
