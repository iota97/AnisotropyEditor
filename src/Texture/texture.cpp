#include "texture.h"
#include "src/UI/vulkanwindow.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include "src/Texture/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8
#include "src/Texture/stb_image_write.h"

Texture::Texture(const QString &path, VulkanWindow *w, uint32_t depth,
                 bool compute, bool disableMipmap, bool colorAttachment, VkSampleCountFlagBits sampleCount, bool *result)
    : m_window(w), m_depth(depth), m_format(VK_FORMAT_R8G8B8A8_UNORM), m_sampleCount(sampleCount), m_compute(compute), m_colorAttachment(colorAttachment), m_disableMipmap(disableMipmap) {
    createTextureImage(path, result);
    if (!result || *result) {
        createImageView();
        createTextureSampler();
        generateMipmaps();
    }
}

Texture::Texture(uint32_t width, uint32_t height, VulkanWindow *w, uint32_t depth, VkFormat fmt,
                 bool compute, bool disableMipmap, bool colorAttachment, bool depthAttachment, VkSampleCountFlagBits sampleCount)
    : m_window(w), m_width(width), m_height(height), m_depth(depth), m_format(fmt), m_sampleCount(sampleCount),
    m_compute(compute), m_colorAttachment(colorAttachment), m_depthAttachment(depthAttachment), m_disableMipmap(disableMipmap) {
    createTextureImage();
    createImageView();
    createTextureSampler();
    generateMipmaps();
}

Texture::Texture(const Texture& other) {
    m_window = other.m_window;
    m_format = other.m_format;
    m_compute = other.m_compute;
    m_width = other.m_width;
    m_height = other.m_height;
    m_mipLevels = other.m_mipLevels;
    m_depth = other.m_depth;
    m_colorAttachment = other.m_colorAttachment;
    m_disableMipmap = other.m_disableMipmap;
    m_sampleCount = other.m_sampleCount;

    copyTextureImage(other);
    createImageView();
    createTextureSampler();
    generateMipmaps();
}

Texture* Texture::createFromTexture(const Texture& from) {
    return new Texture(from);
}

uint32_t Texture::getWidth() {
    return m_width;
}

uint32_t Texture::getHeight() {
    return m_height;
}

Texture::~Texture() {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);
    devFuncs->vkDeviceWaitIdle(dev);

    if (m_textureSampler)
        devFuncs->vkDestroySampler(dev, m_textureSampler, nullptr);
    if (m_textureImageView)
        devFuncs->vkDestroyImageView(dev, m_textureImageView, nullptr);
    if (m_textureImage)
        devFuncs->vkDestroyImage(dev, m_textureImage, nullptr);
    if (m_textureImageMemory)
        devFuncs->vkFreeMemory(dev, m_textureImageMemory, nullptr);
}

Texture::Texture(Texture&& other) {
    m_window = other.m_window;
    m_textureImage = other.m_textureImage;
    m_textureImageMemory = other.m_textureImageMemory;
    m_textureImageView = other.m_textureImageView;
    m_textureSampler = other.m_textureSampler;
    m_format = other.m_format;
    m_compute = other.m_compute;
    m_width = other.m_width;
    m_height = other.m_height;
    m_mipLevels = other.m_mipLevels;
    m_depth = other.m_depth;
    m_colorAttachment = other.m_colorAttachment;
    m_disableMipmap = other.m_disableMipmap;
    m_sampleCount = other.m_sampleCount;
    other.m_textureImage = VK_NULL_HANDLE;
    other.m_textureImageMemory = VK_NULL_HANDLE;
    other.m_textureImageView = VK_NULL_HANDLE;
    other.m_textureSampler = VK_NULL_HANDLE;
}

