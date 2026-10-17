#include "ui/renderer/vulkan_renderer.hpp"
#include "ui/font/font_atlas.hpp"
#include "emit_draw.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace ui {
namespace {

#include "vk_spv.inc"

uint32_t FindMemory(const VkPhysicalDeviceMemoryProperties& mem, uint32_t bits, VkMemoryPropertyFlags want) {
    for (uint32_t i = 0; i < mem.memoryTypeCount; ++i) {
        if ((bits & (1u << i)) && (mem.memoryTypes[i].propertyFlags & want) == want)
            return i;
    }
    return 0xFFFFFFFFu;
}

} // namespace

VulkanRenderer::VulkanRenderer() = default;

VulkanRenderer::~VulkanRenderer() { Shutdown(); }

bool VulkanRenderer::Init(const VulkanCreateInfo& info) {
    Shutdown();
    info_ = info;
    haveInfo_ = info.device && info.physicalDevice && info.renderPass && info.loader.resolve;
    if (!haveInfo_) {
        std::fprintf(stderr, "[aris-ui] vulkan: VulkanCreateInfo needs device, physicalDevice, "
                             "renderPass and loader.resolve\n");
        return false;
    }
    return EnsureDevice();
}

void VulkanRenderer::Shutdown() {
    DestroyGpu();
    haveInfo_ = false;
    commandBuffer_ = VK_NULL_HANDLE;
    fontPtr_ = nullptr;
    fontRevision_ = 0;
    passWarned_ = false;
}

void VulkanRenderer::ReleaseDevice() {
    DestroyGpu();
}

