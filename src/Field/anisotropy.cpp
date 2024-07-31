#include "anisotropy.h"
#include <QCoreApplication>
#include "src/Render/montecarlo.h"

Anisotropy::Anisotropy(VulkanWindow *window, MonteCarlo*& monteCarlo) : m_window(window), m_monteCarlo(monteCarlo) {
    VkDevice dev = m_window->device();
    m_devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_window->getComputeQueueFamilyIndex();
    if (m_devFuncs->vkCreateCommandPool(dev, &poolInfo, nullptr, &m_computeCommandPool) != VK_SUCCESS) {
        m_window->crash("failed to create compute command pool!");
    }

    Texture tmp(QCoreApplication::applicationDirPath()+"/assets/textures/wave.png", m_window);
    m_anisoDir = new Texture(tmp.getWidth(), tmp.getHeight(), m_window, 1, VK_FORMAT_R16G16B16A16_SFLOAT, true);
    m_anisoDir->blitTextureImage(tmp);
    m_anisoMap = new Texture(m_anisoDir->getWidth(), m_anisoDir->getHeight(), m_window, 1, VK_FORMAT_R16G16B16A16_SFLOAT, true);

    createComputeUniformBuffer();
    createComputeDescriptorSet();
    createComputePipelineLayout();
    createDir2CovPipeline();
    createDir2DirPipeline();
    createDir2ZebraPipeline();
    createExportPipeline();
    createAngle2DirPipeline();
    createHalfAngle2DirPipeline();
    updateAnisotropyTextureDescriptor();
}

Anisotropy::~Anisotropy() {
    delete m_anisoDir;
    delete m_anisoMap;

    VkDevice dev = m_window->device();

    if (m_computeCommandPool) {
        m_devFuncs->vkDestroyCommandPool(dev, m_computeCommandPool, nullptr);
        m_computeCommandPool = VK_NULL_HANDLE;
    }

    if (m_dir2CovPipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_dir2CovPipeline, nullptr);
        m_dir2CovPipeline = VK_NULL_HANDLE;
    }

    if (m_exportPipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_exportPipeline, nullptr);
        m_exportPipeline = VK_NULL_HANDLE;
    }

    if (m_halfAngle2DirPipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_halfAngle2DirPipeline, nullptr);
        m_halfAngle2DirPipeline = VK_NULL_HANDLE;
    }

    if (m_angle2DirPipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_angle2DirPipeline, nullptr);
        m_angle2DirPipeline = VK_NULL_HANDLE;
    }

    if (m_dir2DirPipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_dir2DirPipeline, nullptr);
        m_dir2DirPipeline = VK_NULL_HANDLE;
    }

    if (m_dir2ZebraPipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_dir2ZebraPipeline, nullptr);
        m_dir2ZebraPipeline = VK_NULL_HANDLE;
    }

    if (m_computePipelineLayout) {
        m_devFuncs->vkDestroyPipelineLayout(dev, m_computePipelineLayout, nullptr);
        m_computePipelineLayout = VK_NULL_HANDLE;
    }

    if (m_computeDescSetLayout) {
        m_devFuncs->vkDestroyDescriptorSetLayout(dev, m_computeDescSetLayout, nullptr);
        m_computeDescSetLayout = VK_NULL_HANDLE;
    }

    if (m_computeDescPool) {
        m_devFuncs->vkDestroyDescriptorPool(dev, m_computeDescPool, nullptr);
        m_computeDescPool = VK_NULL_HANDLE;
    }

    if (m_computeUniformBuf) {
        m_devFuncs->vkDestroyBuffer(dev, m_computeUniformBuf, nullptr);
        m_computeUniformBuf = VK_NULL_HANDLE;
    }

    if (m_computeUniformBufMem) {
        m_devFuncs->vkFreeMemory(dev, m_computeUniformBufMem, nullptr);
        m_computeUniformBufMem = VK_NULL_HANDLE;
    }
}

Texture* Anisotropy::getMap() {
    return m_anisoMap;
}

