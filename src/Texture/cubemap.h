#pragma once

#include <QVulkanWindow>
#include <QVulkanFunctions>
#include "src/UI/vulkanwindow.h"

class CubeMap {
public:
    CubeMap(const std::array<QString, 6> &paths, VulkanWindow *w);
    ~CubeMap();

    VkImageView getImageView(uint32_t id = 0);
    VkSampler getTextureSampler();

    uint32_t getWidth() const;

private:
    void createComputeDescriptorSet();
    void createComputePipelineLayout();
    void createComputePipeline();
    void integrate(uint32_t lod);

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void createImage(VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
    void createTextureImage(const std::array<QString, 6> &paths);
    void createImageView();
    void createTextureSampler();
    void generateMipmaps();
    void copyTextureImage();

    void copyBufferToImage(VkBuffer buffer, VkImage image);
    void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t id = 0);

    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    VulkanWindow *m_window;

    VkImage m_textureImage[2];
    VkDeviceMemory m_textureImageMemory[2];
    VkImageView m_textureImageView[2];
    VkSampler m_textureSampler;

    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_mipLevels;

    VkCommandPool m_computeCommandPool = VK_NULL_HANDLE;

    VkPipelineLayout m_computePipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_computePipeline = VK_NULL_HANDLE;

    VkDescriptorSet m_computeDescSet = VK_NULL_HANDLE;
    VkDescriptorPool m_computeDescPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_computeDescSetLayout = VK_NULL_HANDLE;
};
