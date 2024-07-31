#pragma once
#include "src/UI/vulkanwindow.h"

class Mesh;
class MonteCarlo {
public:
    MonteCarlo(VulkanWindow *window, VkPipelineCache &pipelineCache, VkPipelineLayout &pipelineLayout);
    ~MonteCarlo();

    void resizeFrameBuffer();
    void setPipeline(const std::string &name);

    void render(Mesh *mesh, VkBuffer *quadBuf, VkDescriptorSet descSet);
    void resolve(VkCommandBuffer cb);

    void clear();
    uint32_t getSampleCount();

private:
    void copy();
    void createMeshData();
    void createResolvePipelineLayout();
    void createRenderPass();
    void createCopyRenderPass();
    void createDescriptors();
    void createResolvePipeline();
    void createCopyPipeline();

    VulkanWindow *m_window;
    QVulkanDeviceFunctions *m_devFuncs;

    VkPipelineCache &m_pipelineCache;
    VkPipelineLayout &m_pipelineLayout;

    VkDescriptorPool m_resolveDescPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_resolveDescSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_resolvePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_resolveDescSet[QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT];
    VkPipeline m_resolvePipeline = VK_NULL_HANDLE;
    VkPipeline m_copyPipeline = VK_NULL_HANDLE;

    VkDeviceMemory m_meshBufMem = VK_NULL_HANDLE;
    VkBuffer m_meshBuf = VK_NULL_HANDLE;

    VkPipeline m_pipeline = VK_NULL_HANDLE;

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkFramebuffer m_frameBuffer = VK_NULL_HANDLE;
    VkRenderPass m_copyRenderPass = VK_NULL_HANDLE;
    VkFramebuffer m_copyFrameBuffer = VK_NULL_HANDLE;
    Texture* m_result = nullptr;
    Texture* m_temp = nullptr;
    Texture* m_depth  = nullptr;

    uint32_t m_sampleCount = 0;
};