bool VulkanRenderer::EnsureDevice() {
    if (gpuReady_) return true;
    if (!haveInfo_) return false;

#define LOAD(field, name) \
    field = (decltype(field))info_.loader.resolve(name, info_.loader.user); \
    if (!field) { std::fprintf(stderr, "[aris-ui] vulkan: missing %s\n", name); return false; }
    LOAD(getMemProps_, "vkGetPhysicalDeviceMemoryProperties");
    LOAD(createShader_, "vkCreateShaderModule");
    LOAD(destroyShader_, "vkDestroyShaderModule");
    LOAD(createSetLayout_, "vkCreateDescriptorSetLayout");
    LOAD(destroySetLayout_, "vkDestroyDescriptorSetLayout");
    LOAD(createPipeLayout_, "vkCreatePipelineLayout");
    LOAD(destroyPipeLayout_, "vkDestroyPipelineLayout");
    LOAD(createPipes_, "vkCreateGraphicsPipelines");
    LOAD(destroyPipe_, "vkDestroyPipeline");
    LOAD(createDescPool_, "vkCreateDescriptorPool");
    LOAD(destroyDescPool_, "vkDestroyDescriptorPool");
    LOAD(allocSets_, "vkAllocateDescriptorSets");
    LOAD(updateSets_, "vkUpdateDescriptorSets");
    LOAD(createSampler_, "vkCreateSampler");
    LOAD(destroySampler_, "vkDestroySampler");
    LOAD(createBuffer_, "vkCreateBuffer");
    LOAD(destroyBuffer_, "vkDestroyBuffer");
    LOAD(getBufReqs_, "vkGetBufferMemoryRequirements");
    LOAD(allocMem_, "vkAllocateMemory");
    LOAD(freeMem_, "vkFreeMemory");
    LOAD(bindBuf_, "vkBindBufferMemory");
    LOAD(mapMem_, "vkMapMemory");
    LOAD(unmapMem_, "vkUnmapMemory");
    LOAD(flushMem_, "vkFlushMappedMemoryRanges");
    LOAD(createImage_, "vkCreateImage");
    LOAD(destroyImage_, "vkDestroyImage");
    LOAD(getImgReqs_, "vkGetImageMemoryRequirements");
    LOAD(bindImg_, "vkBindImageMemory");
    LOAD(createView_, "vkCreateImageView");
    LOAD(destroyView_, "vkDestroyImageView");
    LOAD(createPool_, "vkCreateCommandPool");
    LOAD(destroyPool_, "vkDestroyCommandPool");
    LOAD(allocCmds_, "vkAllocateCommandBuffers");
    LOAD(beginCmd_, "vkBeginCommandBuffer");
    LOAD(endCmd_, "vkEndCommandBuffer");
    LOAD(cmdBarrier_, "vkCmdPipelineBarrier");
    LOAD(cmdCopy_, "vkCmdCopyBufferToImage");
    LOAD(cmdBindPipe_, "vkCmdBindPipeline");
    LOAD(cmdBindVerts_, "vkCmdBindVertexBuffers");
    LOAD(cmdBindSets_, "vkCmdBindDescriptorSets");
    LOAD(cmdPush_, "vkCmdPushConstants");
    LOAD(cmdViewport_, "vkCmdSetViewport");
    LOAD(cmdScissor_, "vkCmdSetScissor");
    LOAD(cmdDraw_, "vkCmdDraw");
    LOAD(createFence_, "vkCreateFence");
    LOAD(destroyFence_, "vkDestroyFence");
    LOAD(waitFences_, "vkWaitForFences");
    LOAD(queueSubmit_, "vkQueueSubmit");
    LOAD(deviceWait_, "vkDeviceWaitIdle");
#undef LOAD

    getMemProps_(info_.physicalDevice, &mem_);

    // From here on every failure must drop what was created; retrying later
    // (or Shutdown) picks up the pieces.
    slots_.clear();
    slots_.assign(std::min(std::max(info_.framesInFlight, 1u), 8u), FrameSlot{});
    frameCounter_ = 0;

    auto module = [&](const uint32_t* code, uint32_t count) {
        VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        ci.codeSize = count * sizeof(uint32_t);
        ci.pCode = code;
        VkShaderModule m = VK_NULL_HANDLE;
        createShader_(info_.device, &ci, nullptr, &m);
        return m;
    };
    vert_ = module(kUiVertSpv, kUiVertSpv_count);
    frag_ = module(kUiFragSpv, kUiFragSpv_count);
    if (!vert_ || !frag_) { DestroyGpu(); return false; }

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    sl.bindingCount = 1;
    sl.pBindings = &binding;
    if (createSetLayout_(info_.device, &sl, nullptr, &setLayout_) != VK_SUCCESS) {
        DestroyGpu();
        return false;
    }

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push.offset = 0;
    push.size = 16;
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &setLayout_;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &push;
    if (createPipeLayout_(info_.device, &pl, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        DestroyGpu();
        return false;
    }

    VkVertexInputBindingDescription bind{0, sizeof(UiVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[11] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(UiVertex, pos)},
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(UiVertex, rect)},
        {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(UiVertex, radii)},
        {3, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(UiVertex, color)},
        {4, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(UiVertex, borderColor)},
        {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(UiVertex, params)},
        {6, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(UiVertex, shadow)},
        {7, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(UiVertex, uv)},
        {8, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(UiVertex, color2)},
        {9, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(UiVertex, shadowColor)},
        {10, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(UiVertex, extra)},
    };
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = 11;
    vi.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.f;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = info_.sampleCount;
    VkPipelineColorBlendAttachmentState att{};
    att.blendEnable = VK_TRUE;
    att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att.colorBlendOp = VK_BLEND_OP_ADD;
    att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att.alphaBlendOp = VK_BLEND_OP_ADD;
    att.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &att;
    VkDynamicState dynStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert_;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag_;
    stages[1].pName = "main";

    VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gp.stageCount = 2;
    gp.pStages = stages;
    gp.pVertexInputState = &vi;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vp;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms;
    gp.pColorBlendState = &cb;
    gp.pDynamicState = &dyn;
    gp.layout = pipelineLayout_;
    gp.renderPass = info_.renderPass;
    gp.subpass = info_.subpass;
    if (createPipes_(info_.device, VK_NULL_HANDLE, 1, &gp, nullptr, &pipeline_) != VK_SUCCESS) {
        std::fprintf(stderr, "[aris-ui] vulkan: pipeline creation failed (subpass %u, %u samples?)\n",
                     info_.subpass, (unsigned)info_.sampleCount);
        DestroyGpu();
        return false;
    }
    pipelinePass_ = info_.renderPass;

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                  (uint32_t)slots_.size()};
    VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dp.maxSets = (uint32_t)slots_.size();
    dp.poolSizeCount = 1;
    dp.pPoolSizes = &poolSize;
    if (createDescPool_(info_.device, &dp, nullptr, &descPool_) != VK_SUCCESS) {
        DestroyGpu();
        return false;
    }
    for (FrameSlot& slot : slots_) {
        VkDescriptorSetAllocateInfo ds{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ds.descriptorPool = descPool_;
        ds.descriptorSetCount = 1;
        ds.pSetLayouts = &setLayout_;
        if (allocSets_(info_.device, &ds, &slot.set) != VK_SUCCESS) {
            DestroyGpu();
            return false;
        }
    }

    VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = sci.minFilter = VK_FILTER_LINEAR;
    sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (createSampler_(info_.device, &sci, nullptr, &sampler_) != VK_SUCCESS) {
        DestroyGpu();
        return false;
    }

    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = info_.queueFamily;
    if (createPool_(info_.device, &pool, nullptr, &cmdPool_) != VK_SUCCESS) {
        DestroyGpu();
        return false;
    }

    const uint8_t white[4] = {255, 255, 255, 255};
    if (!UploadImage(white, 1, 1, 4, whiteImage_, whiteMemory_, whiteView_)) {
        DestroyGpu();
        return false;
    }
    gpuReady_ = true;
    return true;
}