Texture& Texture::operator=(Texture&& other) {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);
    devFuncs->vkDeviceWaitIdle(dev);

    if (m_textureSampler)
        devFuncs->vkDestroySampler(dev, m_textureSampler, nullptr);
    if (m_textureImageView)
        devFuncs->vkDestroyImageView(dev, m_textureImageView, nullptr);
    if (m_textureImage)
        devFuncs->vkDestroyImage(dev, m_textureImage, nullptr);
    if (m_textureImageMemory)
        devFuncs->vkFreeMemory(dev, m_textureImageMemory, nullptr);

    m_window = other.m_window;
    m_textureImage = other.m_textureImage;
    m_textureImageMemory = other.m_textureImageMemory;
    m_textureImageView = other.m_textureImageView;
    m_textureSampler = other.m_textureSampler;
    m_format = other.m_format;
    m_compute = other.m_compute;
    m_width = other.m_width;
    m_height = other.m_height;
    m_mipLevels = other.m_mipLevels;
    m_depth = other.m_depth;
    m_colorAttachment = other.m_colorAttachment;
    m_disableMipmap = other.m_disableMipmap;
    m_sampleCount = other.m_sampleCount;

    other.m_textureImage = VK_NULL_HANDLE;
    other.m_textureImageMemory = VK_NULL_HANDLE;
    other.m_textureImageView = VK_NULL_HANDLE;
    other.m_textureSampler = VK_NULL_HANDLE;

    return *this;
}

VkImageView Texture::getImageView() {
    return m_textureImageView;
}

VkImageView* Texture::getImageViewPtr() {
    return &m_textureImageView;
}

VkSampler Texture::getTextureSampler() {
    return m_textureSampler;
}

VkCommandBuffer Texture::beginSingleTimeCommands() {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_window->graphicsCommandPool();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    devFuncs->vkAllocateCommandBuffers(dev, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    devFuncs->vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void Texture::endSingleTimeCommands(VkCommandBuffer commandBuffer, VkSemaphore *waitSemaphorePtr) {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);
    devFuncs->vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (waitSemaphorePtr) {
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphorePtr;
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT };
        submitInfo.pWaitDstStageMask = waitStages;
    }
    devFuncs->vkQueueSubmit(m_window->graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    devFuncs->vkQueueWaitIdle(m_window->graphicsQueue());
    devFuncs->vkFreeCommandBuffers(dev, m_window->graphicsCommandPool(), 1, &commandBuffer);
}

void Texture::clear(float r, float g, float b, float a) {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    if (!m_depthAttachment) {
        VkClearColorValue color = { .float32 = {r, g, b, a} };
        VkImageSubresourceRange imageSubresourceRange { VK_IMAGE_ASPECT_COLOR_BIT, 0, m_mipLevels, 0, 1 };
        devFuncs->vkCmdClearColorImage(commandBuffer, m_textureImage, VK_IMAGE_LAYOUT_GENERAL, &color, 1, &imageSubresourceRange);
    } else {
        VkClearDepthStencilValue color{ 1, 0 };
        VkImageSubresourceRange imageSubresourceRange { VK_IMAGE_ASPECT_DEPTH_BIT, 0, m_mipLevels, 0, 1 };
        devFuncs->vkCmdClearDepthStencilImage(commandBuffer, m_textureImage, VK_IMAGE_LAYOUT_GENERAL, &color, 1, &imageSubresourceRange);
    }
    endSingleTimeCommands(commandBuffer);

}

void Texture::transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_textureImage;
    barrier.subresourceRange.aspectMask = m_depthAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = m_mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
        if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | (m_compute?VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:0);
        }
    } else {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | (m_compute?VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:0);
    }

    devFuncs->vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
        );

    endSingleTimeCommands(commandBuffer);
}

