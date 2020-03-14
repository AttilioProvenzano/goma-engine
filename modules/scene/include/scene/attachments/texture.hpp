#pragma once

#include "common/include.hpp"
#include "scene/attachment.hpp"

namespace goma {

class Image;

struct Texture {
    std::string path;

    uint32_t width;
    uint32_t height;
    std::vector<uint8_t> data;
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    uint32_t mip_levels = 1;

    struct {
        bool valid = false;
        Image* image = nullptr;
    } rhi;

    // Attachment component and convenience functions
    AttachmentComponent att_;
    void attach_to(Node& node) { att_.attach_to(node); }
    void detach_from(Node& node) { att_.detach_from(node); }
    void detach_all() { att_.detach_all(); }
    const std::vector<Node*>& attached_nodes() const {
        return att_.attached_nodes();
    }
};

}  // namespace goma
