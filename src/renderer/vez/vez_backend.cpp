#include "renderer/vez/vez_backend.hpp"

#include "Core/ShaderModule.h"  // from V-EZ

#define VK_CHECK(fn)                                                         \
    {                                                                        \
        VkResult _r = fn;                                                    \
        if (_r != VK_SUCCESS) {                                              \
            spdlog::error(                                                   \
                "{}, line {}: In function {}, a Vulkan error occurred when " \
                "running {}.",                                               \
                __FILE__, __LINE__, __func__, #fn);                          \
        };                                                                   \
        switch (_r) {                                                        \
            case VK_SUCCESS:                                                 \
                break;                                                       \
            case VK_ERROR_INITIALIZATION_FAILED:                             \
                return Error::VulkanInitializationFailed;                    \
            case VK_ERROR_OUT_OF_HOST_MEMORY:                                \
                return Error::OutOfCPUMemory;                                \
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:                              \
                return Error::OutOfGPUMemory;                                \
            case VK_ERROR_LAYER_NOT_PRESENT:                                 \
                return Error::VulkanLayerNotPresent;                         \
            case VK_ERROR_EXTENSION_NOT_PRESENT:                             \
                return Error::VulkanExtensionNotPresent;                     \
            default:                                                         \
                return Error::GenericVulkanError;                            \
        }                                                                    \
    }

uint64_t sdbm_hash(const char* str) {
    uint64_t hash = 0;
    int c;

    while (c = *str++) hash = c + (hash << 6) + (hash << 16) - hash;

    return hash;
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(
    VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT, uint64_t, size_t,
    int32_t, const char* pLayerPrefix, const char* pMessage, void*) {
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
        spdlog::error("{} - {}", pLayerPrefix, pMessage);
    } else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
        spdlog::warn("{} - {}", pLayerPrefix, pMessage);
    } else {
        spdlog::info("{} - {}", pLayerPrefix, pMessage);
    }

    return VK_FALSE;
}

namespace goma {

VezBackend::VezBackend(const Config& config, const RenderPlan& render_plan)
    : Backend(config, render_plan) {
    SetBuffering(config.buffering);
}

VezBackend::~VezBackend() { TeardownContext(); }

result<void> VezBackend::SetBuffering(Buffering buffering) {
    config_.buffering = buffering;

    uint32_t buf_size = config_.buffering == Buffering::Triple ? 3 : 2;
    if (buf_size >= context_.per_frame.size()) {
        context_.per_frame.resize(buf_size);
    }
    return outcome::success();
}

result<void> VezBackend::SetFramebufferColorSpace(
    FramebufferColorSpace fb_color_space) {
    config_.fb_color_space = fb_color_space;
    return outcome::success();
}

result<void> VezBackend::InitContext() {
    VK_CHECK(volkInitialize());

    OUTCOME_TRY(instance, CreateInstance());
    context_.instance = instance;

    OUTCOME_TRY(debug_callback, CreateDebugCallback(instance));
    context_.debug_callback = debug_callback;

    OUTCOME_TRY(p, CreatePhysicalDevice(instance));
    context_.physical_device = p.physical_device;
    context_.features = p.features;
    context_.properties = p.properties;

    OUTCOME_TRY(device, CreateDevice(p.physical_device));
    context_.device = device;

    return outcome::success();
}

result<void> VezBackend::InitSurface(Platform& platform) {
    assert(context_.instance && "Context must be initialized");
    assert(context_.physical_device && "Context must be initialized");
    assert(context_.device && "Context must be initialized");

    OUTCOME_TRY(surface, platform.CreateVulkanSurface(context_.instance));
    context_.surface = surface;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physical_device, surface,
                                              &context_.capabilities);

    OUTCOME_TRY(swapchain, CreateSwapchain(surface));
    context_.swapchain = swapchain;

    return outcome::success();
}

result<std::shared_ptr<Pipeline>> VezBackend::GetGraphicsPipeline(
    const ShaderDesc& vert, const ShaderDesc& frag) {
    assert(context_.device &&
           "Context must be initialized before creating a pipeline");
    VkDevice device = context_.device;

    OUTCOME_TRY(vertex_shader, GetVertexShaderModule(vert));

    VkShaderModule fragment_shader{VK_NULL_HANDLE};
    if (!frag.source.empty()) {
        OUTCOME_TRY(fs, GetFragmentShaderModule(frag));
        fragment_shader = fs;
    }

    auto hash = GetGraphicsPipelineHash(vertex_shader, fragment_shader);
    auto result = context_.pipeline_cache.find(hash);

    if (result != context_.pipeline_cache.end()) {
        return result->second;
    } else {
        std::vector<VezPipelineShaderStageCreateInfo> shader_stages;
        shader_stages.push_back({nullptr, vertex_shader});
        if (fragment_shader != VK_NULL_HANDLE) {
            shader_stages.push_back({nullptr, fragment_shader});
        }

        VezGraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
        pipeline_info.pStages = shader_stages.data();

        VezPipeline pipeline = VK_NULL_HANDLE;
        VK_CHECK(vezCreateGraphicsPipeline(device, &pipeline_info, &pipeline));

        auto ret = std::make_shared<Pipeline>(pipeline);
        context_.pipeline_cache[hash] = ret;
        return ret;
    }
}

result<void> VezBackend::ClearShaderCache() {
    auto& per_frame = context_.per_frame[context_.current_frame];

    for (auto& shader : context_.vertex_shader_cache) {
        vezDestroyShaderModule(context_.device, shader.second);
    }
    context_.vertex_shader_cache.clear();

    for (auto& shader : context_.fragment_shader_cache) {
        vezDestroyShaderModule(context_.device, shader.second);
    }
    context_.fragment_shader_cache.clear();

    // Pipelines may still be in use, so we add them to a list of orphaned
    // ones and delete them after the next fence for this frame index.
    // This guarantees that they will not be in use anymore.
    for (auto& pipeline : context_.pipeline_cache) {
        per_frame.orphaned_pipelines.push_back(pipeline.second->vez);
        pipeline.second->valid = false;
    }
    context_.pipeline_cache.clear();

    return outcome::success();
}

result<std::shared_ptr<VertexInputFormat>> VezBackend::GetVertexInputFormat(
    const VertexInputFormatDesc& desc) {
    assert(context_.device && "Context must be initialized");
    VkDevice device = context_.device;

    auto hash = GetVertexInputFormatHash(desc);

    auto result = context_.vertex_input_format_cache.find(hash);
    if (result != context_.vertex_input_format_cache.end()) {
        return result->second;
    }

    std::vector<VkVertexInputBindingDescription> bindings;
    for (const auto& b : desc.bindings) {
        bindings.push_back({b.binding, b.stride,
                            b.per_instance ? VK_VERTEX_INPUT_RATE_INSTANCE
                                           : VK_VERTEX_INPUT_RATE_VERTEX});
    }

    std::vector<VkVertexInputAttributeDescription> attributes;
    for (const auto& a : desc.attributes) {
        attributes.push_back(
            {a.location, a.binding, GetVkFormat(a.format), a.offset});
    }

    VezVertexInputFormatCreateInfo input_info{};
    input_info.vertexBindingDescriptionCount =
        static_cast<uint32_t>(bindings.size());
    input_info.pVertexBindingDescriptions = bindings.data();
    input_info.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributes.size());
    input_info.pVertexAttributeDescriptions = attributes.data();

    VezVertexInputFormat input_format;
    vezCreateVertexInputFormat(device, &input_info, &input_format);

    auto ret = std::make_shared<VertexInputFormat>(input_format);
    context_.vertex_input_format_cache[hash] = ret;
    return ret;
};

result<std::shared_ptr<Image>> VezBackend::CreateTexture(
    const char* name, const TextureDesc& texture_desc, void* initial_contents) {
    auto hash = GetTextureHash(name);
    auto result = context_.texture_cache.find(hash);
    if (result != context_.texture_cache.end()) {
        vezDestroyImageView(context_.device, result->second->vez.image_view);
        vezDestroyImage(context_.device, result->second->vez.image);
    }

    VkFormat format = GetVkFormat(texture_desc.format);

    uint32_t mip_levels = 1;
    if (texture_desc.mipmapping) {
        uint32_t min_dim = std::min(texture_desc.width, texture_desc.height);
        mip_levels = static_cast<uint32_t>(floor(log2(min_dim) + 1));
        mip_levels = std::max(1U, mip_levels);
    }

    VezImageCreateInfo image_info{};
    image_info.extent = {texture_desc.width, texture_desc.height, 1};
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.arrayLayers = texture_desc.array_layers;
    image_info.mipLevels = mip_levels;
    image_info.format = format;
    image_info.samples =
        static_cast<VkSampleCountFlagBits>(texture_desc.samples);
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage =
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (mip_levels > 1) {
        // We need TRANSFER_SRC to generate mipmaps
        image_info.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }

    OUTCOME_TRY(vulkan_image, CreateImage(hash, image_info));

    if (initial_contents) {
        OUTCOME_TRY(FillImage(vulkan_image.image,
                              {texture_desc.width, texture_desc.height},
                              {initial_contents}));
    }

    if (mip_levels > 1) {
        OUTCOME_TRY(GenerateMipmaps(vulkan_image.image,
                                    texture_desc.array_layers,
                                    {texture_desc.width, texture_desc.height}));
    }

    OUTCOME_TRY(sampler, GetSampler(texture_desc.sampler));
    vulkan_image.sampler = sampler;

    auto ret = std::make_shared<Image>(vulkan_image);
    context_.texture_cache[hash] = ret;
    return ret;
}