void Texture::saveToPng(const QString& path) {
    assert(m_depth == 1 && m_format == VK_FORMAT_R8G8B8A8_UNORM);
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkImage dstImage;
    VkDeviceMemory dstImageMemory;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_width;
    imageInfo.extent.height = m_height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.samples = m_sampleCount;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (devFuncs->vkCreateImage(dev, &imageInfo, nullptr, &dstImage) != VK_SUCCESS) {
        m_window->crash("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    devFuncs->vkGetImageMemoryRequirements(dev, dstImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = m_window->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (devFuncs->vkAllocateMemory(dev, &allocInfo, nullptr, &dstImageMemory) != VK_SUCCESS) {
        m_window->crash("failed to allocate image memory!");
    }

    devFuncs->vkBindImageMemory(dev, dstImage, dstImageMemory, 0);

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    VkImageMemoryBarrier destBarrier{};
    destBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    destBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    destBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    destBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    destBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    destBarrier.image = dstImage;
    destBarrier.subresourceRange.aspectMask = m_depthAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    destBarrier.subresourceRange.baseMipLevel = 0;
    destBarrier.subresourceRange.levelCount = 1;
    destBarrier.subresourceRange.baseArrayLayer = 0;
    destBarrier.subresourceRange.layerCount = 1;
    destBarrier.srcAccessMask = 0;
    destBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    devFuncs->vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &destBarrier);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = m_textureImage;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = m_depthAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    devFuncs->vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    VkImageCopy imageCopyRegion{};
    imageCopyRegion.srcSubresource.aspectMask = m_depthAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.srcSubresource.layerCount = 1;
    imageCopyRegion.dstSubresource.aspectMask = m_depthAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.dstSubresource.layerCount = 1;
    imageCopyRegion.extent.width = m_width;
    imageCopyRegion.extent.height = m_height;
    imageCopyRegion.extent.depth = 1;

    devFuncs->vkCmdCopyImage(
        commandBuffer,
        m_textureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &imageCopyRegion);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    devFuncs->vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    endSingleTimeCommands(commandBuffer);

    const uint8_t* data;
    devFuncs->vkMapMemory(dev, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void**)&data);

    VkImageSubresource subResource { VkImageAspectFlags(m_depthAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), 0, 0 };
    VkSubresourceLayout subResourceLayout;
    devFuncs->vkGetImageSubresourceLayout(dev, dstImage, &subResource, &subResourceLayout);
    data += subResourceLayout.offset;

    stbi_write_png(path.toUtf8(), m_width, m_height, 4, data, subResourceLayout.rowPitch);

    devFuncs->vkUnmapMemory(dev, dstImageMemory);
    devFuncs->vkFreeMemory(dev, dstImageMemory, nullptr);
    devFuncs->vkDestroyImage(dev, dstImage, nullptr);
}

void Texture::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t depth) {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = m_depthAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        width,
        height,
        depth
    };

    devFuncs->vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

