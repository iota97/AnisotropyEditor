#include "src/Texture/cubemap.h"
#include "src/Texture/stb_image.h"
#include <QCoreApplication>

CubeMap::CubeMap(const std::array<QString, 6> &paths, VulkanWindow *w) : m_window(w) {
    createTextureImage(paths);
    createImageView();
    createTextureSampler();
    generateMipmaps();
    copyTextureImage();

    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_window->getComputeQueueFamilyIndex();
    if (devFuncs->vkCreateCommandPool(dev, &poolInfo, nullptr, &m_computeCommandPool) != VK_SUCCESS) {
        m_window->crash("failed to create compute command pool!");
    }

    createComputeDescriptorSet();
    createComputePipelineLayout();
    createComputePipeline();
    for (uint32_t i = 1; i < m_mipLevels; ++i) {
        integrate(i);
    }
}

CubeMap::~CubeMap() {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);
    devFuncs->vkDeviceWaitIdle(dev);

    if (m_textureSampler)
        devFuncs->vkDestroySampler(dev, m_textureSampler, nullptr);
    if (m_textureImageView[0])
        devFuncs->vkDestroyImageView(dev, m_textureImageView[0], nullptr);
    if (m_textureImage[0])
        devFuncs->vkDestroyImage(dev, m_textureImage[0], nullptr);
    if (m_textureImageMemory[0])
        devFuncs->vkFreeMemory(dev, m_textureImageMemory[0], nullptr);
    if (m_textureImageView[1])
        devFuncs->vkDestroyImageView(dev, m_textureImageView[1], nullptr);
    if (m_textureImage[1])
        devFuncs->vkDestroyImage(dev, m_textureImage[1], nullptr);
    if (m_textureImageMemory[1])
        devFuncs->vkFreeMemory(dev, m_textureImageMemory[1], nullptr);

    if (m_computeCommandPool)
        devFuncs->vkDestroyCommandPool(dev, m_computeCommandPool, nullptr);

    if (m_computePipeline)
        devFuncs->vkDestroyPipeline(dev, m_computePipeline, nullptr);

    if (m_computePipelineLayout)
        devFuncs->vkDestroyPipelineLayout(dev, m_computePipelineLayout, nullptr);

    if (m_computeDescSetLayout)
        devFuncs->vkDestroyDescriptorSetLayout(dev, m_computeDescSetLayout, nullptr);

    if (m_computeDescPool)
        devFuncs->vkDestroyDescriptorPool(dev, m_computeDescPool, nullptr);

}

void CubeMap::copyTextureImage() {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = m_textureImage[0];
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask =  VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;
    barrier.subresourceRange.levelCount = m_mipLevels;

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
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.layerCount = 6;
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.layerCount = 6;
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {(int32_t)m_width, (int32_t)m_height, (int32_t)1};
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {(int32_t)m_width, (int32_t)m_height, (int32_t)1};

    devFuncs->vkCmdBlitImage(
        commandBuffer,
        m_textureImage[0], VK_IMAGE_LAYOUT_GENERAL,
        m_textureImage[1], VK_IMAGE_LAYOUT_GENERAL,
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
}

VkImageView CubeMap::getImageView(uint32_t id) {
    return m_textureImageView[id];
}

VkSampler CubeMap::getTextureSampler() {
    return m_textureSampler;
}

void CubeMap::generateMipmaps() {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    m_window->vulkanInstance()->functions()->
        vkGetPhysicalDeviceFormatProperties(m_window->physicalDevice(), VK_FORMAT_R32G32B32A32_SFLOAT, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        m_window->crash("texture image format does not support linear blitting!");
    }

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = m_textureImage[0];
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;
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
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 6;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 6;

        devFuncs->vkCmdBlitImage(commandBuffer,
                                 m_textureImage[0], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 m_textureImage[0], VK_IMAGE_LAYOUT_GENERAL,
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

    endSingleTimeCommands(commandBuffer);
}

void CubeMap::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
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

void CubeMap::createImage(VkImageUsageFlags usage, VkMemoryPropertyFlags properties) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_width;
    imageInfo.extent.height = m_height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = m_mipLevels;
    imageInfo.arrayLayers = 6;
    imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    if (devFuncs->vkCreateImage(dev, &imageInfo, nullptr, &m_textureImage[0]) != VK_SUCCESS) {
        m_window->crash("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    devFuncs->vkGetImageMemoryRequirements(dev, m_textureImage[0], &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = m_window->findMemoryType(memRequirements.memoryTypeBits, properties);

    if (devFuncs->vkAllocateMemory(dev, &allocInfo, nullptr, &m_textureImageMemory[0]) != VK_SUCCESS) {
        m_window->crash("failed to allocate image memory!");
    }

    devFuncs->vkBindImageMemory(dev, m_textureImage[0], m_textureImageMemory[0], 0);

    VkImageCreateInfo imageInfo1 = imageInfo;
    imageInfo1.format = VK_FORMAT_R16G16B16A16_SFLOAT;

    if (devFuncs->vkCreateImage(dev, &imageInfo1, nullptr, &m_textureImage[1]) != VK_SUCCESS) {
        m_window->crash("failed to create image!");
    }

    VkMemoryRequirements memRequirements1;
    devFuncs->vkGetImageMemoryRequirements(dev, m_textureImage[1], &memRequirements1);

    VkMemoryAllocateInfo allocInfo1{};
    allocInfo1.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo1.allocationSize = memRequirements1.size;
    allocInfo1.memoryTypeIndex = m_window->findMemoryType(memRequirements1.memoryTypeBits, properties);

    if (devFuncs->vkAllocateMemory(dev, &allocInfo1, nullptr, &m_textureImageMemory[1]) != VK_SUCCESS) {
        m_window->crash("failed to allocate image memory!");
    }

    devFuncs->vkBindImageMemory(dev, m_textureImage[1], m_textureImageMemory[1], 0);
}

void CubeMap::createTextureImage(const std::array<QString, 6> &paths) {
    int texWidth = 0, texHeight = 0, texChannels = 0;
    float *pixels[6]{};
    for (int i = 0; i < 6; ++i) {
        pixels[i] = stbi_loadf(paths[i].toUtf8(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        if (!pixels[i]) {
            for (int j = 0; j < i; j++) {
                stbi_image_free(pixels[j]);
            }
            m_window->crash("failed to load texture image: "+paths[i]);
            return;
        }
    }

    m_width = texWidth;
    m_height = texHeight;
    m_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(m_width, m_height)))) + 1;

    VkDeviceSize imageSize = texWidth * texHeight * 4 * 6 * 4;
    VkDeviceSize layerSize = imageSize / 6; //This is just the size of each layer.

    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    devFuncs->vkMapMemory(dev, stagingBufferMemory, 0, imageSize, 0, &data);
    for (int i = 0; i < 6; ++i) {
        memcpy(static_cast<char*>(data)+layerSize*i, pixels[i], static_cast<size_t>(layerSize));
    }
    devFuncs->vkUnmapMemory(dev, stagingBufferMemory);

    for (int i = 0; i < 6; i++) {
        stbi_image_free(pixels[i]);
    }

    createImage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, m_textureImage[0]);
    transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1);


    devFuncs->vkDestroyBuffer(dev, stagingBuffer, nullptr);
    devFuncs->vkFreeMemory(dev, stagingBufferMemory, nullptr);
}