result<std::shared_ptr<Image>> VezBackend::CreateCubemap(
    const char* name, const TextureDesc& texture_desc,
    const CubemapContents& initial_contents) {
    auto hash = GetTextureHash(name);
    auto result = context_.texture_cache.find(hash);
    if (result != context_.texture_cache.end()) {
        vezDestroyImageView(context_.device, result->second->vez.image_view);
        vezDestroyImage(context_.device, result->second->vez.image);
    }

    VkFormat format = GetVkFormat(texture_desc.format);

    uint32_t mip_levels = 1;
    if (texture_desc.mipmapping) {
        uint32_t min_dim = std::min(texture_desc.width, texture_desc.height);
        mip_levels = static_cast<uint32_t>(floor(log2(min_dim) + 1));
        mip_levels = std::max(1U, mip_levels);
    }

    VezImageCreateInfo image_info{};
    image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    image_info.extent = {texture_desc.width, texture_desc.height, 1};
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.arrayLayers = texture_desc.array_layers * 6;
    image_info.mipLevels = mip_levels;
    image_info.format = format;
    image_info.samples =
        static_cast<VkSampleCountFlagBits>(texture_desc.samples);
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage =
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (mip_levels > 1) {
        // We need TRANSFER_SRC to generate mipmaps
        image_info.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }

    OUTCOME_TRY(vulkan_image, CreateImage(hash, image_info));

    if (initial_contents) {
        OUTCOME_TRY(FillImage(vulkan_image.image,
                              {texture_desc.width, texture_desc.height},
                              {initial_contents.right, initial_contents.left,
                               initial_contents.up, initial_contents.down,
                               initial_contents.front, initial_contents.back}));
    }

    if (mip_levels > 1) {
        OUTCOME_TRY(GenerateMipmaps(vulkan_image.image,
                                    texture_desc.array_layers * 6,
                                    {texture_desc.width, texture_desc.height}));
    }

    OUTCOME_TRY(sampler, GetSampler(texture_desc.sampler));
    vulkan_image.sampler = sampler;

    auto ret = std::make_shared<Image>(vulkan_image);
    context_.texture_cache[hash] = ret;

    return ret;
}

result<std::shared_ptr<Image>> VezBackend::GetTexture(const char* name) {
    auto hash = GetTextureHash(name);
    auto result = context_.texture_cache.find(hash);
    if (result != context_.texture_cache.end()) {
        return result->second;
    }

    return Error::NotFound;
}

result<Framebuffer> VezBackend::CreateFramebuffer(FrameIndex frame_id,
                                                  const char* name,
                                                  RenderPassDesc rp_desc) {
    assert(context_.device &&
           "Context must be initialized before creating a framebuffer");
    VkDevice device = context_.device;

    auto hash = GetFramebufferHash(frame_id, name);
    auto result = context_.framebuffer_cache.find(hash);
    if (result != context_.framebuffer_cache.end()) {
        vezDestroyFramebuffer(context_.device, result->second);
    }

    Extent extent{};

    std::vector<VkImageView> attachments;
    for (auto& attachment : rp_desc.color_attachments) {
        auto image_desc_res =
            render_plan_.color_images.find(attachment.rt_name);
        if (image_desc_res != render_plan_.color_images.end()) {
            auto image_res =
                GetRenderTarget(frame_id, attachment.rt_name.c_str());
            if (!image_res) {
                spdlog::error(
                    "Could not find color target \"{}\" when creating "
                    "framebuffer \"{}\"",
                    attachment.rt_name, name);
                return image_res.error();
            }
            auto image = image_res.value();

            attachments.push_back(image->vez.image_view);

            if (extent == Extent{}) {
                extent = image_desc_res->second.extent;
            }

            if (image_desc_res->second.extent != extent) {
                spdlog::error(
                    "Dimensions for color image \"{}\" not matching dimensions "
                    "of other images when creating framebuffer \"{}\".",
                    attachment.rt_name, name);
                return Error::DimensionsNotMatching;
            }
        } else {
            spdlog::error(
                "Color image \"{}\" not found when creating framebuffer "
                "\"{}\".",
                attachment.rt_name, name);
            return Error::NotFound;
        }
    }

    if (rp_desc.depth_attachment.rt_name != "") {
        auto depth_image_desc_res =
            render_plan_.depth_images.find(rp_desc.depth_attachment.rt_name);
        if (depth_image_desc_res != render_plan_.depth_images.end()) {
            auto image_res = GetRenderTarget(
                frame_id, rp_desc.depth_attachment.rt_name.c_str());
            if (!image_res) {
                spdlog::error(
                    "Could not find depth target \"{}\" when creating "
                    "framebuffer \"{}\"",
                    rp_desc.depth_attachment.rt_name, name);
                return image_res.error();
            }
            auto image = image_res.value();

            attachments.push_back(image->vez.image_view);

            if (extent == Extent{}) {
                extent = depth_image_desc_res->second.extent;
            }

            if (depth_image_desc_res->second.extent != extent) {
                spdlog::error(
                    "Dimensions for depth image \"{}\" not matching dimensions "
                    "of other images when creating framebuffer \"{}\".",
                    rp_desc.depth_attachment.rt_name, name);
                return Error::DimensionsNotMatching;
            }
            extent = depth_image_desc_res->second.extent;
        }
    }

    // Convert to absolute extent
    extent = GetAbsoluteExtent(extent);

    VezFramebufferCreateInfo fb_info{};
    fb_info.width = extent.rounded_width();
    fb_info.height = extent.rounded_height();
    fb_info.layers = 1;
    fb_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    fb_info.pAttachments = attachments.data();

    VezFramebuffer framebuffer = VK_NULL_HANDLE;
    VK_CHECK(vezCreateFramebuffer(device, &fb_info, &framebuffer));
    context_.framebuffer_cache[hash] = framebuffer;
    return {framebuffer};
}

result<Framebuffer> VezBackend::GetFramebuffer(FrameIndex frame_id,
                                               const char* name) {
    assert(context_.device &&
           "Context must be initialized before getting a framebuffer");

    auto hash = GetFramebufferHash(frame_id, name);
    auto result = context_.framebuffer_cache.find(hash);
    if (result != context_.framebuffer_cache.end()) {
        return result->second;
    }

    return Error::NotFound;
}

result<std::shared_ptr<Buffer>> VezBackend::CreateUniformBuffer(
    BufferType type, const GenIndex& index, const char* name, uint64_t size,
    bool gpu_stored, void* initial_contents) {
    auto hash = GetBufferHash(type, index, name);
    OUTCOME_TRY(buffer, CreateBuffer(hash, static_cast<VkDeviceSize>(size),
                                     gpu_stored ? VEZ_MEMORY_GPU_ONLY
                                                : VEZ_MEMORY_CPU_TO_GPU,
                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     initial_contents));
    return buffer;
}

result<std::shared_ptr<Buffer>> VezBackend::GetUniformBuffer(
    BufferType type, const GenIndex& index, const char* name) {
    auto hash = GetBufferHash(type, index, name);
    auto result = context_.buffer_cache.find(hash);
    if (result != context_.buffer_cache.end()) {
        return result->second;
    }

    return Error::NotFound;
}

result<std::shared_ptr<Buffer>> VezBackend::CreateUniformBuffer(
    const char* name, uint64_t size, bool gpu_stored, void* initial_contents) {
    auto hash = GetBufferHash(name);
    OUTCOME_TRY(buffer, CreateBuffer(hash, static_cast<VkDeviceSize>(size),
                                     gpu_stored ? VEZ_MEMORY_GPU_ONLY
                                                : VEZ_MEMORY_CPU_TO_GPU,
                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     initial_contents));
    return buffer;
}

result<std::shared_ptr<Buffer>> VezBackend::GetUniformBuffer(const char* name) {
    auto hash = GetBufferHash(name);
    auto result = context_.buffer_cache.find(hash);
    if (result != context_.buffer_cache.end()) {
        return result->second;
    }

    return Error::NotFound;
}

result<std::shared_ptr<Buffer>> VezBackend::CreateVertexBuffer(
    const AttachmentIndex<Mesh>& mesh, const char* name, uint64_t size,
    bool gpu_stored, void* initial_contents) {
    auto hash = GetBufferHash(BufferType::PerMesh, mesh, name);
    OUTCOME_TRY(buffer, CreateBuffer(hash, static_cast<VkDeviceSize>(size),
                                     gpu_stored ? VEZ_MEMORY_GPU_ONLY
                                                : VEZ_MEMORY_CPU_TO_GPU,
                                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     initial_contents));
    return buffer;
}