Texture* Anisotropy::getDir() {
    return m_anisoDir;
}

void Anisotropy::newAnisoDirTexture(uint32_t width, uint32_t heigth) {
    Texture *oldAnisoDir = m_anisoDir;
    Texture *oldAnisoMap = m_anisoMap;
    m_anisoDir = new Texture(width, heigth, m_window, 1, VK_FORMAT_R16G16B16A16_SFLOAT, true);
    m_anisoDir->clear(1, 0.5, 0.5, 1.0);
    m_anisoMap = new Texture(width, heigth, m_window, 1, VK_FORMAT_R16G16B16A16_SFLOAT, true);
    updateAnisotropyTextureDescriptor();

    VkDevice dev = m_window->device();
    m_devFuncs->vkDeviceWaitIdle(dev);
    delete oldAnisoDir;
    delete oldAnisoMap;
}


void Anisotropy::setAnisoDirTexture(const QString &path) {
    Texture *oldAnisoDir = m_anisoDir;
    Texture *oldAnisoMap = m_anisoMap;

    bool result = true;
    Texture tmp(path, m_window, 1, true, false, false, VK_SAMPLE_COUNT_1_BIT, &result);
    if (!result) {
        return;
    }
    m_anisoDir = new Texture(tmp.getWidth(), tmp.getHeight(), m_window, 1, VK_FORMAT_R16G16B16A16_SFLOAT , true);
    m_anisoDir->blitTextureImage(tmp);

    m_anisoMap = new Texture(m_anisoDir->getWidth(), m_anisoDir->getHeight(), m_window, 1, VK_FORMAT_R16G16B16A16_SFLOAT, true);
    updateAnisotropyTextureDescriptor();
    delete oldAnisoDir;
    delete oldAnisoMap;
}

void Anisotropy::setAnisoAngleTexture(const QString &path, bool half) {
    Texture *oldAnisoDir = m_anisoDir;
    Texture *oldAnisoMap = m_anisoMap;

    bool result = true;
    Texture tmp(path, m_window, 1, true, false, false, VK_SAMPLE_COUNT_1_BIT, &result);
    if (!result) {
        return;
    }
    m_anisoDir = new Texture(tmp.getWidth(), tmp.getHeight(), m_window, 1, VK_FORMAT_R16G16B16A16_SFLOAT , true);
    m_anisoDir->blitTextureImage(tmp);

    m_anisoMap = new Texture(m_anisoDir->getWidth(), m_anisoDir->getHeight(), m_window, 1, VK_FORMAT_R16G16B16A16_SFLOAT, true);
    convertAnisoAngleTexture(half);
    updateAnisotropyTextureDescriptor();
    delete oldAnisoDir;
    delete oldAnisoMap;
}

void Anisotropy::updateAnisotropyTextureMap() {
    VkDevice dev = m_window->device();

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_computeCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    m_devFuncs->vkAllocateCommandBuffers(dev, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    m_devFuncs->vkBeginCommandBuffer(commandBuffer, &beginInfo);
    m_devFuncs->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_dir2CovPipeline);
    m_devFuncs->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 1, &m_computeDescSet, 0, 0);
    m_devFuncs->vkCmdDispatch(commandBuffer, ceil(m_anisoDir->getWidth()/16.0), ceil(m_anisoDir->getHeight()/16.0), 1);
    m_devFuncs->vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkQueue computeQueue;
    m_devFuncs->vkGetDeviceQueue(dev, m_window->getComputeQueueFamilyIndex(), 0, &computeQueue);
    m_devFuncs->vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
    m_devFuncs->vkQueueWaitIdle(computeQueue);
    m_anisoMap->generateMipmaps();

    if (m_monteCarlo) m_monteCarlo->clear();
}

