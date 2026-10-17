#pragma once
// Vulkan rasterizer that records into a host render pass. It does not own a
// swapchain. Call Prepare() outside the pass (font uploads), then set the
// command buffer and Context::Render() inside the pass.
//
// Note: this header defines VK_NO_PROTOTYPES for the vulkan headers it
// includes, and aris-ui ships its own Vulkan-Headers on the public include
// path (see third_party/VERSIONS.md).
//
// Contract with the host:
//   - renderPass/subpass/sampleCount describe the pass the UI records into.
//     The pipeline is built for them once: call Init() again after recreating
//     the render pass (swapchain resize) or the UI will stop drawing.
//   - Draw() rotates `framesInFlight` vertex/descriptor slots, so keep at most
//     that many frames in flight; match it to the host's own constant.
//   - Prepare() and ReleaseDevice() wait for the device (they replace images).
//     Do not call them with frames in flight if you can avoid the stall.
//   - Not thread safe; record from one thread.
#include <cstdint>
#include <memory>
#include <vector>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "ui/renderer/draw_backend.hpp"
#include "ui/renderer/ui_vertex.hpp"

namespace ui {

class FontAtlas;
struct SpanScratch;

struct VulkanLoader {
    void* user = nullptr;
    PFN_vkVoidFunction (*resolve)(const char* name, void* user) = nullptr;
};

struct VulkanCreateInfo {
    uint32_t apiVersion = VK_API_VERSION_1_0;   // informational; the renderer targets Vulkan 1.0
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queueFamily = 0;
    VkQueue queue = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpass = 0;                       // subpass the UI draws in
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;   // must match the pass attachments
    uint32_t framesInFlight = 2;                // vertex/descriptor slots rotated by Draw()
    VulkanLoader loader;
};

class VulkanRenderer : public DrawBackend {
public:
    VulkanRenderer();
    ~VulkanRenderer() override;

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    bool Init(const VulkanCreateInfo& info);
    void Shutdown();
    void ReleaseDevice() override;

    // Uploads a dirty atlas. Must run outside a render pass.
    void Prepare(const FontAtlas* atlas);
    void SetCommandBuffer(VkCommandBuffer cmd) { commandBuffer_ = cmd; }

    // Blocks until the device is idle. Call before the host recreates or
    // destroys anything this renderer's recorded frames reference.
    void WaitIdle();

    void Draw(const CommandQueue& queue, const FontAtlas* atlas,
              float width, float height) override;

private:
    bool EnsureDevice();
    void DestroyGpu();
    bool UploadImage(const std::uint8_t* pixels, int width, int height, int channels,
                     VkImage& image, VkDeviceMemory& memory, VkImageView& view);

    // Per-frame resources. Draw() rotates through these so the frame recorded on
    // one slot can still be in flight while the next one is written.
    struct FrameSlot {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize capacity = 0;
        VkDescriptorSet set = VK_NULL_HANDLE;
    };

    VulkanCreateInfo info_{};
    bool haveInfo_ = false;
    bool gpuReady_ = false;
    VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;

    VkPhysicalDeviceMemoryProperties mem_{};
    VkShaderModule vert_ = VK_NULL_HANDLE;
    VkShaderModule frag_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkRenderPass pipelinePass_ = VK_NULL_HANDLE;
    VkDescriptorPool descPool_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;
    VkCommandPool cmdPool_ = VK_NULL_HANDLE;
    std::vector<FrameSlot> slots_;
    uint32_t frameCounter_ = 0;

    VkImage whiteImage_ = VK_NULL_HANDLE;
    VkDeviceMemory whiteMemory_ = VK_NULL_HANDLE;
    VkImageView whiteView_ = VK_NULL_HANDLE;
    VkImage fontImage_ = VK_NULL_HANDLE;
    VkDeviceMemory fontMemory_ = VK_NULL_HANDLE;
    VkImageView fontView_ = VK_NULL_HANDLE;
    const FontAtlas* fontPtr_ = nullptr;
    std::uint64_t fontRevision_ = 0;
    bool blurWarned_ = false;
    bool passWarned_ = false;

