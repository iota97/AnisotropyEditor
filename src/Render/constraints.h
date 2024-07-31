#pragma once
#include <QVulkanWindow>
#include <QVulkanFunctions>
#include "src/UI/vulkanwindow.h"
#include "src/Texture/texture.h"
#include "src/Field/optimizer.h"

class PainterTools;
class ConstraintsRenderer {
public:
    ConstraintsRenderer(VulkanWindow *window);
    ~ConstraintsRenderer();

    void draw(VkCommandBuffer cb, Optimizer *opt, const QMatrix4x4& mvp, float scale,
              const QPointF& lastDown, const QPointF& currentDown, const std::vector<std::array<float, 2>> &pointList);

private:
    void createRectData();
    void createHandleData();
    void createPipeline();
    void createHandlePipeline();
    void createPipelineLayout();
    void createUniformBuffer();
    void createDescriptors();
    void drawHandles(VkCommandBuffer cb, uint32_t &drawn, const QMatrix4x4& mvp, float scale, float *p, int selected, const std::vector<Optimizer::Constraint>& constraints);

    VulkanWindow *m_window;
    QVulkanDeviceFunctions *m_devFuncs;

    VkDeviceMemory m_uniformBufMem = VK_NULL_HANDLE;
    VkBuffer m_uniformBuf = VK_NULL_HANDLE;
    VkDescriptorBufferInfo m_uniformBufInfo[QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT];

    VkDescriptorPool m_descPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_descSet[QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT];

    VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipeline m_noStencilPipeline = VK_NULL_HANDLE;
    VkPipeline m_handlePipeline = VK_NULL_HANDLE;

    VkDeviceMemory m_rectBufMem = VK_NULL_HANDLE;
    VkBuffer m_rectBuf = VK_NULL_HANDLE;
    VkDeviceMemory m_handleBufMem = VK_NULL_HANDLE;
    VkBuffer m_handleBuf = VK_NULL_HANDLE;

    uint32_t m_dynamicAlignment;
    static constexpr uint32_t MAX_CONSTRAINTS = 65536;

    Texture *m_atlas;
};