void CubeMap::copyBufferToImage(VkBuffer buffer, VkImage image) {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 6;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        m_width,
        m_height,
        1
    };

    devFuncs->vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

void CubeMap::transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t id) {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_textureImage[id];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = m_mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;

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
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
    } else {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
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

void CubeMap::createImageView() {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_textureImage[0];
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = m_mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    if (devFuncs->vkCreateImageView(dev, &viewInfo, nullptr, &m_textureImageView[0]) != VK_SUCCESS) {
        m_window->crash("failed to create texture image view!");
    }

    VkImageViewCreateInfo viewInfo1 = viewInfo;
    viewInfo1.image = m_textureImage[1];
    viewInfo1.format = VK_FORMAT_R16G16B16A16_SFLOAT;

    if (devFuncs->vkCreateImageView(dev, &viewInfo1, nullptr, &m_textureImageView[1]) != VK_SUCCESS) {
        m_window->crash("failed to create texture image view!");
    }
}

void CubeMap::createTextureSampler() {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkPhysicalDeviceProperties properties{};
    m_window->vulkanInstance()->functions()->vkGetPhysicalDeviceProperties(m_window->physicalDevice(), &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.maxLod = m_mipLevels;

    if (devFuncs->vkCreateSampler(dev, &samplerInfo, nullptr, &m_textureSampler) != VK_SUCCESS) {
        m_window->crash("failed to create texture sampler!");
    }
}

VkCommandBuffer CubeMap::beginSingleTimeCommands() {
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

void CubeMap::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);
    devFuncs->vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    devFuncs->vkQueueSubmit(m_window->graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    devFuncs->vkQueueWaitIdle(m_window->graphicsQueue());
    devFuncs->vkFreeCommandBuffers(dev, m_window->graphicsCommandPool(), 1, &commandBuffer);
}

uint32_t CubeMap::getWidth() const {
    return m_width;
}

void CubeMap::createComputeDescriptorSet() {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    // Set up descriptor set and its layout.
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 6;

    VkDescriptorPoolCreateInfo descPoolInfo {};
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descPoolInfo.pPoolSizes = poolSizes.data();
    descPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    descPoolInfo.maxSets = 1;

    VkResult err = devFuncs->vkCreateDescriptorPool(dev, &descPoolInfo, nullptr, &m_computeDescPool);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create descriptor pool");

    VkDescriptorSetLayoutBinding cubeBinding = {
        0, // binding
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        1,
        VK_SHADER_STAGE_COMPUTE_BIT,
        nullptr
    };
    VkDescriptorSetLayoutBinding mipBinding = {
        1, // binding
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        1,
        VK_SHADER_STAGE_COMPUTE_BIT,
        nullptr
    };

    std::array<VkDescriptorBindingFlags, 2> flags{VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT};
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags {};
    bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlags.pNext = nullptr;
    bindingFlags.pBindingFlags = flags.data();
    bindingFlags.bindingCount = static_cast<uint32_t>(flags.size());;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {cubeBinding, mipBinding};
    VkDescriptorSetLayoutCreateInfo descLayoutInfo{};
    descLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    descLayoutInfo.pBindings = bindings.data();
    descLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    descLayoutInfo.pNext = &bindingFlags;

    err = devFuncs->vkCreateDescriptorSetLayout(dev, &descLayoutInfo, nullptr, &m_computeDescSetLayout);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create descriptor set layout");

    VkDescriptorSetAllocateInfo descSetAllocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        nullptr,
        m_computeDescPool,
        1,
        &m_computeDescSetLayout
    };

    err = devFuncs->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_computeDescSet);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to allocate descriptor set");
}