result<std::shared_ptr<Buffer>> VezBackend::GetVertexBuffer(
    const AttachmentIndex<Mesh>& mesh, const char* name) {
    auto hash = GetBufferHash(BufferType::PerMesh, mesh, name);
    auto result = context_.buffer_cache.find(hash);
    if (result != context_.buffer_cache.end()) {
        return result->second;
    }

    return Error::NotFound;
}

result<std::shared_ptr<Buffer>> VezBackend::CreateIndexBuffer(
    const AttachmentIndex<Mesh>& mesh, const char* name, uint64_t size,
    bool gpu_stored, void* initial_contents) {
    auto hash = GetBufferHash(BufferType::PerMesh, mesh, name);
    OUTCOME_TRY(buffer, CreateBuffer(hash, static_cast<VkDeviceSize>(size),
                                     gpu_stored ? VEZ_MEMORY_GPU_ONLY
                                                : VEZ_MEMORY_CPU_TO_GPU,
                                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     initial_contents));
    return buffer;
}

result<std::shared_ptr<Buffer>> VezBackend::GetIndexBuffer(
    const AttachmentIndex<Mesh>& mesh, const char* name) {
    auto hash = GetBufferHash(BufferType::PerMesh, mesh, name);
    auto result = context_.buffer_cache.find(hash);
    if (result != context_.buffer_cache.end()) {
        return result->second;
    }

    return Error::NotFound;
}

result<void> VezBackend::UpdateBuffer(const Buffer& buffer, uint64_t offset,
                                      uint64_t size, const void* contents) {
    uint8_t* data;
    auto res = vezMapBuffer(context_.device, buffer.vez, offset, size,
                            reinterpret_cast<void**>(&data));
    if (res != VK_SUCCESS) {
        return Error::BufferCannotBeMapped;
    }

    memcpy(data + offset, contents, size);
    vezUnmapBuffer(context_.device, buffer.vez);

    return outcome::success();
}

result<void> VezBackend::SetRenderPlan(RenderPlan render_plan) {
    // Teardown any existing render plan
    for (auto framebuffer : context_.framebuffer_cache) {
        vezDestroyFramebuffer(context_.device, framebuffer.second);
    }

    for (auto image : context_.fb_image_cache) {
        vezDestroyImageView(context_.device, image.second->vez.image_view);
        vezDestroyImage(context_.device, image.second->vez.image);
    }

    // Copy the render plan into render_plan
    render_plan_ = std::move(render_plan);
    return outcome::success();
}

result<void> VezBackend::RenderFrame(std::vector<PassFn> pass_fns,
                                     const char* present_image) {
    auto frame_id = StartFrame().value();

    for (size_t i = 0; i < render_plan_.passes.size(); i++) {
        auto& pass = render_plan_.passes[i];
        const auto& pass_name =
            pass.match([](const GeneralPassEntry& gp) { return gp.name; },
                       [](const RenderPassEntry& rp) { return rp.name; });

        if (pass_fns.size() != 1 && i >= pass_fns.size()) {
            spdlog::error("Skipping pass \"{}\", no function provided.",
                          pass_name);
            continue;
        }
        auto& pass_fn = (pass_fns.size() == 1) ? pass_fns[0] : pass_fns[i];

        auto pass_res = pass.match(
            [&pass_fn, frame_id](const GeneralPassEntry&) {
                return pass_fn(frame_id, nullptr);
            },
            [this, &pass_fn,
             frame_id](const RenderPassEntry& rp) -> result<void> {
                auto fb_res = GetFramebuffer(frame_id, rp.name.c_str());
                if (!fb_res) {
                    fb_res =
                        CreateFramebuffer(frame_id, rp.name.c_str(), rp.desc);

                    if (!fb_res) {
                        spdlog::error(
                            "Skipping pass \"{}\", could not create "
                            "framebuffer.",
                            rp.name);
                        return fb_res.error();
                    }
                }

                StartRenderPass(fb_res.value(), rp.desc);
                return pass_fn(frame_id, &rp.desc);
            });

        if (!pass_res) {
            spdlog::error("Pass \"{}\" failed, terminating rendering.",
                          pass_name);
            return pass_res;
        }

        if (rp_in_progress_) {
            vezCmdEndRenderPass();
        };
        rp_in_progress_ = false;

        // Resolve attachments
        pass.match(
            [](const GeneralPassEntry&) { return outcome::success(); },
            [&](const RenderPassEntry& rp) -> result<void> {
                for (const auto& c : rp.desc.color_attachments) {
                    if (!c.resolve_to_rt.empty()) {
                        OUTCOME_TRY(
                            src_image,
                            GetRenderTarget(frame_id, c.rt_name.c_str()));
                        OUTCOME_TRY(
                            dst_image,
                            GetRenderTarget(frame_id, c.resolve_to_rt.c_str()));

                        const auto& rt_desc_res =
                            render_plan_.color_images.find(c.rt_name);
                        if (rt_desc_res != render_plan_.color_images.end()) {
                            auto extent =
                                GetAbsoluteExtent(rt_desc_res->second.extent);

                            VezImageResolve resolve{};
                            resolve.srcSubresource = {0, 0, 1};
                            resolve.dstSubresource = {0, 0, 1};
                            resolve.extent = {
                                static_cast<uint32_t>(extent.width),
                                static_cast<uint32_t>(extent.height), 1};
                            vezCmdResolveImage(src_image->vez.image,
                                               dst_image->vez.image, 1,
                                               &resolve);
                        } else {
                            spdlog::error(
                                "Resolve failed: color image \"{}\" not found.",
                                c.rt_name);
                            return Error::NotFound;
                        }
                    }
                }

                return outcome::success();
            });

        // Blits
        const auto& blits =
            pass.match([](const GeneralPassEntry& gp) { return gp.blits; },
                       [](const RenderPassEntry& rp) { return rp.blits; });

        for (const auto& b : blits) {
            OUTCOME_TRY(src_image, GetRenderTarget(frame_id, b.src.rt.c_str()));
            OUTCOME_TRY(dst_image, GetRenderTarget(frame_id, b.dst.rt.c_str()));

            VezImageBlit blit{};
            blit.srcSubresource = {b.src.mip_level, b.src.base_array_layer,
                                   b.src.layer_count};
            blit.dstSubresource = {b.dst.mip_level, b.dst.base_array_layer,
                                   b.dst.layer_count};

            auto src_extent = GetAbsoluteExtent(b.src.extent);
            blit.srcOffsets[1] = {
                static_cast<int32_t>(src_extent.rounded_width()),
                static_cast<int32_t>(src_extent.rounded_height()), 1};

            auto dst_extent = GetAbsoluteExtent(b.dst.extent);
            blit.dstOffsets[1] = {
                static_cast<int32_t>(dst_extent.rounded_width()),
                static_cast<int32_t>(dst_extent.rounded_height()), 1};

            vezCmdBlitImage(src_image->vez.image, dst_image->vez.image, 1,
                            &blit, VK_FILTER_LINEAR);
        }
    }

    FinishFrame();
    PresentImage(present_image);

    uint32_t buf_size = config_.buffering == Buffering::Triple ? 3 : 2;
    context_.current_frame = (context_.current_frame + 1) % buf_size;

    // Ensure resources for the next frame are not in use
    auto& per_frame = context_.per_frame[context_.current_frame];
    if (per_frame.setup_fence != VK_NULL_HANDLE) {
        VK_CHECK(vezWaitForFences(context_.device, 1, &per_frame.setup_fence,
                                  VK_TRUE, ~0ULL));
        vezDestroyFence(context_.device, per_frame.setup_fence);
        per_frame.setup_fence = VK_NULL_HANDLE;
    }
    per_frame.setup_semaphore = VK_NULL_HANDLE;

    if (per_frame.submission_fence != VK_NULL_HANDLE) {
        VK_CHECK(vezWaitForFences(context_.device, 1,
                                  &per_frame.submission_fence, VK_TRUE, ~0ULL));
        vezDestroyFence(context_.device, per_frame.submission_fence);
        per_frame.submission_fence = VK_NULL_HANDLE;
    }

    // Destroy any orphaned pipelines
    std::for_each(
        std::begin(per_frame.orphaned_pipelines),
        std::end(per_frame.orphaned_pipelines),
        [this](VezPipeline p) { vezDestroyPipeline(context_.device, p); });
    per_frame.orphaned_pipelines.clear();

    return outcome::success();
}

result<void> VezBackend::BindUniformBuffer(const Buffer& buffer,
                                           uint64_t offset, uint64_t size,
                                           uint32_t binding,
                                           uint32_t array_index) {
    vezCmdBindBuffer(buffer.vez, offset, size, 0, binding, array_index);
    return outcome::success();
}

