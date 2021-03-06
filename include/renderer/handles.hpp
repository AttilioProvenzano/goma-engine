#pragma once

#include "common/include.hpp"
#include "common/vez.hpp"

namespace goma {

struct VulkanImage {
    VkImage image{VK_NULL_HANDLE};
    VkImageView image_view{VK_NULL_HANDLE};
    VkSampler sampler{VK_NULL_HANDLE};
};

struct Image {
    VulkanImage vez{};
    bool valid{false};

    Image(VulkanImage vez_) : vez(vez_), valid(true) {}
};

struct Framebuffer {
    VezFramebuffer vez{};
    bool valid{false};

    Framebuffer(VezFramebuffer vez_) : vez(vez_), valid(true) {}
};

struct Pipeline {
    VezPipeline vez{VK_NULL_HANDLE};
    bool valid{false};

    Pipeline(VezPipeline vez_) : vez(vez_), valid(true) {}
};

struct Buffer {
    VkBuffer vez{VK_NULL_HANDLE};
    bool valid{false};

    Buffer(VkBuffer vez_) : vez(vez_), valid(true) {}
};

struct Viewport {
    float width;
    float height;
    float x{0.0f};
    float y{0.0f};
    float min_depth{0.0f};
    float max_depth{1.0f};
};

struct Scissor {
    uint32_t width;
    uint32_t height;
    int32_t x{0};
    int32_t y{0};
};

struct VertexInputFormat {
    const VezVertexInputFormat vez{VK_NULL_HANDLE};
    bool valid{false};

    VertexInputFormat(VezVertexInputFormat vez_) : vez(vez_), valid(true) {}
};

enum class Format {
    Undefined,
    SwapchainFormat,
    UNormRGBA8,
    UNormBGRA8,
    SrgbRGBA8,
    SFloatRGBA32,
    SFloatRGB32,
    SFloatRG32,
    SFloatR32,
    DepthOnly,
    DepthStencil
};

enum class FilterType { Nearest, Linear };

enum class TextureType {
    Diffuse,  // also Albedo for PBR
    Specular,
    Ambient,
    Emissive,
    MetallicRoughness,
    HeightMap,
    NormalMap,
    Shininess,
    Opacity,
    Displacement,
    LightMap,  // also OcclusionMap
    Reflection,
};

enum class TextureOp {
    Multiply,
    Add,
    Subtract,
    Divide,
    SmoothAdd,  // (T1 + T2) - (T1 * T2)
    SignedAdd   // T1 + (T2 - 0.5)
};

enum class TextureWrappingMode { Repeat, MirroredRepeat, ClampToEdge, Decal };

enum class CompareOp {
    Never = 0,
    Less = 1,
    Equal = 2,
    LessOrEqual = 3,
    Greater = 4,
    NotEqual = 5,
    GreaterOrEqual = 6,
    Always = 7
};

struct SamplerDesc {
    FilterType filter_type{FilterType::Linear};
    FilterType mipmap_mode{FilterType::Linear};

    float min_lod{0.0f};
    float max_lod{std::numeric_limits<float>::max()};
    float lod_bias{0.0f};

    TextureWrappingMode addressing_mode{TextureWrappingMode::Repeat};
    float anisotropy{16.0f};
    CompareOp compare_op{CompareOp::Never};
};

struct TextureDesc {
    uint32_t width;
    uint32_t height;
    Format format{Format::UNormRGBA8};

    bool mipmapping{true};
    uint32_t array_layers{1};
    uint32_t samples{1};

    SamplerDesc sampler{};
};

struct CubemapContents {
    void* right{nullptr};
    void* left{nullptr};
    void* up{nullptr};
    void* down{nullptr};
    void* front{nullptr};
    void* back{nullptr};

    operator bool() const {
        return right && left && up && down && front && back;
    }
};

enum class ExtentType { Absolute, RelativeToSwapchain };

struct Extent {
    float width{1.0f};
    float height{1.0f};
    ExtentType type{ExtentType::RelativeToSwapchain};

    inline uint32_t rounded_width() {
        return static_cast<uint32_t>(round(width));
    }
    inline uint32_t rounded_height() {
        return static_cast<uint32_t>(round(height));
    }

    bool operator==(const Extent& other) const {
        return (memcmp(this, &other, sizeof(*this)) == 0);
    }

    bool operator!=(const Extent& other) const { return !(*this == other); }
};

struct ColorRenderTargetDesc {
    Extent extent{};
    uint32_t samples{1};
    uint32_t mip_levels{1};
    uint32_t array_layers{1};

    Format format{Format::SwapchainFormat};
    SamplerDesc sampler{};
};

struct DepthRenderTargetDesc {
    Extent extent{};
    uint32_t samples{1};

    Format format{Format::DepthStencil};
    SamplerDesc sampler{};
};

struct ColorAttachmentDesc {
    std::string rt_name;
    bool clear{true};
    bool store{true};
    std::array<float, 4> clear_color{0.0f, 0.0f, 0.0f, 1.0f};
    std::string resolve_to_rt{""};  // resolve to rt at the end of the pass
};

struct DepthAttachmentDesc {
    std::string rt_name;
    bool clear{true};
    bool store{true};
    float clear_depth{1.0f};
    uint32_t clear_stencil{0};
};

struct RenderPassDesc {
    std::vector<ColorAttachmentDesc> color_attachments;
    DepthAttachmentDesc depth_attachment{""};  // empty	rt_name for no depth
};

struct BlitTargetDesc {
    std::string rt;
    Extent extent;
    Extent offset{0.0f, 0.0f, ExtentType::Absolute};