void CubeMap::createComputePipelineLayout() {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkPipelineLayoutCreateInfo m_computePipelineLayoutInfo{};
    m_computePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    m_computePipelineLayoutInfo.setLayoutCount = 1;
    m_computePipelineLayoutInfo.pSetLayouts = &m_computeDescSetLayout;

    if (devFuncs->vkCreatePipelineLayout(dev, &m_computePipelineLayoutInfo, nullptr, &m_computePipelineLayout) != VK_SUCCESS) {
        m_window->crash("failed to create compute pipeline layout!");
    }
}

void CubeMap::createComputePipeline() {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkShaderModule computeShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                                "/assets/shaders/cubeIntegrator_comp.spv");

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_computePipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (devFuncs->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_computePipeline) != VK_SUCCESS) {
        m_window->crash("failed to create compute pipeline!");
    }

    devFuncs->vkDestroyShaderModule(dev, computeShaderModule, nullptr);
}

void CubeMap::integrate(uint32_t lod) {
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);
    devFuncs->vkDeviceWaitIdle(dev);

    std::array<VkImageView, 2> imageView;
    std::array<VkImageViewCreateInfo, 2> viewInfo{};
    viewInfo[0].sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo[0].image = m_textureImage[0];
    viewInfo[0].viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo[0].subresourceRange.baseMipLevel = 0;
    viewInfo[0].subresourceRange.levelCount = m_mipLevels;
    viewInfo[0].subresourceRange.baseArrayLayer = 0;
    viewInfo[0].subresourceRange.layerCount = 6;

    viewInfo[1].sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo[1].image = m_textureImage[1];
    viewInfo[1].viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo[1].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo[1].subresourceRange.baseMipLevel = lod;
    viewInfo[1].subresourceRange.levelCount = 1;
    viewInfo[1].subresourceRange.baseArrayLayer = 0;
    viewInfo[1].subresourceRange.layerCount = 6;

    if (devFuncs->vkCreateImageView(dev, &viewInfo[0], nullptr, &imageView[0]) != VK_SUCCESS) {
        m_window->crash("failed to create texture image view!");
    }

    if (devFuncs->vkCreateImageView(dev, &viewInfo[1], nullptr, &imageView[1]) != VK_SUCCESS) {
        m_window->crash("failed to create texture image view!");
    }

    std::array<VkDescriptorImageInfo, 2> imageInfo{};
    imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo[0].imageView = imageView[0];
    imageInfo[0].sampler = getTextureSampler();
    imageInfo[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo[1].imageView = imageView[1];

    std::array<VkWriteDescriptorSet, 2> descWrites{};
    descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[0].dstSet = m_computeDescSet;
    descWrites[0].dstBinding = 0;
    descWrites[0].dstArrayElement = 0;
    descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descWrites[0].descriptorCount = 1;
    descWrites[0].pImageInfo = &imageInfo[0];
    descWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[1].dstSet = m_computeDescSet;
    descWrites[1].dstBinding = 1;
    descWrites[1].dstArrayElement = 0;
    descWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descWrites[1].descriptorCount = 1;
    descWrites[1].pImageInfo = &imageInfo[1];

    devFuncs->vkUpdateDescriptorSets(dev, static_cast<uint32_t>(descWrites.size()), descWrites.data(), 0, nullptr);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_computeCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    devFuncs->vkAllocateCommandBuffers(dev, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    devFuncs->vkBeginCommandBuffer(commandBuffer, &beginInfo);
    devFuncs->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline);
    devFuncs->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 1, &m_computeDescSet, 0, 0);
    devFuncs->vkCmdDispatch(commandBuffer, ceil(getWidth()/16.0), ceil(getWidth()/16.0), 1);
    devFuncs->vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkQueue computeQueue;
    devFuncs->vkGetDeviceQueue(dev, m_window->getComputeQueueFamilyIndex(), 0, &computeQueue);
    devFuncs->vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
    devFuncs->vkQueueWaitIdle(computeQueue);
    devFuncs->vkFreeCommandBuffers(dev, m_computeCommandPool, 1, &commandBuffer);
    devFuncs->vkDestroyImageView(dev, imageView[0], nullptr);
    devFuncs->vkDestroyImageView(dev, imageView[1], nullptr);
}