result<void> VezBackend::BindTexture(const Image& image, uint32_t binding,
                                     const SamplerDesc* sampler_override) {
    VkSampler sampler_ovr = VK_NULL_HANDLE;
    if (sampler_override != nullptr) {
        OUTCOME_TRY(s, GetSampler(*sampler_override));
        sampler_ovr = s;
    }

    vezCmdBindImageView(image.vez.image_view,
                        sampler_override ? sampler_ovr : image.vez.sampler, 0,
                        binding, 0);

    return outcome::success();
}

result<void> VezBackend::BindTextures(const std::vector<Image>& images,
                                      uint32_t first_binding,
                                      const SamplerDesc* sampler_override) {
    VkSampler sampler_ovr = VK_NULL_HANDLE;
    if (sampler_override != nullptr) {
        OUTCOME_TRY(s, GetSampler(*sampler_override));
        sampler_ovr = s;
    }

    for (const auto& image : images) {
        vezCmdBindImageView(image.vez.image_view,
                            sampler_override ? sampler_ovr : image.vez.sampler,
                            0, first_binding, 0);
    }
    return outcome::success();
}

result<void> VezBackend::BindVertexBuffer(const Buffer& vertex_buffer,
                                          uint32_t binding, size_t offset) {
    vezCmdBindVertexBuffers(binding, 1, &vertex_buffer.vez, &offset);
    return outcome::success();
}

result<void> VezBackend::BindVertexBuffers(
    const std::vector<Buffer>& vertex_buffers, uint32_t first_binding,
    std::vector<size_t> offsets) {
    std::vector<VkDeviceSize> vk_offsets;
    vk_offsets.reserve(vertex_buffers.size());

    for (const auto& o : offsets) {
        vk_offsets.push_back(static_cast<VkDeviceSize>(o));
    }
    vk_offsets.resize(vertex_buffers.size(), 0);

    std::vector<VkBuffer> buffers;
    buffers.reserve(vertex_buffers.size());

    for (const auto& vb : vertex_buffers) {
        buffers.push_back(vb.vez);
    }

    vezCmdBindVertexBuffers(first_binding,
                            static_cast<uint32_t>(buffers.size()),
                            buffers.data(), vk_offsets.data());
    return outcome::success();
}