void Texture::createImage(VkImageUsageFlags usage, VkMemoryPropertyFlags properties) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = m_depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_width;
    imageInfo.extent.height = m_height;
    imageInfo.extent.depth = m_depth;
    imageInfo.mipLevels = m_mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = m_sampleCount;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    if (devFuncs->vkCreateImage(dev, &imageInfo, nullptr, &m_textureImage) != VK_SUCCESS) {
        m_window->crash("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    devFuncs->vkGetImageMemoryRequirements(dev, m_textureImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = m_window->findMemoryType(memRequirements.memoryTypeBits, properties);

    if (devFuncs->vkAllocateMemory(dev, &allocInfo, nullptr, &m_textureImageMemory) != VK_SUCCESS) {
        m_window->crash("failed to allocate image memory!");
    }

    devFuncs->vkBindImageMemory(dev, m_textureImage, m_textureImageMemory, 0);
}

void Texture::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    if (devFuncs->vkCreateBuffer(dev, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        m_window->crash("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    devFuncs->vkGetBufferMemoryRequirements(dev, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = m_window->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    if (devFuncs->vkAllocateMemory(dev, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        m_window->crash("failed to allocate buffer memory!");
    }

    devFuncs->vkBindBufferMemory(dev, buffer, bufferMemory, 0);
}

void Texture::createTextureImage() {
    m_mipLevels = m_depth > 1 || m_disableMipmap ? 1 : static_cast<uint32_t>(std::floor(std::log2(std::max(m_width, m_height)))) + 1;
    createImage((m_colorAttachment?VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT:0) | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | (m_depthAttachment?VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:0) |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | (m_compute?VK_IMAGE_USAGE_STORAGE_BIT:0),
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
}


void Texture::createTextureImage(const QString& path, bool *result) {
    int texWidth = 0, texHeight = 0, texChannels = 0;
    stbi_uc *pixels = nullptr;
    pixels = stbi_load(path.toUtf8(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels || !texWidth || !texHeight || !texChannels) {
        if (!result) {
            m_window->crash("failed to load texture image: "+path);
        } else {
            *result = false;
        }
        return;
    }

    VkDeviceSize imageSize = texWidth * texHeight * 4;
    m_width = texWidth/sqrt(m_depth);
    m_height = texHeight/sqrt(m_depth);
    m_mipLevels = m_depth > 1 || m_disableMipmap ? 1 : static_cast<uint32_t>(std::floor(std::log2(std::max(m_width, m_height)))) + 1;

    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    devFuncs->vkMapMemory(dev, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    devFuncs->vkUnmapMemory(dev, stagingBufferMemory);
    stbi_image_free(pixels);

    createImage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | (m_compute?VK_IMAGE_USAGE_STORAGE_BIT:0),
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, m_textureImage, m_width, m_height, m_depth);
    transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    devFuncs->vkDestroyBuffer(dev, stagingBuffer, nullptr);
    devFuncs->vkFreeMemory(dev, stagingBufferMemory, nullptr);
}

void Texture::copyTextureImage(const Texture& other) {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    createImage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | (m_compute?VK_IMAGE_USAGE_STORAGE_BIT:0),
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = other.m_textureImage;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = m_depthAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = m_mipLevels;

    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    devFuncs->vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    VkImageCopy imageCopyRegion{};
    imageCopyRegion.srcSubresource.aspectMask = m_depthAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.srcSubresource.layerCount = 1;
    imageCopyRegion.dstSubresource.aspectMask = m_depthAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.dstSubresource.layerCount = 1;
    imageCopyRegion.extent.width = m_width;
    imageCopyRegion.extent.height = m_height;
    imageCopyRegion.extent.depth = m_depth;

    devFuncs->vkCmdCopyImage(
        commandBuffer,
        other.m_textureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        m_textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &imageCopyRegion);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    devFuncs->vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    endSingleTimeCommands(commandBuffer);
    transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
}

void Texture::blitTextureImage(const Texture& other) {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = other.m_textureImage;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = m_depthAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = std::min(other.m_mipLevels, m_mipLevels);

    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    devFuncs->vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    VkImageBlit blit{};
    blit.srcSubresource.aspectMask = m_depthAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.layerCount = 1;
    blit.dstSubresource.aspectMask = m_depthAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.layerCount = 1;
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {(int32_t)m_width, (int32_t)m_height, (int32_t)m_depth};
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {(int32_t)m_width, (int32_t)m_height, (int32_t)m_depth};

    devFuncs->vkCmdBlitImage(
        commandBuffer,
        other.m_textureImage, VK_IMAGE_LAYOUT_GENERAL,
        m_textureImage, VK_IMAGE_LAYOUT_GENERAL,
        1,
        &blit, VK_FILTER_NEAREST);

    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    devFuncs->vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    endSingleTimeCommands(commandBuffer);
    generateMipmaps();
}

void Texture::createImageView() {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_textureImage;
    viewInfo.viewType = m_depth > 1 ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_format;
    viewInfo.subresourceRange.aspectMask = m_depthAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : m_depthAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = m_mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (devFuncs->vkCreateImageView(dev, &viewInfo, nullptr, &m_textureImageView) != VK_SUCCESS) {
        m_window->crash("failed to create texture image view!");
    }
}

void Texture::createTextureSampler() {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkPhysicalDeviceProperties properties{};
    m_window->vulkanInstance()->functions()->vkGetPhysicalDeviceProperties(m_window->physicalDevice(), &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.maxLod = static_cast<float>(m_mipLevels);

    if (devFuncs->vkCreateSampler(dev, &samplerInfo, nullptr, &m_textureSampler) != VK_SUCCESS) {
        m_window->crash("failed to create texture sampler!");
    }
}

void Texture::generateMipmaps(VkSemaphore *waitSemaphorePtr) {
    if (m_depth > 1 || m_disableMipmap) {
        return;
    }

    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    m_window->vulkanInstance()->functions()->
        vkGetPhysicalDeviceFormatProperties(m_window->physicalDevice(), m_format, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        m_window->crash("texture image format does not support linear blitting!");
    }

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = m_textureImage;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = m_depthAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = m_width;
    int32_t mipHeight = m_height;

    for (uint32_t i = 1; i < m_mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        devFuncs->vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = m_depthAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = m_depthAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        devFuncs->vkCmdBlitImage(commandBuffer,
            m_textureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            m_textureImage, VK_IMAGE_LAYOUT_GENERAL,
            1, &blit,
            VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        devFuncs->vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = m_mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    devFuncs->vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    endSingleTimeCommands(commandBuffer, waitSemaphorePtr);
}

VkImage Texture::getImage() {
    return m_textureImage;
}

uint32_t Texture::getMipLevels() {
    return m_mipLevels;
}