void Anisotropy::saveZebra(const QString &path) {
    VkDevice dev = m_window->device();

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_computeCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    m_devFuncs->vkAllocateCommandBuffers(dev, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    m_devFuncs->vkBeginCommandBuffer(commandBuffer, &beginInfo);
    m_devFuncs->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_dir2ZebraPipeline);
    m_devFuncs->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 1, &m_computeDescSet, 0, 0);
    m_devFuncs->vkCmdDispatch(commandBuffer, ceil(m_anisoDir->getWidth()/16.0), ceil(m_anisoDir->getHeight()/16.0), 1);
    m_devFuncs->vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkQueue computeQueue;
    m_devFuncs->vkGetDeviceQueue(dev, m_window->getComputeQueueFamilyIndex(), 0, &computeQueue);
    m_devFuncs->vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
    m_devFuncs->vkQueueWaitIdle(computeQueue);

    Texture tmp(m_anisoMap->getWidth(), m_anisoMap->getHeight(),
                m_window, 1, VK_FORMAT_R8G8B8A8_UNORM, true);
    tmp.blitTextureImage(*m_anisoMap);
    tmp.saveToPng(path);

    updateAnisotropyTextureMap();
}

void Anisotropy::setAnisoValues(float roughness, float anisotropy) {
    VkDevice dev = m_window->device();
    float *p;
    VkResult err = m_devFuncs->vkMapMemory(dev, m_computeUniformBufMem, 0, 2*sizeof(float), 0, reinterpret_cast<void **>(&p));
    if (err != VK_SUCCESS)
        m_window->crash("Failed to map memory");
    p[0] = roughness;
    p[1] = anisotropy;
    m_devFuncs->vkUnmapMemory(dev, m_computeUniformBufMem);

    updateAnisotropyTextureMap();
}

void Anisotropy::convertAnisoAngleTexture(bool half) {
    VkDevice dev = m_window->device();

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.imageView = m_anisoDir->getImageView();
    imageInfo.sampler = m_anisoDir->getTextureSampler();

    VkWriteDescriptorSet descWrites{};
    descWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites.dstSet = m_computeDescSet;
    descWrites.dstBinding = 1;
    descWrites.dstArrayElement = 0;
    descWrites.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descWrites.descriptorCount = 1;
    descWrites.pImageInfo = &imageInfo;

    m_devFuncs->vkUpdateDescriptorSets(m_window->device(), 1, &descWrites, 0, nullptr);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_computeCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    m_devFuncs->vkAllocateCommandBuffers(dev, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    m_devFuncs->vkBeginCommandBuffer(commandBuffer, &beginInfo);
    m_devFuncs->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, half ?  m_halfAngle2DirPipeline : m_angle2DirPipeline);
    m_devFuncs->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 1, &m_computeDescSet, 0, 0);
    m_devFuncs->vkCmdDispatch(commandBuffer, ceil(m_anisoDir->getWidth()/16.0), ceil(m_anisoDir->getHeight()/16.0), 1);
    m_devFuncs->vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkQueue computeQueue;
    m_devFuncs->vkGetDeviceQueue(dev, m_window->getComputeQueueFamilyIndex(), 0, &computeQueue);
    m_devFuncs->vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
    m_devFuncs->vkQueueWaitIdle(computeQueue);
    m_devFuncs->vkFreeCommandBuffers(dev, m_computeCommandPool, 1, &commandBuffer);
    m_anisoDir->generateMipmaps();
}