    // Persistent per-frame scratch (spans + staging vertices).
    std::unique_ptr<SpanScratch> scratch_;
    std::vector<UiVertex> all_;

    PFN_vkGetPhysicalDeviceMemoryProperties getMemProps_ = nullptr;
    PFN_vkCreateShaderModule createShader_ = nullptr;
    PFN_vkDestroyShaderModule destroyShader_ = nullptr;
    PFN_vkCreateDescriptorSetLayout createSetLayout_ = nullptr;
    PFN_vkDestroyDescriptorSetLayout destroySetLayout_ = nullptr;
    PFN_vkCreatePipelineLayout createPipeLayout_ = nullptr;
    PFN_vkDestroyPipelineLayout destroyPipeLayout_ = nullptr;
    PFN_vkCreateGraphicsPipelines createPipes_ = nullptr;
    PFN_vkDestroyPipeline destroyPipe_ = nullptr;
    PFN_vkCreateDescriptorPool createDescPool_ = nullptr;
    PFN_vkDestroyDescriptorPool destroyDescPool_ = nullptr;
    PFN_vkAllocateDescriptorSets allocSets_ = nullptr;
    PFN_vkUpdateDescriptorSets updateSets_ = nullptr;
    PFN_vkCreateSampler createSampler_ = nullptr;
    PFN_vkDestroySampler destroySampler_ = nullptr;
    PFN_vkCreateBuffer createBuffer_ = nullptr;
    PFN_vkDestroyBuffer destroyBuffer_ = nullptr;
    PFN_vkGetBufferMemoryRequirements getBufReqs_ = nullptr;
    PFN_vkAllocateMemory allocMem_ = nullptr;
    PFN_vkFreeMemory freeMem_ = nullptr;
    PFN_vkBindBufferMemory bindBuf_ = nullptr;
    PFN_vkMapMemory mapMem_ = nullptr;
    PFN_vkUnmapMemory unmapMem_ = nullptr;
    PFN_vkFlushMappedMemoryRanges flushMem_ = nullptr;
    PFN_vkCreateImage createImage_ = nullptr;
    PFN_vkDestroyImage destroyImage_ = nullptr;
    PFN_vkGetImageMemoryRequirements getImgReqs_ = nullptr;
    PFN_vkBindImageMemory bindImg_ = nullptr;
    PFN_vkCreateImageView createView_ = nullptr;
    PFN_vkDestroyImageView destroyView_ = nullptr;
    PFN_vkCreateCommandPool createPool_ = nullptr;
    PFN_vkDestroyCommandPool destroyPool_ = nullptr;
    PFN_vkAllocateCommandBuffers allocCmds_ = nullptr;
    PFN_vkBeginCommandBuffer beginCmd_ = nullptr;
    PFN_vkEndCommandBuffer endCmd_ = nullptr;
    PFN_vkCmdPipelineBarrier cmdBarrier_ = nullptr;
    PFN_vkCmdCopyBufferToImage cmdCopy_ = nullptr;
    PFN_vkCmdBindPipeline cmdBindPipe_ = nullptr;
    PFN_vkCmdBindVertexBuffers cmdBindVerts_ = nullptr;
    PFN_vkCmdBindDescriptorSets cmdBindSets_ = nullptr;
    PFN_vkCmdPushConstants cmdPush_ = nullptr;
    PFN_vkCmdSetViewport cmdViewport_ = nullptr;
    PFN_vkCmdSetScissor cmdScissor_ = nullptr;
    PFN_vkCmdDraw cmdDraw_ = nullptr;
    PFN_vkCreateFence createFence_ = nullptr;
    PFN_vkDestroyFence destroyFence_ = nullptr;
    PFN_vkWaitForFences waitFences_ = nullptr;
    PFN_vkQueueSubmit queueSubmit_ = nullptr;
    PFN_vkDeviceWaitIdle deviceWait_ = nullptr;
};

} // namespace ui
