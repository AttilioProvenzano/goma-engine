#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"
#include "renderer/buffer.hpp"
#include "renderer/image.hpp"
#include "renderer/pipeline.hpp"
#include "renderer/shader.hpp"

namespace std {

template <>
struct hash<goma::PipelineDesc> {
    union PackedDesc {
        struct {
            VkPrimitiveTopology primitive_topology : 4;
            VkCullModeFlagBits cull_mode : 2;
            VkFrontFace front_face : 1;
            VkSampleCountFlagBits sample_count : 6;
            bool depth_test : 1;
            bool depth_write : 1;
            VkCompareOp depth_compare_op : 3;
            bool stencil_test : 1;
            bool color_blend : 1;
            bool suppress_fragment : 1;
        };
        uint32_t val;
    };

    // FIXME: this and operator== don't take FramebufferDesc into account

    size_t operator()(const goma::PipelineDesc& desc) const {
        size_t seed = vector_hash<goma::Shader*>()(desc.shaders);

        auto packed_desc = PackedDesc{
            desc.primitive_topology, desc.cull_mode,    desc.front_face,
            desc.sample_count,       desc.depth_test,   desc.depth_write,
            desc.depth_compare_op,   desc.stencil_test, desc.color_blend,
            desc.suppress_fragment};
        ::hash_combine(seed, packed_desc.val);

        return seed;
    };
};

}  // namespace std

namespace goma {

struct FramebufferDesc;
class Context;
class Platform;

struct Receipt {
    size_t submission_id;
    VkDevice device = VK_NULL_HANDLE;
};
using ReceiptPtr = std::unique_ptr<Receipt>;

class Device {
  public:
    struct Config {
        enum class FbColorSpace {
            Linear,
            Srgb
        } fb_color_space = FbColorSpace::Srgb;
    };

    Device(const Config& = {});
    ~Device();

    result<void> InitWindow(Platform&);

    VkDevice GetHandle();
    uint32_t GetQueueFamilyIndex();

    result<Buffer*> CreateBuffer(const BufferDesc&);
    result<void*> MapBuffer(Buffer&);
    void UnmapBuffer(Buffer&);

    result<Image*> AcquireSwapchainImage();
    result<Image*> CreateImage(const ImageDesc&);

    result<Shader*> CreateShader(ShaderDesc);
    result<Pipeline*> CreatePipeline(PipelineDesc, FramebufferDesc&);

    result<ReceiptPtr> Submit(Context&);
    result<void> WaitOnWork(ReceiptPtr&&);
    result<void> Present();

    // void ProcessWindowChanges(Platform&);

  private:
    result<void> Init();
    VkSemaphore GetSemaphore();
    VkFence GetFence();

    Config config_;
    uint32_t queue_family_index_ = -1;

    struct {
        VkInstance instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkPhysicalDeviceFeatures features = {};
        VkPhysicalDeviceProperties properties = {};
        VkDevice device = VK_NULL_HANDLE;
        VkQueue queue = VK_NULL_HANDLE;

        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;

        VmaAllocator allocator = VK_NULL_HANDLE;
        VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
        std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
        std::vector<VkPipelineLayout> pipeline_layouts;
        std::vector<VkRenderPass> render_passes;
    } api_handles_;

    std::vector<std::unique_ptr<Buffer>> buffers_;
    std::vector<std::unique_ptr<Image>> images_;
    std::vector<std::unique_ptr<Image>> swapchain_images_;
    std::vector<std::unique_ptr<Shader>> shaders_;

    using PipelineMap =
        std::unordered_map<std::vector<Shader*>, std::unique_ptr<Pipeline>,
                           VectorHash<Shader*>>;
    PipelineMap pipeline_map_;

    std::vector<VkSemaphore> recycled_semaphores_;

    std::unordered_map<size_t, VkSemaphore> acquisition_semaphores_;
    std::unordered_map<size_t, VkSemaphore> presentation_semaphores_;

    std::unordered_map<size_t, VkCommandBuffer> presentation_cmd_bufs_;
    VkCommandPool cmd_pool_ = VK_NULL_HANDLE;

    std::vector<VkFence> recycled_fences_;
    std::unordered_map<size_t, VkFence> submission_fences_;
    std::unordered_map<size_t, VkFence> presentation_fences_;

    static const uint32_t kInvalidSwapchainIndex;
    size_t last_submission_id_ = 0;
    uint32_t swapchain_index_ = kInvalidSwapchainIndex;
};

}  // namespace goma