    uint32_t mip_level{0};
    uint32_t base_array_layer{0};
    uint32_t layer_count{1};
};

struct BlitDesc {
    BlitTargetDesc src;
    BlitTargetDesc dst;
};

using RenderTargetName = std::string;
using PassName = std::string;

struct RenderPassEntry {
    PassName name;
    RenderPassDesc desc;
    std::vector<BlitDesc> blits{};
};

struct GeneralPassEntry {
    PassName name;
    std::vector<BlitDesc> blits{};
};

using PassEntry = variant<RenderPassEntry, GeneralPassEntry>;

struct RenderPlan {
    std::map<RenderTargetName, ColorRenderTargetDesc> color_images{};
    std::map<RenderTargetName, DepthRenderTargetDesc> depth_images{};

    std::vector<PassEntry> passes{};
};

struct VertexInputBindingDesc {
    uint32_t binding;
    uint32_t stride;
    bool per_instance{false};
};

struct VertexInputAttributeDesc {
    uint32_t location;
    uint32_t binding;
    Format format;
    uint32_t offset;
};

struct VertexInputFormatDesc {
    std::vector<VertexInputBindingDesc> bindings;
    std::vector<VertexInputAttributeDesc> attributes;
};

enum class StencilFace { Front = 1, Back = 2, FrontAndBack = 3 };

enum class StencilOp {
    Keep = 0,
    Zero = 1,
    Replace = 2,
    IncrementAndClamp = 3,
    DecrementAndClamp = 4,
    Invert = 5,
    IncrementAndWrap = 6,
    DecrementAndWrap = 7
};

struct StencilOpState {
    StencilOp fail_op{StencilOp::Keep};
    StencilOp pass_op{StencilOp::Keep};
    StencilOp depth_fail_op{StencilOp::Keep};
    CompareOp compare_op{CompareOp::Never};
};

struct DepthStencilState {
    bool depth_test{true};
    bool depth_write{true};
    CompareOp depth_compare{CompareOp::Less};
    bool depth_bounds{false};

    bool stencil_test{false};
    StencilOpState front{};
    StencilOpState back{};
};

enum class LogicOp {
    Clear = 0,
    And = 1,
    AndReverse = 2,
    Copy = 3,
    AndInverted = 4,
    NoOp = 5,
    Xor = 6,
    Or = 7,
    Nor = 8,
    Equivalent = 9,
    Invert = 10,
    OrReverse = 11,
    CopyInverted = 12,
    OrInverted = 13,
    Nand = 14,
    Set = 15
};

enum class BlendFactor {
    Zero = 0,
    One = 1,
    SrcColor = 2,
    OneMinusSrcColor = 3,
    DstColor = 4,
    OneMinusDstColor = 5,
    SrcAlpha = 6,
    OneMinusSrcAlpha = 7,
    DstAlpha = 8,
    OneMinusDstAlpha = 9
};

enum class BlendOp {
    Add = 0,
    Subtract = 1,
    ReverseSubtract = 2,
    Min = 3,
    Max = 4
};

enum ColorMaskBits {
    ColorMaskR = 1,
    ColorMaskG = 2,
    ColorMaskB = 4,
    ColorMaskA = 8
};

struct ColorBlendAttachment {
    bool color_blend{true};
    BlendFactor src_color_blend_factor{BlendFactor::OneMinusDstAlpha};
    BlendFactor dst_color_blend_factor{BlendFactor::DstAlpha};
    BlendOp color_blend_op{BlendOp::Add};

    BlendFactor src_alpha_blend_factor{BlendFactor::One};
    BlendFactor dst_alpha_blend_factor{BlendFactor::Zero};
    BlendOp alpha_blend_op{BlendOp::Add};

    uint8_t color_write_mask =
        ColorMaskR | ColorMaskG | ColorMaskB | ColorMaskA;
};

struct ColorBlendState {
    VezColorBlendState state;
    bool color_blend{false};
    LogicOp logic_op{LogicOp::And};
    std::vector<ColorBlendAttachment> attachments{};
};

struct MultisampleState {
    uint32_t samples{1};
    bool sample_shading{false};
    float min_sample_shading{1.0f};
};

enum class PrimitiveTopology {
    PointList = 0,
    LineList = 1,
    LineStrip = 2,
    TriangleList = 3,
    TriangleStrip = 4,
    TriangleFan = 5
};

struct InputAssemblyState {
    PrimitiveTopology topology{PrimitiveTopology::TriangleList};
    bool primitive_restart{true};
};

enum class PolygonMode { Fill = 0, Line = 1, Point = 2 };

enum class CullMode { None = 0, Front = 1, Back = 2, FrontAndBack = 3 };

enum class FrontFace { CounterClockwise = 0, Clockwise = 1 };

struct RasterizationState {
    bool depth_clamp{false};
    bool rasterizer_discard{false};
    PolygonMode polygon_mode{PolygonMode::Fill};
    CullMode cull_mode{CullMode::Back};
    FrontFace front_face{FrontFace::CounterClockwise};
    bool depth_bias{false};
};

enum class ShaderSourceType { Source, Filename };

struct ShaderDesc {
    std::string source;
    ShaderSourceType source_type{ShaderSourceType::Source};

    std::string preamble{""};
    std::string entry_point{"main"};
};

enum class BufferType { General, PerNode, PerMesh, PerMaterial };

struct Box {
    glm::vec3 min{glm::vec3(std::numeric_limits<float>::max())};
    glm::vec3 max{glm::vec3(std::numeric_limits<float>::min())};
};

}  // namespace goma