result<void> VezBackend::BindIndexBuffer(const Buffer& index_buffer,
                                         uint64_t offset, bool short_indices) {
    vezCmdBindIndexBuffer(
        index_buffer.vez, static_cast<VkDeviceSize>(offset),
        short_indices ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
    return outcome::success();
}

result<void> VezBackend::BindGraphicsPipeline(Pipeline pipeline) {
    vezCmdBindPipeline(pipeline.vez);
    return outcome::success();
}

result<void> VezBackend::BindVertexInputFormat(
    VertexInputFormat vertex_input_format) {
    vezCmdSetVertexInputFormat(vertex_input_format.vez);
    return outcome::success();
}

result<void> VezBackend::BindDepthStencilState(const DepthStencilState& state) {
    VezDepthStencilState vez_state{};
    vez_state.depthTestEnable = state.depth_test;
    vez_state.depthWriteEnable = state.depth_write;
    vez_state.depthBoundsTestEnable = state.depth_bounds;
    vez_state.depthCompareOp = static_cast<VkCompareOp>(state.depth_compare);
    vez_state.stencilTestEnable = state.stencil_test;
    vez_state.front = {static_cast<VkStencilOp>(state.front.fail_op),
                       static_cast<VkStencilOp>(state.front.pass_op),
                       static_cast<VkStencilOp>(state.front.depth_fail_op),
                       static_cast<VkCompareOp>(state.front.compare_op)};
    vez_state.back = {static_cast<VkStencilOp>(state.back.fail_op),
                      static_cast<VkStencilOp>(state.back.pass_op),
                      static_cast<VkStencilOp>(state.back.depth_fail_op),
                      static_cast<VkCompareOp>(state.back.compare_op)};

    vezCmdSetDepthStencilState(&vez_state);
    return outcome::success();
}

result<void> VezBackend::BindColorBlendState(const ColorBlendState& state) {
    VezColorBlendState vez_state{};
    vez_state.logicOpEnable = state.color_blend;
    vez_state.logicOp = static_cast<VkLogicOp>(state.logic_op);

    if (!state.attachments.empty()) {
        std::vector<VezColorBlendAttachmentState> vez_attachments;
        for (const auto& attachment : state.attachments) {
            vez_attachments.push_back(
                {attachment.color_blend,
                 static_cast<VkBlendFactor>(attachment.src_color_blend_factor),
                 static_cast<VkBlendFactor>(attachment.dst_color_blend_factor),
                 static_cast<VkBlendOp>(attachment.color_blend_op),
                 static_cast<VkBlendFactor>(attachment.src_alpha_blend_factor),
                 static_cast<VkBlendFactor>(attachment.dst_alpha_blend_factor),
                 static_cast<VkBlendOp>(attachment.alpha_blend_op),
                 static_cast<VkColorComponentFlags>(
                     attachment.color_write_mask)});
        }

        vez_state.attachmentCount =
            static_cast<uint32_t>(vez_attachments.size());
        vez_state.pAttachments = vez_attachments.data();

        vezCmdSetColorBlendState(&vez_state);
    } else {
        vezCmdSetColorBlendState(&vez_state);
    }

    return outcome::success();
}

result<void> VezBackend::BindMultisampleState(const MultisampleState& state) {
    VezMultisampleState vez_state{};

    // Reduce state.samples to a power of 2
    uint32_t samples_po2 = std::min(state.samples, 64U);
    uint32_t mask = 64;
    while (mask) {
        if (samples_po2 & mask) {
            samples_po2 = mask;
            break;
        }
        mask >>= 1;
    }

    vez_state.rasterizationSamples =
        static_cast<VkSampleCountFlagBits>(samples_po2);
    vez_state.sampleShadingEnable = state.sample_shading;
    vez_state.minSampleShading = state.min_sample_shading;

    vezCmdSetMultisampleState(&vez_state);
    return outcome::success();
}

result<void> VezBackend::BindInputAssemblyState(
    const InputAssemblyState& state) {
    VezInputAssemblyState vez_state{};
    vez_state.topology = static_cast<VkPrimitiveTopology>(state.topology);
    vez_state.primitiveRestartEnable = state.primitive_restart;

    vezCmdSetInputAssemblyState(&vez_state);
    return outcome::success();
}

result<void> VezBackend::BindRasterizationState(
    const RasterizationState& state) {
    VezRasterizationState vez_state{};
    vez_state.depthBiasEnable = state.depth_bias;
    vez_state.depthClampEnable = state.depth_clamp;
    vez_state.cullMode = static_cast<VkCullModeFlags>(state.cull_mode);
    vez_state.frontFace = static_cast<VkFrontFace>(state.front_face);
    vez_state.polygonMode = static_cast<VkPolygonMode>(state.polygon_mode);
    vez_state.rasterizerDiscardEnable = state.rasterizer_discard;

    vezCmdSetRasterizationState(&vez_state);
    return outcome::success();
}

result<void> VezBackend::BindViewportState(uint32_t viewport_count) {
    vezCmdSetViewportState(viewport_count);
    return outcome::success();
}

result<void> VezBackend::SetDepthBias(float constant_factor, float clamp,
                                      float slope_factor) {
    vezCmdSetDepthBias(constant_factor, clamp, slope_factor);
    return outcome::success();
}

result<void> VezBackend::SetDepthBounds(float min, float max) {
    vezCmdSetDepthBounds(min, max);
    return outcome::success();
}

result<void> VezBackend::SetStencil(StencilFace face, uint32_t reference,
                                    uint32_t write_mask,
                                    uint32_t compare_mask) {
    vezCmdSetStencilReference(static_cast<VkStencilFaceFlags>(face), reference);
    vezCmdSetStencilWriteMask(static_cast<VkStencilFaceFlags>(face),
                              write_mask);
    vezCmdSetStencilCompareMask(static_cast<VkStencilFaceFlags>(face),
                                compare_mask);
    return outcome::success();
}

result<void> VezBackend::SetBlendConstants(
    const std::array<float, 4>& blend_constants) {
    vezCmdSetBlendConstants(blend_constants.data());
    return outcome::success();
}

result<void> VezBackend::SetViewport(const std::vector<Viewport> viewports,
                                     uint32_t first_viewport) {
    std::vector<VkViewport> viewports_vez;
    for (const auto& viewport : viewports) {
        viewports_vez.push_back({viewport.x, viewport.y, viewport.width,
                                 viewport.height, viewport.min_depth,
                                 viewport.max_depth});
    }

    vezCmdSetViewport(first_viewport,
                      static_cast<uint32_t>(viewports_vez.size()),
                      viewports_vez.data());
    return outcome::success();
}

result<void> VezBackend::SetScissor(const std::vector<Scissor> scissors,
                                    uint32_t first_scissor) {
    std::vector<VkRect2D> scissors_vez;
    for (const auto& scissor : scissors) {
        scissors_vez.push_back(
            {{scissor.x, scissor.y}, {scissor.width, scissor.height}});
    }

    vezCmdSetScissor(first_scissor, static_cast<uint32_t>(scissors_vez.size()),
                     scissors_vez.data());
    return outcome::success();
}

result<void> VezBackend::Draw(uint32_t vertex_count, uint32_t instance_count,
                              uint32_t first_vertex, uint32_t first_instance) {
    vezCmdDraw(vertex_count, instance_count, first_vertex, first_instance);
    return outcome::success();
}

result<void> VezBackend::DrawIndexed(uint32_t index_count,
                                     uint32_t instance_count,
                                     uint32_t first_index,
                                     uint32_t vertex_offset,
                                     uint32_t first_instance) {
    vezCmdDrawIndexed(index_count, instance_count, first_index, vertex_offset,
                      first_instance);
    return outcome::success();
}

result<size_t> VezBackend::StartFrame(uint32_t threads) {
    assert(context_.device &&
           "Context must be initialized before starting a frame");
    VkDevice device = context_.device;

    if (threads == 0) {
        threads = 1;
    }

    auto& per_frame = context_.per_frame[context_.current_frame];

    auto command_buffer_count = per_frame.command_buffers.size();
    if (threads > command_buffer_count) {
        VkQueue graphics_queue = VK_NULL_HANDLE;
        vezGetDeviceGraphicsQueue(device, 0, &graphics_queue);

        VezCommandBufferAllocateInfo cmd_info{};
        cmd_info.commandBufferCount =
            static_cast<uint32_t>(threads - command_buffer_count);
        cmd_info.queue = graphics_queue;

        per_frame.command_buffers.resize(threads);
        VK_CHECK(vezAllocateCommandBuffers(
            context_.device, &cmd_info,
            &per_frame.command_buffers[command_buffer_count]));
    }

    per_frame.command_buffer_active.resize(threads);
    for (size_t i = 0; i < per_frame.command_buffer_active.size(); i++) {
        per_frame.command_buffer_active[i] = false;
    }

    return context_.current_frame;
}

result<void> VezBackend::StartRenderPass(Framebuffer fb,
                                         RenderPassDesc rp_desc) {
    assert(context_.device &&
           "Context must be initialized before starting a render pass");

    // Ensure there is a valid command buffer for the current thread
    OUTCOME_TRY(GetActiveCommandBuffer());

    if (rp_in_progress_) {
        vezCmdEndRenderPass();
    };
    rp_in_progress_ = false;

    std::vector<VezAttachmentInfo> attach_infos;

    for (auto attachment : rp_desc.color_attachments) {
        attach_infos.push_back(
            {attachment.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR
                              : VK_ATTACHMENT_LOAD_OP_LOAD,
             attachment.store ? VK_ATTACHMENT_STORE_OP_STORE
                              : VK_ATTACHMENT_STORE_OP_DONT_CARE,
             {attachment.clear_color[0], attachment.clear_color[1],
              attachment.clear_color[2], attachment.clear_color[3]}});
    }

    if (!rp_desc.depth_attachment.rt_name.empty()) {
        VezAttachmentInfo depth_info{
            rp_desc.depth_attachment.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                           : VK_ATTACHMENT_LOAD_OP_LOAD,
            rp_desc.depth_attachment.store ? VK_ATTACHMENT_STORE_OP_STORE
                                           : VK_ATTACHMENT_STORE_OP_DONT_CARE};
        depth_info.clearValue.depthStencil = {
            rp_desc.depth_attachment.clear_depth,
            rp_desc.depth_attachment.clear_stencil};
        attach_infos.push_back(depth_info);
    }

    VezRenderPassBeginInfo rp_info{};
    rp_info.framebuffer = fb.vez;
    rp_info.attachmentCount = static_cast<uint32_t>(attach_infos.size());
    rp_info.pAttachments = attach_infos.data();

    vezCmdBeginRenderPass(&rp_info);
    rp_in_progress_ = true;
    return outcome::success();
}

result<void> VezBackend::FinishFrame() {
    VkDevice device = context_.device;

    VkQueue graphics_queue = VK_NULL_HANDLE;
    vezGetDeviceGraphicsQueue(device, 0, &graphics_queue);

    if (rp_in_progress_) {
        vezCmdEndRenderPass();
    }
    rp_in_progress_ = false;

    vezEndCommandBuffer();

    auto& per_frame = context_.per_frame[context_.current_frame];

    // Submit the command buffer for the current thread
    VezSubmitInfo submit_info{};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &per_frame.command_buffers[0];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &per_frame.submission_semaphore;

    std::vector<VkSemaphore> wait_semaphores;
    std::vector<VkPipelineStageFlags> wait_dst;

    if (per_frame.setup_semaphore != VK_NULL_HANDLE) {
        wait_semaphores.push_back(per_frame.setup_semaphore);
        wait_dst.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }

    if (per_frame.presentation_semaphore != VK_NULL_HANDLE) {
        wait_semaphores.push_back(per_frame.presentation_semaphore);
        wait_dst.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }

    submit_info.waitSemaphoreCount =
        static_cast<uint32_t>(wait_semaphores.size());
    submit_info.pWaitSemaphores = wait_semaphores.data();
    submit_info.pWaitDstStageMask = wait_dst.data();

    VK_CHECK(vezQueueSubmit(graphics_queue, 1, &submit_info,
                            &per_frame.submission_fence));
    return outcome::success();
}

result<void> VezBackend::PresentImage(const char* present_image_name) {
    VkDevice device = context_.device;

    OUTCOME_TRY(fb_image,
                GetRenderTarget(context_.current_frame, present_image_name));
    VkImage present_image = fb_image->vez.image;

    VkPipelineStageFlags wait_dst =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkQueue graphics_queue = VK_NULL_HANDLE;
    vezGetDeviceGraphicsQueue(device, 0, &graphics_queue);

    auto& per_frame = context_.per_frame[context_.current_frame];
    per_frame.presentation_semaphore = VK_NULL_HANDLE;

    VezPresentInfo present_info{};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &per_frame.submission_semaphore;
    present_info.pWaitDstStageMask = &wait_dst;
    present_info.signalSemaphoreCount = 1;
    present_info.pSignalSemaphores = &per_frame.presentation_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &context_.swapchain;
    present_info.pImages = &present_image;

    VK_CHECK(vezQueuePresent(graphics_queue, &present_info));

    return outcome::success();
};

result<void> VezBackend::TeardownContext() {
    if (context_.device) {
        vezDeviceWaitIdle(context_.device);
    }

    for (auto& per_frame : context_.per_frame) {
        if (per_frame.setup_command_buffer != VK_NULL_HANDLE) {
            vezFreeCommandBuffers(context_.device, 1,
                                  &per_frame.setup_command_buffer);
            per_frame.setup_command_buffer = VK_NULL_HANDLE;
        }

        vezFreeCommandBuffers(
            context_.device,
            static_cast<uint32_t>(per_frame.command_buffers.size()),
            per_frame.command_buffers.data());
    }

    for (auto& vertex_input : context_.vertex_input_format_cache) {
        vezDestroyVertexInputFormat(context_.device, vertex_input.second->vez);
        vertex_input.second->valid = false;
    }

    for (auto framebuffer : context_.framebuffer_cache) {
        vezDestroyFramebuffer(context_.device, framebuffer.second);
    }

    for (auto image : context_.texture_cache) {
        vezDestroyImageView(context_.device, image.second->vez.image_view);
        vezDestroyImage(context_.device, image.second->vez.image);
    }

    for (auto image : context_.fb_image_cache) {
        vezDestroyImageView(context_.device, image.second->vez.image_view);
        vezDestroyImage(context_.device, image.second->vez.image);
    }

    for (auto sampler : context_.sampler_cache) {
        vezDestroySampler(context_.device, sampler.second);
    }

    for (auto buffer : context_.buffer_cache) {
        vezDestroyBuffer(context_.device, buffer.second->vez);
        buffer.second->valid = false;
    }
    context_.buffer_cache.clear();

    for (auto pipeline : context_.pipeline_cache) {
        vezDestroyPipeline(context_.device, pipeline.second->vez);
        pipeline.second->valid = false;
    }

    for (auto shader : context_.vertex_shader_cache) {
        vezDestroyShaderModule(context_.device, shader.second);
    }

    for (auto shader : context_.fragment_shader_cache) {
        vezDestroyShaderModule(context_.device, shader.second);
    }

    if (context_.swapchain) {
        vezDestroySwapchain(context_.device, context_.swapchain);
    }

    if (context_.surface) {
        vkDestroySurfaceKHR(context_.instance, context_.surface, nullptr);
    }

    if (context_.device) {
        vezDestroyDevice(context_.device);
    }

    if (context_.debug_callback) {
        vkDestroyDebugReportCallbackEXT(context_.instance,
                                        context_.debug_callback, nullptr);
    }

    if (context_.instance) {
        vezDestroyInstance(context_.instance);
    }

    context_ = VezContext();
    return outcome::success();
}

result<VkInstance> VezBackend::CreateInstance() {
    VezApplicationInfo appInfo{};
    appInfo.pApplicationName = "Goma App";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.pEngineName = "Goma Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);

    std::vector<const char*> enabled_layers{
        // "VK_LAYER_LUNARG_standard_validation",
        "VK_LAYER_LUNARG_monitor",
    };

    uint32_t instance_layer_count;
    vezEnumerateInstanceLayerProperties(&instance_layer_count, nullptr);

    std::vector<VkLayerProperties> instance_layers(instance_layer_count);
    vezEnumerateInstanceLayerProperties(&instance_layer_count,
                                        instance_layers.data());

    std::vector<const char*> instance_layer_names;
    for (auto& enabled_layer : enabled_layers) {
        for (auto& layer : instance_layers) {
            if (!strcmp(layer.layerName, enabled_layer)) {
                instance_layer_names.push_back(layer.layerName);
                break;
            }
        }
    }

    std::vector<const char*> enabled_extensions{
        "VK_KHR_surface", "VK_KHR_win32_surface", "VK_EXT_debug_report"};

#ifndef BYPASS_EXTENSION_ENUMERATION
    uint32_t instance_extension_count;
    vezEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count,
                                            nullptr);

    std::vector<VkExtensionProperties> instance_extensions(
        instance_extension_count);
    vezEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count,
                                            instance_extensions.data());

    std::vector<const char*> instance_extension_names;
    for (auto& enabled_extension : enabled_extensions) {
        for (auto& extension : instance_extensions) {
            if (!strcmp(extension.extensionName, enabled_extension)) {
                instance_extension_names.push_back(extension.extensionName);
                break;
            }
        }
    }