void Anisotropy::updateAnisotropyTextureDescriptor() {
    VkDevice dev = m_window->device();

    std::array<VkDescriptorImageInfo, 2> imageInfo{};
    imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo[0].imageView = m_anisoDir->getImageView();
    imageInfo[0].sampler = m_anisoDir->getTextureSampler();
    imageInfo[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo[1].imageView = m_anisoMap->getImageView();
    imageInfo[1].sampler = m_anisoMap->getTextureSampler();

    std::array<VkWriteDescriptorSet, 2> descWrites{};
    descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[0].dstSet = m_computeDescSet;
    descWrites[0].dstBinding = 1;
    descWrites[0].dstArrayElement = 0;
    descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descWrites[0].descriptorCount = 1;
    descWrites[0].pImageInfo = &imageInfo[0];

    descWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[1].dstSet = m_computeDescSet;
    descWrites[1].dstBinding = 2;
    descWrites[1].dstArrayElement = 0;
    descWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descWrites[1].descriptorCount = 1;
    descWrites[1].pImageInfo = &imageInfo[1];

    m_devFuncs->vkUpdateDescriptorSets(dev, static_cast<uint32_t>(descWrites.size()), descWrites.data(), 0, nullptr);

    updateAnisotropyTextureMap();
}

void Anisotropy::createComputeDescriptorSet() {
    VkDevice dev = m_window->device();

    // Set up descriptor set and its layout.
    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 1;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[2].descriptorCount = 1;

    VkDescriptorPoolCreateInfo descPoolInfo {};
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descPoolInfo.pPoolSizes = poolSizes.data();
    descPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    descPoolInfo.maxSets = 1;

    VkResult err = m_devFuncs->vkCreateDescriptorPool(dev, &descPoolInfo, nullptr, &m_computeDescPool);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create descriptor pool");

    VkDescriptorSetLayoutBinding uniformBinding = {
        0, // binding
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        1,
        VK_SHADER_STAGE_COMPUTE_BIT,
        nullptr
    };
    VkDescriptorSetLayoutBinding inputSamplerBinding = {
        1, // binding
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        1,
        VK_SHADER_STAGE_COMPUTE_BIT,
        nullptr
    };
    VkDescriptorSetLayoutBinding outputSamplerBinding = {
        2, // binding
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        1,
        VK_SHADER_STAGE_COMPUTE_BIT,
        nullptr
    };

    std::array<VkDescriptorBindingFlags, 3> flags{0, VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT, 0};

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags {};
    bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlags.pNext = nullptr;
    bindingFlags.pBindingFlags = flags.data();
    bindingFlags.bindingCount = static_cast<uint32_t>(flags.size());;

    std::array<VkDescriptorSetLayoutBinding, 3> bindings = {uniformBinding, inputSamplerBinding, outputSamplerBinding};
    VkDescriptorSetLayoutCreateInfo descLayoutInfo{};
    descLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    descLayoutInfo.pBindings = bindings.data();
    descLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    descLayoutInfo.pNext = &bindingFlags;

    err = m_devFuncs->vkCreateDescriptorSetLayout(dev, &descLayoutInfo, nullptr, &m_computeDescSetLayout);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create descriptor set layout");

    VkDescriptorSetAllocateInfo descSetAllocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        nullptr,
        m_computeDescPool,
        1,
        &m_computeDescSetLayout
    };

    err = m_devFuncs->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_computeDescSet);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to allocate descriptor set");

    std::array<VkDescriptorImageInfo, 2> imageInfo{};
    imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo[0].imageView = m_anisoDir->getImageView();
    imageInfo[0].sampler = m_anisoDir->getTextureSampler();
    imageInfo[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo[1].imageView = m_anisoMap->getImageView();
    imageInfo[1].sampler = m_anisoMap->getTextureSampler();

    std::array<VkWriteDescriptorSet, 3> descWrites{};

    descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[0].dstSet = m_computeDescSet;
    descWrites[0].dstBinding = 0;
    descWrites[0].dstArrayElement = 0;
    descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descWrites[0].descriptorCount = 1;
    descWrites[0].pBufferInfo = &m_computeUniformBufInfo;

    descWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[1].dstSet = m_computeDescSet;
    descWrites[1].dstBinding = 1;
    descWrites[1].dstArrayElement = 0;
    descWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descWrites[1].descriptorCount = 1;
    descWrites[1].pImageInfo = &imageInfo[0];

    descWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[2].dstSet = m_computeDescSet;
    descWrites[2].dstBinding = 2;
    descWrites[2].dstArrayElement = 0;
    descWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descWrites[2].descriptorCount = 1;
    descWrites[2].pImageInfo = &imageInfo[1];

    m_devFuncs->vkUpdateDescriptorSets(dev, static_cast<uint32_t>(descWrites.size()), descWrites.data(), 0, nullptr);
}

