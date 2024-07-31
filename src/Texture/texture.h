#pragma once

#include <QVulkanWindow>
#include <QVulkanFunctions>

class VulkanWindow;
class Texture {
public:
    Texture(const QString &path, VulkanWindow *w, uint32_t depth = 1,
            bool compute = false, bool disableMipmap = false, bool colorAttachment = false, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT, bool *result = nullptr);

    Texture(uint32_t width, uint32_t height, VulkanWindow *w, uint32_t depth = 1, VkFormat fmt = VK_FORMAT_R8G8B8A8_SRGB,
            bool compute = false, bool disableMipmap = false, bool colorAttachment = false, bool depthAttachment = false,
            VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

    virtual ~Texture();
    Texture(Texture&& other);
    Texture& operator=(Texture&& other);
    Texture& operator=(const Texture&) = delete;

    static Texture* createFromTexture(const Texture&);

    VkImageView getImageView();
    VkImageView* getImageViewPtr();
    VkSampler getTextureSampler();
    VkImage getImage();

    void saveToPng(const QString &path);
    void clear(float r, float g, float b, float a);
    void blitTextureImage(const Texture& other);
    void generateMipmaps(VkSemaphore *waitSemaphorePtr = nullptr);
    uint32_t getWidth();
    uint32_t getHeight();
    uint32_t getMipLevels();

protected:
    Texture(const Texture&);

    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkSemaphore *waitSemaphorePtr = nullptr);
    void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);

    void createTextureImage(const QString& path, bool *result);
    void createTextureImage();
    void createImageView();
    void createTextureSampler();

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void createImage(VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t depth);
    void copyTextureImage(const Texture& other);

    VulkanWindow *m_window;

    VkImage m_textureImage = VK_NULL_HANDLE;
    VkDeviceMemory m_textureImageMemory = VK_NULL_HANDLE;
    VkImageView m_textureImageView = VK_NULL_HANDLE;
    VkSampler m_textureSampler = VK_NULL_HANDLE;

    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_depth;
    uint32_t m_mipLevels;

    VkFormat m_format;
    VkSampleCountFlagBits m_sampleCount;

    bool m_compute;
    bool m_colorAttachment;
    bool m_depthAttachment = false;
    bool m_disableMipmap;
};