void VulkanRenderer::DestroyGpu() {
    const VkDevice dev = info_.device;
    // Safe after a partial EnsureDevice: every call is guarded, so a failed
    // step never leaks the ones that succeeded.
    if (dev && deviceWait_) deviceWait_(dev);

    auto killView = [&](VkImageView& view) {
        if (dev && destroyView_ && view) destroyView_(dev, view, nullptr);
        view = VK_NULL_HANDLE;
    };
    auto killImage = [&](VkImage& image, VkDeviceMemory& mem, VkImageView& view) {
        killView(view);
        if (dev && destroyImage_ && image) destroyImage_(dev, image, nullptr);
        if (dev && freeMem_ && mem) freeMem_(dev, mem, nullptr);
        image = VK_NULL_HANDLE;
        mem = VK_NULL_HANDLE;
    };
    killImage(fontImage_, fontMemory_, fontView_);
    killImage(whiteImage_, whiteMemory_, whiteView_);
    for (FrameSlot& slot : slots_) {
        if (dev && destroyBuffer_ && slot.buffer) destroyBuffer_(dev, slot.buffer, nullptr);
        if (dev && freeMem_ && slot.memory) freeMem_(dev, slot.memory, nullptr);
        slot = FrameSlot{};
    }
    slots_.clear();
    if (dev && destroyPool_ && cmdPool_) destroyPool_(dev, cmdPool_, nullptr);
    if (dev && destroySampler_ && sampler_) destroySampler_(dev, sampler_, nullptr);
    if (dev && destroyDescPool_ && descPool_) destroyDescPool_(dev, descPool_, nullptr);
    if (dev && destroyPipe_ && pipeline_) destroyPipe_(dev, pipeline_, nullptr);
    if (dev && destroyPipeLayout_ && pipelineLayout_) destroyPipeLayout_(dev, pipelineLayout_, nullptr);
    if (dev && destroySetLayout_ && setLayout_) destroySetLayout_(dev, setLayout_, nullptr);
    if (dev && destroyShader_ && vert_) destroyShader_(dev, vert_, nullptr);
    if (dev && destroyShader_ && frag_) destroyShader_(dev, frag_, nullptr);
    cmdPool_ = VK_NULL_HANDLE;
    sampler_ = VK_NULL_HANDLE;
    descPool_ = VK_NULL_HANDLE;
    pipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
    setLayout_ = VK_NULL_HANDLE;
    vert_ = VK_NULL_HANDLE;
    frag_ = VK_NULL_HANDLE;
    pipelinePass_ = VK_NULL_HANDLE;
    frameCounter_ = 0;
    gpuReady_ = false;
    fontPtr_ = nullptr;
    fontRevision_ = 0;
}