void Anisotropy::createComputePipelineLayout() {
    VkDevice dev = m_window->device();

    VkPipelineLayoutCreateInfo m_computePipelineLayoutInfo{};
    m_computePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    m_computePipelineLayoutInfo.setLayoutCount = 1;
    m_computePipelineLayoutInfo.pSetLayouts = &m_computeDescSetLayout;

    if (m_devFuncs->vkCreatePipelineLayout(dev, &m_computePipelineLayoutInfo, nullptr, &m_computePipelineLayout) != VK_SUCCESS) {
        m_window->crash("failed to create compute pipeline layout!");
    }
}

void Anisotropy::createDir2CovPipeline() {
    VkDevice dev = m_window->device();
    VkShaderModule computeShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                                "/assets/shaders/dir2cov_comp.spv");

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_computePipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (m_devFuncs->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_dir2CovPipeline) != VK_SUCCESS) {
        m_window->crash("failed to create compute pipeline!");
    }

    m_devFuncs->vkDestroyShaderModule(dev, computeShaderModule, nullptr);
}

void Anisotropy::createDir2DirPipeline() {
    VkDevice dev = m_window->device();
    VkShaderModule computeShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                                "/assets/shaders/dir2dir_comp.spv");

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_computePipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (m_devFuncs->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_dir2DirPipeline) != VK_SUCCESS) {
        m_window->crash("failed to create compute pipeline!");
    }

    m_devFuncs->vkDestroyShaderModule(dev, computeShaderModule, nullptr);
}

void Anisotropy::createDir2ZebraPipeline() {
    VkDevice dev = m_window->device();
    VkShaderModule computeShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                                "/assets/shaders/dir2zebra_comp.spv");

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_computePipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (m_devFuncs->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_dir2ZebraPipeline) != VK_SUCCESS) {
        m_window->crash("failed to create compute pipeline!");
    }

    m_devFuncs->vkDestroyShaderModule(dev, computeShaderModule, nullptr);
}

void Anisotropy::createExportPipeline() {
    VkDevice dev = m_window->device();
    VkShaderModule computeShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                                "/assets/shaders/imageExporter_comp.spv");

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_computePipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (m_devFuncs->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_exportPipeline) != VK_SUCCESS) {
        m_window->crash("failed to create compute pipeline!");
    }

    m_devFuncs->vkDestroyShaderModule(dev, computeShaderModule, nullptr);
}

void Anisotropy::createAngle2DirPipeline() {
    VkDevice dev = m_window->device();
    VkShaderModule computeShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                                "/assets/shaders/imageImporter_comp.spv");

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_computePipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (m_devFuncs->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_angle2DirPipeline) != VK_SUCCESS) {
        m_window->crash("failed to create compute pipeline!");
    }

    m_devFuncs->vkDestroyShaderModule(dev, computeShaderModule, nullptr);
}

void Anisotropy::createHalfAngle2DirPipeline() {
    VkDevice dev = m_window->device();
    VkShaderModule computeShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                                "/assets/shaders/half2dir_comp.spv");

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_computePipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (m_devFuncs->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_halfAngle2DirPipeline) != VK_SUCCESS) {
        m_window->crash("failed to create compute pipeline!");
    }

    m_devFuncs->vkDestroyShaderModule(dev, computeShaderModule, nullptr);
}