#else
    std::vector<const char*> instance_extension_names =
        std::move(enabled_extensions);
#endif

    VezInstanceCreateInfo instance_info{};
    instance_info.pApplicationInfo = &appInfo;
    instance_info.enabledLayerCount =
        static_cast<uint32_t>(instance_layer_names.size());
    instance_info.ppEnabledLayerNames = instance_layer_names.data();
    instance_info.enabledExtensionCount =
        static_cast<uint32_t>(instance_extension_names.size());
    instance_info.ppEnabledExtensionNames = instance_extension_names.data();

    VkInstance instance = VK_NULL_HANDLE;
    VK_CHECK(vezCreateInstance(&instance_info, &instance));
    volkLoadInstance(instance);
    return instance;
}

result<VkDebugReportCallbackEXT> VezBackend::CreateDebugCallback(
    VkInstance instance) {
    VkDebugReportCallbackCreateInfoEXT debug_callback_info{};
    debug_callback_info.sType =
        VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debug_callback_info.flags =
        // VK_DEBUG_REPORT_DEBUG_BIT_EXT |
        // VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
        // VK_DEBUG_REPORT_WARNING_BIT_EXT |
        // VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_ERROR_BIT_EXT;
    debug_callback_info.pfnCallback = &DebugReportCallback;

    VkDebugReportCallbackEXT debug_callback = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDebugReportCallbackEXT(instance, &debug_callback_info,
                                            nullptr, &debug_callback));
    return debug_callback;
}

result<VezBackend::PhysicalDevice> VezBackend::CreatePhysicalDevice(
    VkInstance instance) {
    uint32_t physical_device_count;
    vezEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);

    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    vezEnumeratePhysicalDevices(instance, &physical_device_count,
                                physical_devices.data());

    PhysicalDevice p{physical_devices[0]};
    vezGetPhysicalDeviceProperties(p.physical_device, &p.properties);
    vezGetPhysicalDeviceFeatures(p.physical_device, &p.features);

    spdlog::info("Physical device: {}, driver version: {}",
                 p.properties.deviceName, p.properties.driverVersion);

    return p;
}

result<VkDevice> VezBackend::CreateDevice(VkPhysicalDevice physical_device) {
    std::vector<const char*> enabled_extensions{"VK_KHR_swapchain"};

#ifndef BYPASS_EXTENSION_ENUMERATION
    uint32_t device_extension_count;
    vezEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                          &device_extension_count, nullptr);

    std::vector<VkExtensionProperties> device_extensions(
        device_extension_count);
    vezEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                          &device_extension_count,
                                          device_extensions.data());

    std::vector<const char*> device_extension_names;
    for (auto& enabled_extension : enabled_extensions) {
        for (auto& extension : device_extensions) {
            if (!strcmp(extension.extensionName, enabled_extension)) {
                device_extension_names.push_back(extension.extensionName);
                break;
            }
        }
    }
#else
    std::vector<const char*> device_extension_names =
        std::move(enabled_extensions);
#endif

    VezDeviceCreateInfo device_info{};
    device_info.enabledExtensionCount =
        static_cast<uint32_t>(device_extension_names.size());
    device_info.ppEnabledExtensionNames = device_extension_names.data();

    VkDevice device = VK_NULL_HANDLE;
    VK_CHECK(vezCreateDevice(physical_device, &device_info, &device));
    volkLoadDevice(device);
    return device;
}

