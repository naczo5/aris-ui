#pragma once
// Vulkan rasterizer that records into a host render pass. It does not own a
// swapchain. Call Prepare() outside the pass (font uploads), then set the
// command buffer and Context::Render() inside the pass.
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
    uint32_t apiVersion = VK_API_VERSION_1_0;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queueFamily = 0;
    VkQueue queue = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
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

    void Draw(const CommandQueue& queue, const FontAtlas* atlas,
              float width, float height) override;

private:
    bool EnsureDevice();
    void DestroyGpu();
    bool UploadImage(const std::uint8_t* pixels, int width, int height, int channels,
                     VkImage& image, VkDeviceMemory& memory, VkImageView& view);

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
    VkDescriptorSet descSet_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;
    VkCommandPool cmdPool_ = VK_NULL_HANDLE;

    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory_ = VK_NULL_HANDLE;
    VkDeviceSize vertexCapacity_ = 0;

    VkImage whiteImage_ = VK_NULL_HANDLE;
    VkDeviceMemory whiteMemory_ = VK_NULL_HANDLE;
    VkImageView whiteView_ = VK_NULL_HANDLE;
    VkImage fontImage_ = VK_NULL_HANDLE;
    VkDeviceMemory fontMemory_ = VK_NULL_HANDLE;
    VkImageView fontView_ = VK_NULL_HANDLE;
    const FontAtlas* fontPtr_ = nullptr;
    std::uint64_t fontRevision_ = 0;
    bool blurWarned_ = false;

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
    PFN_vkQueueWaitIdle queueWait_ = nullptr;
    PFN_vkDeviceWaitIdle deviceWait_ = nullptr;
};

} // namespace ui