void Anisotropy::createComputeUniformBuffer() {
    VkDevice dev = m_window->device();
    const VkPhysicalDeviceLimits *pdevLimits = &m_window->physicalDeviceProperties()->limits;
    const VkDeviceSize uniAlign = pdevLimits->minUniformBufferOffsetAlignment;

    VkBufferCreateInfo uniformBufInfo{};
    uniformBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    const VkDeviceSize uniformAllocSize = m_window->aligned(2*sizeof(float), uniAlign);
    uniformBufInfo.size = uniformAllocSize;
    uniformBufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VkResult err = m_devFuncs->vkCreateBuffer(dev, &uniformBufInfo, nullptr, &m_computeUniformBuf);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create buffer");

    VkMemoryRequirements uniformMemReq;
    m_devFuncs->vkGetBufferMemoryRequirements(dev, m_computeUniformBuf, &uniformMemReq);

    VkMemoryAllocateInfo uniformMemAllocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        uniformMemReq.size,
        m_window->findMemoryType(uniformMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    };

    err = m_devFuncs->vkAllocateMemory(dev, &uniformMemAllocInfo, nullptr, &m_computeUniformBufMem);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to allocate memory");

    err = m_devFuncs->vkBindBufferMemory(dev, m_computeUniformBuf, m_computeUniformBufMem, 0);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to bind buffer memory");

    float *p;
    err = m_devFuncs->vkMapMemory(dev, m_computeUniformBufMem, 0, uniformMemReq.size, 0, reinterpret_cast<void **>(&p));
    if (err != VK_SUCCESS)
        m_window->crash("Failed to map memory");

    p[0] = 0.2; //roughness
    p[1] = 0.7; //anisotropy

    memset(&m_computeUniformBufInfo, 0, sizeof(m_computeUniformBufInfo));
    m_computeUniformBufInfo.buffer = m_computeUniformBuf;
    m_computeUniformBufInfo.range = uniformAllocSize;
    m_devFuncs->vkUnmapMemory(dev, m_computeUniformBufMem);
}

void Anisotropy::saveAnisoAngle(const QString &path) {
    VkDevice dev = m_window->device();

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_computeCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    m_devFuncs->vkAllocateCommandBuffers(dev, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    m_devFuncs->vkBeginCommandBuffer(commandBuffer, &beginInfo);
    m_devFuncs->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_exportPipeline);
    m_devFuncs->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 1, &m_computeDescSet, 0, 0);
    m_devFuncs->vkCmdDispatch(commandBuffer, ceil(m_anisoDir->getWidth()/16.0), ceil(m_anisoDir->getHeight()/16.0), 1);
    m_devFuncs->vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkQueue computeQueue;
    m_devFuncs->vkGetDeviceQueue(dev, m_window->getComputeQueueFamilyIndex(), 0, &computeQueue);
    m_devFuncs->vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
    m_devFuncs->vkQueueWaitIdle(computeQueue);

    Texture tmp(m_anisoMap->getWidth(), m_anisoMap->getHeight(),
                m_window, 1, VK_FORMAT_R8G8B8A8_UNORM, true);
    tmp.blitTextureImage(*m_anisoMap);
    tmp.saveToPng(path);

    updateAnisotropyTextureMap();
}

void Anisotropy::saveAnisoDir(const QString &path) {
    VkDevice dev = m_window->device();

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_computeCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    m_devFuncs->vkAllocateCommandBuffers(dev, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    m_devFuncs->vkBeginCommandBuffer(commandBuffer, &beginInfo);
    m_devFuncs->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_dir2DirPipeline);
    m_devFuncs->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 1, &m_computeDescSet, 0, 0);
    m_devFuncs->vkCmdDispatch(commandBuffer, ceil(m_anisoDir->getWidth()/16.0), ceil(m_anisoDir->getHeight()/16.0), 1);
    m_devFuncs->vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkQueue computeQueue;
    m_devFuncs->vkGetDeviceQueue(dev, m_window->getComputeQueueFamilyIndex(), 0, &computeQueue);
    m_devFuncs->vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
    m_devFuncs->vkQueueWaitIdle(computeQueue);

    Texture tmp(m_anisoMap->getWidth(), m_anisoMap->getHeight(),
                m_window, 1, VK_FORMAT_R8G8B8A8_UNORM, true);
    tmp.blitTextureImage(*m_anisoMap);
    tmp.saveToPng(path);

    updateAnisotropyTextureMap();
}