result<VezSwapchain> VezBackend::CreateSwapchain(VkSurfaceKHR surface) {
    assert(context_.device &&
           "Context must be initialized before creating a swapchain");
    VkDevice device = context_.device;

    VkFormat format = config_.fb_color_space == FramebufferColorSpace::Linear
                          ? VK_FORMAT_B8G8R8A8_UNORM
                          : VK_FORMAT_B8G8R8A8_SRGB;

    VezSwapchainCreateInfo swapchain_info{};
    swapchain_info.surface = surface;
    swapchain_info.tripleBuffer = VK_TRUE;
    swapchain_info.format = {format, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

    VezSwapchain swapchain = VK_NULL_HANDLE;
    VK_CHECK(vezCreateSwapchain(device, &swapchain_info, &swapchain));

    vezGetSwapchainSurfaceFormat(swapchain, &context_.swapchain_format);
    spdlog::info("Swapchain format: {}, {}", context_.swapchain_format.format,
                 context_.swapchain_format.colorSpace);

    return swapchain;
}

result<VkShaderModule> VezBackend::GetVertexShaderModule(
    const ShaderDesc& vert) {
    assert(context_.device &&
           "Context must be initialized before creating a shader");
    VkDevice device = context_.device;

    auto hash = GetShaderHash(vert.source.c_str(), vert.preamble.c_str(),
                              vert.entry_point.c_str());
    auto result = context_.vertex_shader_cache.find(hash);

    if (result != context_.vertex_shader_cache.end()) {
        return result->second;
    } else {
        std::string buffer;
        if (vert.source_type == ShaderSourceType::Filename) {
            std::ifstream t(vert.source,
                            std::ios_base::in | std::ios_base::binary);
            t.seekg(0, std::ios::end);
            size_t size = t.tellg();

            buffer.resize(size);
            t.seekg(0);
            t.read(&buffer[0], size);
        }

        VezShaderModuleCreateInfo shader_info{};
        shader_info.stage = VK_SHADER_STAGE_VERTEX_BIT;

        if (vert.source_type == ShaderSourceType::Filename) {
            shader_info.pGLSLSource = buffer.c_str();
        } else {
            shader_info.pGLSLSource = vert.source.c_str();
        }
        shader_info.pPreamble = vert.preamble.c_str();
        shader_info.pEntryPoint = vert.entry_point.c_str();

        VkShaderModule shader = VK_NULL_HANDLE;
        auto shader_compilation_result =
            vezCreateShaderModule(device, &shader_info, &shader);

        if (shader_compilation_result == VK_ERROR_INITIALIZATION_FAILED) {
            // The info log is stored in the V-EZ shader object
            auto vez_shader = reinterpret_cast<vez::ShaderModule*>(shader);
            spdlog::error("Vertex shader compilation failed:\n{}",
                          vez_shader->GetInfoLog());
        }
        VK_CHECK(shader_compilation_result);

        context_.vertex_shader_cache[hash] = shader;
        return shader;
    }
}

result<VkShaderModule> VezBackend::GetFragmentShaderModule(
    const ShaderDesc& frag) {
    assert(context_.device &&
           "Context must be initialized before creating a shader");
    VkDevice device = context_.device;

    auto hash = GetShaderHash(frag.source.c_str(), frag.preamble.c_str(),
                              frag.entry_point.c_str());
    auto result = context_.fragment_shader_cache.find(hash);

    if (result != context_.fragment_shader_cache.end()) {
        return result->second;
    } else {
        std::string buffer;
        if (frag.source_type == ShaderSourceType::Filename) {
            std::ifstream t(frag.source,
                            std::ios_base::in | std::ios_base::binary);
            t.seekg(0, std::ios::end);
            size_t size = t.tellg();

            buffer.resize(size);
            t.seekg(0);
            t.read(&buffer[0], size);
        }

        VezShaderModuleCreateInfo shader_info{};
        shader_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

        if (frag.source_type == ShaderSourceType::Filename) {
            shader_info.pGLSLSource = buffer.c_str();
        } else {
            shader_info.pGLSLSource = frag.source.c_str();
        }
        shader_info.pPreamble = frag.preamble.c_str();
        shader_info.pEntryPoint = frag.entry_point.c_str();

        VkShaderModule shader = VK_NULL_HANDLE;
        auto shader_compilation_result =
            vezCreateShaderModule(device, &shader_info, &shader);

        if (shader_compilation_result == VK_ERROR_INITIALIZATION_FAILED) {
            // The info log is stored in the V-EZ shader object
            auto vez_shader = reinterpret_cast<vez::ShaderModule*>(shader);
            spdlog::error("Fragment shader compilation failed:\n{}",
                          vez_shader->GetInfoLog());
        }
        VK_CHECK(shader_compilation_result);

        context_.fragment_shader_cache[hash] = shader;
        return shader;
    }
}

result<std::shared_ptr<Buffer>> VezBackend::CreateBuffer(
    VezContext::BufferHash hash, VkDeviceSize size, VezMemoryFlagsBits storage,
    VkBufferUsageFlags usage, void* initial_contents) {
    assert(context_.device &&
           "Context must be initialized before creating a buffer");
    VkDevice device = context_.device;

    auto result = context_.buffer_cache.find(hash);
    if (result != context_.buffer_cache.end()) {
        vezDestroyBuffer(device, result->second->vez);
        result->second->valid = false;
    }

    VezBufferCreateInfo buffer_info{};
    buffer_info.size = size;
    buffer_info.usage = usage;

    VkBuffer buffer = VK_NULL_HANDLE;
    VK_CHECK(vezCreateBuffer(device, storage, &buffer_info, &buffer));

    if (initial_contents) {
        if (storage != VEZ_MEMORY_GPU_ONLY) {
            void* buffer_memory;
            VK_CHECK(vezMapBuffer(device, buffer, 0, size, &buffer_memory));
            memcpy(buffer_memory, initial_contents, size);

            VezMappedBufferRange range{};
            range.buffer = buffer;
            range.offset = 0;
            range.size = size;
            VK_CHECK(vezFlushMappedBufferRanges(device, 1, &range));

            vezUnmapBuffer(device, buffer);
        } else {
            // GPU-only buffer can't be mapped, so we use V-EZ's utility
            // function to submit data
            VK_CHECK(
                vezBufferSubData(device, buffer, 0, size, initial_contents));
        }
    }

    auto ret = std::make_shared<Buffer>(buffer);
    context_.buffer_cache[hash] = ret;
    return ret;
}

result<std::shared_ptr<Buffer>> VezBackend::GetBuffer(
    VezContext::BufferHash hash) {
    auto result = context_.buffer_cache.find(hash);
    if (result != context_.buffer_cache.end()) {
        return result->second;
    }

    return Error::NotFound;
}

result<std::shared_ptr<Image>> VezBackend::CreateRenderTarget(
    FrameIndex frame_id, const char* name,
    const ColorRenderTargetDesc& image_desc) {
    auto hash = GetRenderTargetHash(frame_id, name);
    auto result = context_.fb_image_cache.find(hash);
    if (result != context_.fb_image_cache.end()) {
        vezDestroyImageView(context_.device, result->second->vez.image_view);
        vezDestroyImage(context_.device, result->second->vez.image);
    }

    VkFormat format = GetVkFormat(image_desc.format);
    Extent extent = GetAbsoluteExtent(image_desc.extent);

    VezImageCreateInfo image_info{};
    image_info.extent = {extent.rounded_width(), extent.rounded_height(), 1};
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.arrayLayers = 1;
    image_info.mipLevels = image_desc.mip_levels;
    image_info.format = format;
    image_info.samples = static_cast<VkSampleCountFlagBits>(image_desc.samples);
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    OUTCOME_TRY(vulkan_image, CreateImage(hash, image_info));
    OUTCOME_TRY(sampler, GetSampler(image_desc.sampler));
    vulkan_image.sampler = sampler;

    auto ret = std::make_shared<Image>(vulkan_image);
    context_.fb_image_cache[hash] = ret;
    return ret;
}

result<std::shared_ptr<Image>> VezBackend::CreateRenderTarget(
    FrameIndex frame_id, const char* name,
    const DepthRenderTargetDesc& image_desc) {
    auto hash = GetRenderTargetHash(frame_id, name);
    auto result = context_.fb_image_cache.find(hash);
    if (result != context_.fb_image_cache.end()) {
        vezDestroyImageView(context_.device, result->second->vez.image_view);
        vezDestroyImage(context_.device, result->second->vez.image);
    }

    VkFormat format = GetVkFormat(image_desc.format);
    Extent extent = GetAbsoluteExtent(image_desc.extent);

    VezImageCreateInfo image_info{};
    image_info.extent = {extent.rounded_width(), extent.rounded_height(), 1};
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.arrayLayers = 1;
    image_info.mipLevels = 1;
    image_info.format = format;
    image_info.samples = static_cast<VkSampleCountFlagBits>(image_desc.samples);
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT;

    OUTCOME_TRY(vulkan_image, CreateImage(hash, image_info));
    OUTCOME_TRY(sampler, GetSampler(image_desc.sampler));
    vulkan_image.sampler = sampler;

    auto ret = std::make_shared<Image>(vulkan_image);
    context_.fb_image_cache[hash] = ret;
    return ret;
}

result<std::shared_ptr<Image>> VezBackend::GetRenderTarget(FrameIndex frame_id,
                                                           const char* name) {
    auto hash = GetRenderTargetHash(frame_id, name);
    auto result = context_.fb_image_cache.find(hash);
    if (result != context_.fb_image_cache.end()) {
        return result->second;
    } else {
        auto desc_res = render_plan_.color_images.find(name);
        if (desc_res != render_plan_.color_images.end()) {
            OUTCOME_TRY(fb_image,
                        CreateRenderTarget(frame_id, name, desc_res->second));
            return fb_image;
        }

        auto depth_desc_res = render_plan_.depth_images.find(name);
        if (depth_desc_res != render_plan_.depth_images.end()) {
            OUTCOME_TRY(fb_image, CreateRenderTarget(frame_id, name,
                                                     depth_desc_res->second));
            return fb_image;
        }
    }

    return Error::NotFound;
}

result<VkSampler> VezBackend::GetSampler(const SamplerDesc& sampler_desc) {
    auto hash = GetSamplerHash(sampler_desc);
    auto result = context_.sampler_cache.find(hash);

    if (result != context_.sampler_cache.end()) {
        return result->second;
    } else {
        VezSamplerCreateInfo sampler_info{};

        VkSamplerAddressMode address_mode{};
        switch (sampler_desc.addressing_mode) {
            case TextureWrappingMode::Repeat:
                address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                break;
            case TextureWrappingMode::MirroredRepeat:
                address_mode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
                break;
            case TextureWrappingMode::ClampToEdge:
                address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                break;
            default:
                address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                break;
        }

        VkSamplerMipmapMode mipmap_mode{};
        switch (sampler_desc.mipmap_mode) {
            case FilterType::Linear:
                mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                break;
            case FilterType::Nearest:
            default:
                mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
                break;
        }

        VkFilter filter{};
        switch (sampler_desc.filter_type) {
            case FilterType::Linear:
                filter = VK_FILTER_LINEAR;
                break;
            case FilterType::Nearest:
            default:
                filter = VK_FILTER_NEAREST;
                break;
        }

        sampler_info.addressModeU = address_mode;
        sampler_info.addressModeV = address_mode;
        sampler_info.addressModeW = address_mode;
        if (sampler_desc.anisotropy > 0) {
            sampler_info.anisotropyEnable = VK_TRUE;
            sampler_info.maxAnisotropy = sampler_desc.anisotropy;
        }
        sampler_info.minLod = sampler_desc.min_lod;
        sampler_info.maxLod = sampler_desc.max_lod;
        sampler_info.mipLodBias = sampler_desc.lod_bias;
        sampler_info.mipmapMode = mipmap_mode;
        sampler_info.minFilter = filter;
        sampler_info.magFilter = filter;
        sampler_info.compareEnable =
            sampler_desc.compare_op != CompareOp::Never;
        sampler_info.compareOp =
            static_cast<VkCompareOp>(sampler_desc.compare_op);

        VkSampler sampler = VK_NULL_HANDLE;
        vezCreateSampler(context_.device, &sampler_info, &sampler);
        context_.sampler_cache[hash] = sampler;
        return sampler;
    }
}

result<VulkanImage> VezBackend::CreateImage(VezContext::ImageHash hash,
                                            VezImageCreateInfo image_info) {
    assert(context_.device &&
           "Context must be initialized before creating an image");
    VkDevice device = context_.device;

    VkImage image = VK_NULL_HANDLE;
    VK_CHECK(vezCreateImage(device, VEZ_MEMORY_GPU_ONLY, &image_info, &image));

    VezImageSubresourceRange range{};
    range.layerCount = image_info.arrayLayers;
    range.levelCount = image_info.mipLevels;

    VezImageViewCreateInfo image_view_info{};
    image_view_info.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    image_view_info.format = image_info.format;
    image_view_info.image = image;
    image_view_info.subresourceRange = range;

    // Check for cubemap
    image_view_info.viewType = image_info.arrayLayers == 6
                                   ? VK_IMAGE_VIEW_TYPE_CUBE
                                   : VK_IMAGE_VIEW_TYPE_2D;

    VkImageView image_view;
    VK_CHECK(vezCreateImageView(device, &image_view_info, &image_view));

    VulkanImage vulkan_image{image, image_view};
    return vulkan_image;
}

result<void> VezBackend::FillImage(VkImage image, VkExtent2D extent,
                                   std::vector<void*> initial_contents) {
    uint32_t i = 0;
    for (auto& data : initial_contents) {
        VezImageSubresourceLayers layers{};
        layers.baseArrayLayer = i++;
        layers.layerCount = 1;
        layers.mipLevel = 0;

        VezImageSubDataInfo sub_data_info{};
        sub_data_info.imageExtent = {extent.width, extent.height, 1};
        sub_data_info.imageOffset = {0, 0, 0};
        sub_data_info.imageSubresource = layers;

        vezImageSubData(context_.device, image, &sub_data_info, data);
    }

    return outcome::success();
}

result<void> VezBackend::GenerateMipmaps(VkImage image, uint32_t layer_count,
                                         VkExtent2D extent) {
    GetSetupCommandBuffer();

    uint32_t min_dim = std::min(extent.width, extent.height);
    auto mip_levels = static_cast<uint32_t>(floor(log2(min_dim) + 1));
    mip_levels = std::max(1U, mip_levels);

    int32_t last_w = static_cast<int32_t>(extent.width);
    int32_t last_h = static_cast<int32_t>(extent.height);

    for (uint32_t i = 1; i < mip_levels; i++) {
        int32_t w = std::max(1, last_w / 2);
        int32_t h = std::max(1, last_h / 2);

        VezImageBlit blit{};
        blit.srcSubresource = {i - 1, 0, layer_count};
        blit.srcOffsets[1] = {last_w, last_h, 1};
        blit.dstSubresource = {i, 0, layer_count};
        blit.dstOffsets[1] = {w, h, 1};

        vezCmdBlitImage(image, image, 1, &blit, VK_FILTER_LINEAR);

        last_w = w;
        last_h = h;
    }

    return outcome::success();
}

result<void> VezBackend::GetSetupCommandBuffer() {
    auto& per_frame = context_.per_frame[context_.current_frame];

    if (per_frame.setup_command_buffer == VK_NULL_HANDLE) {
        VkQueue graphics_queue = VK_NULL_HANDLE;
        vezGetDeviceGraphicsQueue(context_.device, 0, &graphics_queue);

        VezCommandBufferAllocateInfo cmd_info{};
        cmd_info.commandBufferCount = 1;
        cmd_info.queue = graphics_queue;

        VK_CHECK(vezAllocateCommandBuffers(context_.device, &cmd_info,
                                           &per_frame.setup_command_buffer));
    }

    if (!per_frame.setup_command_buffer_active) {
        VK_CHECK(
            vezBeginCommandBuffer(per_frame.setup_command_buffer,
                                  VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT));
        per_frame.setup_command_buffer_active = true;
    }

    return outcome::success();
}

result<void> VezBackend::GetActiveCommandBuffer(uint32_t thread) {
    auto& per_frame = context_.per_frame[context_.current_frame];

    if (per_frame.setup_command_buffer_active) {
        per_frame.setup_command_buffer_active = false;
        vezEndCommandBuffer();

        // Submit the setup command buffer
        VezSubmitInfo submit_info{};
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &per_frame.setup_command_buffer;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &per_frame.setup_semaphore;

        VkQueue graphics_queue = VK_NULL_HANDLE;
        vezGetDeviceGraphicsQueue(context_.device, 0, &graphics_queue);

        VK_CHECK(vezQueueSubmit(graphics_queue, 1, &submit_info,
                                &per_frame.setup_fence));
    }

    if (!per_frame.command_buffer_active[thread]) {
        VK_CHECK(
            vezBeginCommandBuffer(per_frame.command_buffers[thread],
                                  VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT));
        per_frame.command_buffer_active[thread] = true;
    }

    return outcome::success();
}

VkFormat VezBackend::GetVkFormat(Format format) {
    switch (format) {
        case Format::UNormRGBA8:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::UNormBGRA8:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case Format::SrgbRGBA8:
            return VK_FORMAT_R8G8B8A8_SRGB;
        case Format::SFloatRGBA32:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::SFloatRGB32:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case Format::SFloatRG32:
            return VK_FORMAT_R32G32_SFLOAT;
        case Format::SFloatR32:
            return VK_FORMAT_R32_SFLOAT;
        case Format::DepthOnly:
            return VK_FORMAT_D32_SFLOAT;
        case Format::DepthStencil:
            return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case Format::SwapchainFormat:
        case Format::Undefined:
        default:
            // We default to the swapchain format even
            // if the format is undefined
            return context_.swapchain_format.format;
    }
}

Extent VezBackend::GetAbsoluteExtent(Extent extent) {
    switch (extent.type) {
        case ExtentType::Absolute:
            return extent;
        case ExtentType::RelativeToSwapchain:
        default:
            auto w = context_.capabilities.currentExtent.width;
            auto h = context_.capabilities.currentExtent.height;
            return Extent{extent.width * w, extent.height * h,
                          ExtentType::Absolute};
    }
}

VezContext::BufferHash VezBackend::GetBufferHash(const char* name) {
    return {sdbm_hash(name)};
}

VezContext::BufferHash VezBackend::GetBufferHash(BufferType type,
                                                 const GenIndex& index,
                                                 const char* name) {
    return {static_cast<uint64_t>(type), index.id, index.gen, sdbm_hash(name)};
}

VezContext::ShaderHash VezBackend::GetShaderHash(const char* source,
                                                 const char* preamble,
                                                 const char* entry_point) {
    return {sdbm_hash(source), sdbm_hash(preamble), sdbm_hash(entry_point)};
}

VezContext::PipelineHash VezBackend::GetGraphicsPipelineHash(
    VkShaderModule vs, VkShaderModule fs) {
    return {vs, fs};
}

VezContext::VertexInputFormatHash VezBackend::GetVertexInputFormatHash(
    const VertexInputFormatDesc& desc) {
    VezContext::VertexInputFormatHash hash;

    union BindingAttributeBitField {
        struct {
            uint32_t b_binding : 8;
            uint32_t b_stride : 16;
            bool b_per_instance : 1;
            uint32_t a_location : 8;
            uint32_t a_binding : 8;
            uint32_t a_offset : 16;
            Format a_format : 7;
        };

        uint64_t int_repr;
    };

    size_t max_size = std::max(desc.bindings.size(), desc.attributes.size());
    for (size_t i = 0; i < max_size; i++) {
        BindingAttributeBitField bit_field;
        if (i < desc.bindings.size()) {
            const auto& binding = desc.bindings[i];
            bit_field.b_binding = binding.binding;
            bit_field.b_stride = binding.stride;
            bit_field.b_per_instance = binding.per_instance;
        }

        if (i < desc.attributes.size()) {
            const auto& attribute = desc.attributes[i];
            bit_field.a_location = attribute.location;
            bit_field.a_binding = attribute.binding;
            bit_field.a_offset = attribute.offset;
            bit_field.a_format = attribute.format;
        }

        hash.push_back(bit_field.int_repr);
    }

    return hash;
}

VezContext::ImageHash VezBackend::GetTextureHash(const char* name) {
    return {sdbm_hash(name)};
}

VezContext::ImageHash VezBackend::GetRenderTargetHash(FrameIndex frame_id,
                                                      const char* name) {
    return {frame_id, sdbm_hash(name)};
}

VezContext::FramebufferHash VezBackend::GetFramebufferHash(FrameIndex frame_id,
                                                           const char* name) {
    return {frame_id, sdbm_hash(name)};
}

VezContext::SamplerHash VezBackend::GetSamplerHash(
    const SamplerDesc& sampler_desc) {
    struct SamplerHashBitField {
        float min_lod;
        float max_lod;
        float lod_bias;
        float anisotropy;
        FilterType filter_type : 4;
        FilterType mipmap_mode : 4;
        TextureWrappingMode addressing_mode : 4;
        CompareOp compare_op : 3;
    };

    SamplerHashBitField bit_field{
        sampler_desc.min_lod,
        sampler_desc.max_lod,
        sampler_desc.lod_bias,
        sampler_desc.anisotropy,
        static_cast<int>(sampler_desc.filter_type),
        static_cast<int>(sampler_desc.mipmap_mode),
        static_cast<int>(sampler_desc.addressing_mode),
        static_cast<int>(sampler_desc.compare_op),
    };

    std::vector<uint64_t> hash(3);
    memcpy(hash.data(), &bit_field, sizeof(bit_field));

    return std::move(hash);
}

}  // namespace goma