void VulkanRenderer::WaitIdle() {
    if (info_.device && deviceWait_) deviceWait_(info_.device);
}

bool VulkanRenderer::UploadImage(const uint8_t* pixels, int w, int h, int channels,
                                 VkImage& image, VkDeviceMemory& memory, VkImageView& view) {
    std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
    for (int i = 0; i < w * h; ++i) {
        rgba[i * 4 + 0] = pixels[i * channels + 0];
        rgba[i * 4 + 1] = channels > 1 ? pixels[i * channels + 1] : pixels[i * channels];
        rgba[i * 4 + 2] = channels > 2 ? pixels[i * channels + 2] : pixels[i * channels];
        rgba[i * 4 + 3] = channels > 3 ? pixels[i * channels + 3] : 255;
    }
    const VkDeviceSize bytes = rgba.size();

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    // Free whatever the caller already owns: a failed upload must not leak.
    auto freeStaging = [&]() {
        if (staging) destroyBuffer_(info_.device, staging, nullptr);
        if (stagingMem) freeMem_(info_.device, stagingMem, nullptr);
        staging = VK_NULL_HANDLE;
        stagingMem = VK_NULL_HANDLE;
    };
    auto freeTarget = [&]() {
        if (view) destroyView_(info_.device, view, nullptr);
        if (image) destroyImage_(info_.device, image, nullptr);
        if (memory) freeMem_(info_.device, memory, nullptr);
        view = VK_NULL_HANDLE;
        image = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
    };

    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = bytes;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (createBuffer_(info_.device, &bci, nullptr, &staging) != VK_SUCCESS) return false;
    VkMemoryRequirements req{};
    getBufReqs_(info_.device, staging, &req);
    uint32_t type = FindMemory(mem_, req.memoryTypeBits,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (type == 0xFFFFFFFFu)
        type = FindMemory(mem_, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    if (type == 0xFFFFFFFFu) { freeStaging(); return false; }
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = type;
    if (allocMem_(info_.device, &mai, nullptr, &stagingMem) != VK_SUCCESS) {
        freeStaging();
        return false;
    }
    bindBuf_(info_.device, staging, stagingMem, 0);
    void* mapped = nullptr;
    mapMem_(info_.device, stagingMem, 0, req.size, 0, &mapped);
    std::memcpy(mapped, rgba.data(), bytes);
    if (!(mem_.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        range.memory = stagingMem;
        range.size = VK_WHOLE_SIZE;
        flushMem_(info_.device, 1, &range);
    }
    unmapMem_(info_.device, stagingMem);

    VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (createImage_(info_.device, &ici, nullptr, &image) != VK_SUCCESS) {
        freeStaging();
        return false;
    }
    getImgReqs_(info_.device, image, &req);
    type = FindMemory(mem_, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (type == 0xFFFFFFFFu) type = FindMemory(mem_, req.memoryTypeBits, 0);
    if (type == 0xFFFFFFFFu) {
        freeTarget();
        freeStaging();
        return false;
    }
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = type;
    if (allocMem_(info_.device, &mai, nullptr, &memory) != VK_SUCCESS) {
        freeTarget();
        freeStaging();
        return false;
    }
    bindImg_(info_.device, image, memory, 0);

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = cmdPool_;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    if (allocCmds_(info_.device, &cai, &cmd) != VK_SUCCESS) {
        freeTarget();
        freeStaging();
        return false;
    }
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginCmd_(cmd, &bi);
    VkImageMemoryBarrier toDst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toDst.srcAccessMask = 0;
    toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toDst.srcQueueFamilyIndex = toDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toDst.image = image;
    toDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    cmdBarrier_(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &toDst);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
    cmdCopy_(cmd, staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    VkImageMemoryBarrier toSample = toDst;
    toSample.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toSample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toSample.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toSample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    cmdBarrier_(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &toSample);
    endCmd_(cmd);
    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (createFence_(info_.device, &fi, nullptr, &fence) != VK_SUCCESS) {
        freeTarget();
        freeStaging();
        return false;
    }
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    const bool submitted = queueSubmit_(info_.queue, 1, &si, fence) == VK_SUCCESS;
    if (submitted) waitFences_(info_.device, 1, &fence, VK_TRUE, UINT64_MAX);
    destroyFence_(info_.device, fence, nullptr);
    freeStaging();
    if (!submitted) {
        freeTarget();
        return false;
    }

    VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.image = image;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = VK_FORMAT_R8G8B8A8_UNORM;
    vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (createView_(info_.device, &vci, nullptr, &view) != VK_SUCCESS) {
        freeTarget();
        return false;
    }
    return true;
}

void VulkanRenderer::Prepare(const FontAtlas* atlas) {
    if (!EnsureDevice()) return;
    if (!atlas || !atlas->Loaded() || !atlas->Pixels()) return;
    if (fontView_ && fontPtr_ == atlas && fontRevision_ == atlas->Revision()) return;
    if (fontView_ || fontImage_ || fontMemory_) {
        // The old image may still be sampled by an in-flight frame; the host
        // calls Prepare() outside the pass, so a device wait is affordable here.
        WaitIdle();
        if (fontView_) destroyView_(info_.device, fontView_, nullptr);
        if (fontImage_) destroyImage_(info_.device, fontImage_, nullptr);
        if (fontMemory_) freeMem_(info_.device, fontMemory_, nullptr);
        fontView_ = VK_NULL_HANDLE; fontImage_ = VK_NULL_HANDLE; fontMemory_ = VK_NULL_HANDLE;
        fontPtr_ = nullptr;
        fontRevision_ = 0;
    }
    if (UploadImage(atlas->Pixels(), atlas->Width(), atlas->Height(), atlas->Channels(),
                    fontImage_, fontMemory_, fontView_)) {
        fontPtr_ = atlas;
        fontRevision_ = atlas->Revision();
    } else {
        std::fprintf(stderr, "[aris-ui] vulkan: font atlas upload failed\n");
    }
}

void VulkanRenderer::Draw(const CommandQueue& queue, const FontAtlas* atlas,
                          float width, float height) {
    if (!commandBuffer_ || width <= 0.f || height <= 0.f) return;
    if (!EnsureDevice()) return;
    if (pipelinePass_ != info_.renderPass) {
        // Swapchain recreation invalidated the pass the pipeline was built for.
        if (!passWarned_) {
            passWarned_ = true;
            std::fprintf(stderr, "[aris-ui] vulkan: render pass changed, call Init() again to rebuild the UI pipeline\n");
        }
        return;
    }
    if (slots_.empty()) return;
    FrameSlot& slot = slots_[frameCounter_++ % slots_.size()];

    if (!scratch_) scratch_ = std::make_unique<SpanScratch>();
    BuildUiSpans(queue, *scratch_);
    const std::vector<UiSpan>& spans = scratch_->spans;
    if (spans.empty()) return;

    all_.clear();
    for (const UiSpan& span : spans) {
        if (span.backdrop && !blurWarned_) {
            blurWarned_ = true;
            std::fprintf(stderr, "[aris-ui] vulkan: backdrop blur is OpenGL-only and was skipped\n");
        }
        all_.insert(all_.end(), span.verts.begin(), span.verts.end());
    }
    if (all_.empty()) return;

    const VkDeviceSize bytes = all_.size() * sizeof(UiVertex);
    if (slot.capacity < bytes) {
        // Growing drops the old buffer, which an in-flight frame may still read.
        WaitIdle();
        if (slot.buffer) destroyBuffer_(info_.device, slot.buffer, nullptr);
        if (slot.memory) freeMem_(info_.device, slot.memory, nullptr);
        slot.buffer = VK_NULL_HANDLE;
        slot.memory = VK_NULL_HANDLE;
        slot.capacity = 0;
        VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bci.size = bytes;
        bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (createBuffer_(info_.device, &bci, nullptr, &slot.buffer) != VK_SUCCESS) {
            std::fprintf(stderr, "[aris-ui] vulkan: vertex buffer creation failed\n");
            return;
        }
        VkMemoryRequirements req{};
        getBufReqs_(info_.device, slot.buffer, &req);
        uint32_t type = FindMemory(mem_, req.memoryTypeBits,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (type == 0xFFFFFFFFu)
            type = FindMemory(mem_, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        if (type == 0xFFFFFFFFu) {
            std::fprintf(stderr, "[aris-ui] vulkan: no host-visible memory for the vertex buffer\n");
            destroyBuffer_(info_.device, slot.buffer, nullptr);
            slot.buffer = VK_NULL_HANDLE;
            return;
        }
        VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize = req.size;
        mai.memoryTypeIndex = type;
        if (allocMem_(info_.device, &mai, nullptr, &slot.memory) != VK_SUCCESS) {
            std::fprintf(stderr, "[aris-ui] vulkan: vertex buffer allocation failed\n");
            destroyBuffer_(info_.device, slot.buffer, nullptr);
            slot.buffer = VK_NULL_HANDLE;
            return;
        }
        bindBuf_(info_.device, slot.buffer, slot.memory, 0);
        slot.capacity = req.size;
    }
    void* mapped = nullptr;
    mapMem_(info_.device, slot.memory, 0, bytes, 0, &mapped);
    if (!mapped) return;
    std::memcpy(mapped, all_.data(), static_cast<size_t>(bytes));
    unmapMem_(info_.device, slot.memory);

    const float pxRange = (atlas && atlas->Loaded() && atlas->GenerationFontSize() > 0.f)
        ? atlas->PxRange() : 0.f;
    const float gen = (atlas && atlas->Loaded()) ? atlas->GenerationFontSize() : 1.f;

    cmdBindPipe_(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    VkViewport viewport{0.f, 0.f, width, height, 0.f, 1.f};
    cmdViewport_(commandBuffer_, 0, 1, &viewport);
    VkDeviceSize offset = 0;
    cmdBindVerts_(commandBuffer_, 0, 1, &slot.buffer, &offset);

    VkImageView bound = VK_NULL_HANDLE;
    uint32_t first = 0;
    auto bindView = [&](VkImageView view) {
        if (view == bound) return;
        bound = view;
        VkDescriptorImageInfo ii{sampler_, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = slot.set;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &ii;
        updateSets_(info_.device, 1, &write, 0, nullptr);
        cmdBindSets_(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_,
                     0, 1, &slot.set, 0, nullptr);
    };

    for (const UiSpan& span : spans) {
        if (span.backdrop || span.verts.empty()) continue;
        const uint32_t count = static_cast<uint32_t>(span.verts.size());
        // Vulkan scissor rectangles must stay inside the render area; a panel
        // dragged off-screen produces clips that GL would silently trim.
        VkRect2D scissor{{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}};
        if (span.scissor) {
            const int32_t x0 = (int32_t)std::floor(std::max(span.scissorX, 0.f));
            const int32_t y0 = (int32_t)std::floor(std::max(span.scissorY, 0.f));
            const int32_t x1 = (int32_t)std::ceil(std::min(span.scissorX + span.scissorW, width));
            const int32_t y1 = (int32_t)std::ceil(std::min(span.scissorY + span.scissorH, height));
            if (x1 <= x0 || y1 <= y0) {
                first += count;      // fully clipped: skip the draw, keep the offsets
                continue;
            }
            scissor.offset = {x0, y0};
            scissor.extent = {static_cast<uint32_t>(x1 - x0), static_cast<uint32_t>(y1 - y0)};
        }
        const bool glyph = span.atlasId != 0 && fontView_;
        bindView(glyph ? fontView_ : whiteView_);
        float range = 0.f;
        if (glyph && gen > 0.f) range = pxRange * (span.fontSize / gen);
        float push[4] = {width, height, range, 0.f};
        cmdPush_(commandBuffer_, pipelineLayout_,
                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16, push);
        cmdScissor_(commandBuffer_, 0, 1, &scissor);
        cmdDraw_(commandBuffer_, count, 1, first, 0);
        first += count;
    }
}

} // namespace ui
