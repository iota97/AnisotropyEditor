#include "optimizer.h"
#include <QCoreApplication>
#include <QDateTime>
#include <fstream>
#include <iostream>

Optimizer::Optimizer(VulkanWindow *window, Anisotropy *anisotropy) : m_window(window), m_anisotropy(anisotropy) {
    VkDevice dev = m_window->device();
    m_devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    m_buffer[0] = new Texture(anisotropy->getDir()->getWidth(), anisotropy->getDir()->getHeight(), m_window, 1, VK_FORMAT_R16G16B16A16_SFLOAT , true, false, true);
    m_buffer[1] = new Texture(anisotropy->getDir()->getWidth(), anisotropy->getDir()->getHeight(), m_window, 1, VK_FORMAT_R16G16B16A16_SFLOAT , true);
    m_depth = new Texture(m_anisotropy->getDir()->getWidth(), m_anisotropy->getDir()->getHeight(), m_window, 1, VK_FORMAT_D32_SFLOAT, false, true, false, true);
    m_frameImage = new Texture(m_anisotropy->getDir()->getWidth(), m_anisotropy->getDir()->getHeight(), m_window, 1, VK_FORMAT_R16G16B16A16_SFLOAT , true, true, true);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_window->getComputeQueueFamilyIndex();
    if (m_devFuncs->vkCreateCommandPool(dev, &poolInfo, nullptr, &m_computeCommandPool) != VK_SUCCESS) {
        m_window->crash("failed to create compute command pool!");
    }

    createComputeUniformBuffer();
    createComputeDescriptorSet();
    createComputePipelineLayout();
    createPipelineLayout();
    createRenderPass();
    createLinePipeline();
    createOptimizeSmoothPipeline();
    createOptimizeProlongPipeline();
    createOptimizeRestrictPipeline();
    createOptimizeFinalizePipeline();
    createOptimizeInitPipeline();
    createMeshData();

    std::array<VkImageView, 2> attachments = {
        m_frameImage->getImageView(),
        m_depth->getImageView()
    };

    VkFramebufferCreateInfo framebufferInfo {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_renderPass;
    framebufferInfo.attachmentCount = attachments.size();
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = m_frameImage->getWidth();
    framebufferInfo.height =  m_frameImage->getHeight();
    framebufferInfo.layers = 1;

    m_devFuncs->vkCreateFramebuffer(m_window->device(), &framebufferInfo, NULL, &m_frameBuffer);

    m_listWidget = m_window->getConstraintWidgetList();
    commitChange();
}

Optimizer::~Optimizer() {
    VkDevice dev = m_window->device();

    delete m_buffer[0];
    delete m_buffer[1];
    delete m_depth;
    delete m_frameImage;
    delete m_directionBackup;

    if (m_computeCommandPool) {
        m_devFuncs->vkDestroyCommandPool(dev, m_computeCommandPool, nullptr);
    }

    if (m_optimizeSmoothPipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_optimizeSmoothPipeline, nullptr);
    }

    if (m_optimizeFinalizePipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_optimizeFinalizePipeline, nullptr);
    }

    if (m_optimizeProlongPipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_optimizeProlongPipeline, nullptr);
    }

    if (m_optimizeInitPipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_optimizeInitPipeline, nullptr);
    }

    if (m_optimizeRestrictPipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_optimizeRestrictPipeline, nullptr);
    }

    if (m_linePipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_linePipeline, nullptr);
    }

    if (m_pipelineCache) {
        m_devFuncs->vkDestroyPipelineCache(dev, m_pipelineCache, nullptr);
    }

    if (m_computePipelineLayout) {
        m_devFuncs->vkDestroyPipelineLayout(dev, m_computePipelineLayout, nullptr);
    }

    if (m_pipelineLayout) {
        m_devFuncs->vkDestroyPipelineLayout(dev, m_pipelineLayout, nullptr);
    }

    if (m_computeDescSetLayout[0]) {
        m_devFuncs->vkDestroyDescriptorSetLayout(dev, m_computeDescSetLayout[0], nullptr);
    }

    if (m_computeDescSetLayout[1]) {
        m_devFuncs->vkDestroyDescriptorSetLayout(dev, m_computeDescSetLayout[1], nullptr);
    }

    if (m_computeDescPool) {
        m_devFuncs->vkDestroyDescriptorPool(dev, m_computeDescPool, nullptr);
    }

    if (m_computeUniformBuf) {
        m_devFuncs->vkDestroyBuffer(dev, m_computeUniformBuf, nullptr);
    }

    if (m_computeUniformBufMem) {
        m_devFuncs->vkFreeMemory(dev, m_computeUniformBufMem, nullptr);
    }

    if (m_renderPass) {
        m_devFuncs->vkDestroyRenderPass(dev, m_renderPass, nullptr);
    }

    if (m_frameBuffer) {
        m_devFuncs->vkDestroyFramebuffer(dev, m_frameBuffer, nullptr);
    }

    if (m_meshBuf) {
        m_devFuncs->vkDestroyBuffer(dev, m_meshBuf, nullptr);
    }

    if (m_meshBufMem) {
        m_devFuncs->vkFreeMemory(dev, m_meshBufMem, nullptr);
    }
}

void Optimizer::createComputeDescriptorSet() {
    VkDevice dev = m_window->device();

    // Set up descriptor set and its layout.
    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
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
    descPoolInfo.maxSets = 2;

    VkResult err = m_devFuncs->vkCreateDescriptorPool(dev, &descPoolInfo, nullptr, &m_computeDescPool);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create descriptor pool");

    VkDescriptorSetLayoutBinding uniformBinding = {
        0, // binding
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        1,
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
        nullptr
    };
    VkDescriptorSetLayoutBinding inputSamplerBinding = {
        0, // binding
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        1,
        VK_SHADER_STAGE_COMPUTE_BIT,
        nullptr
    };
    VkDescriptorSetLayoutBinding outputSamplerBinding = {
        1, // binding
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        1,
        VK_SHADER_STAGE_COMPUTE_BIT,
        nullptr
    };

    VkDescriptorSetLayoutCreateInfo descLayoutInfo0{};
    descLayoutInfo0.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descLayoutInfo0.bindingCount = 1;
    descLayoutInfo0.pBindings = &uniformBinding;
    descLayoutInfo0.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    err = m_devFuncs->vkCreateDescriptorSetLayout(dev, &descLayoutInfo0, nullptr, &m_computeDescSetLayout[0]);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create descriptor set layout");

    VkDescriptorSetLayoutCreateInfo descLayoutInfo1{};
    std::array<VkDescriptorBindingFlags, 2> flags{VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT, VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT};
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
    bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlags.pNext = nullptr;
    bindingFlags.pBindingFlags = flags.data();
    bindingFlags.bindingCount = static_cast<uint32_t>(flags.size());

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {inputSamplerBinding, outputSamplerBinding};
    descLayoutInfo1.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descLayoutInfo1.bindingCount = static_cast<uint32_t>(bindings.size());
    descLayoutInfo1.pBindings = bindings.data();
    descLayoutInfo1.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    descLayoutInfo1.pNext = &bindingFlags;

    err = m_devFuncs->vkCreateDescriptorSetLayout(dev, &descLayoutInfo1, nullptr, &m_computeDescSetLayout[1]);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create descriptor set layout");

    VkDescriptorSetAllocateInfo descSetAllocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        nullptr,
        m_computeDescPool,
        2,
        m_computeDescSetLayout
    };

    err = m_devFuncs->vkAllocateDescriptorSets(dev, &descSetAllocInfo, m_computeDescSet);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to allocate descriptor set");

    std::array<VkDescriptorImageInfo, 2> imageInfo{};
    imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo[0].imageView = m_buffer[0]->getImageView();
    imageInfo[0].sampler = m_buffer[0]->getTextureSampler();
    imageInfo[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo[1].imageView = m_buffer[1]->getImageView();
    imageInfo[1].sampler = m_buffer[1]->getTextureSampler();

    std::array<VkWriteDescriptorSet, 3> descWrites{};
    descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[0].dstSet = m_computeDescSet[0];
    descWrites[0].dstBinding = 0;
    descWrites[0].dstArrayElement = 0;
    descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    descWrites[0].descriptorCount = 1;
    descWrites[0].pBufferInfo = &m_computeUniformBufInfo;

    descWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[1].dstSet = m_computeDescSet[1];
    descWrites[1].dstBinding = 0;
    descWrites[1].dstArrayElement = 0;
    descWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descWrites[1].descriptorCount = 1;
    descWrites[1].pImageInfo = &imageInfo[0];

    descWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[2].dstSet = m_computeDescSet[1];
    descWrites[2].dstBinding = 1;
    descWrites[2].dstArrayElement = 0;
    descWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descWrites[2].descriptorCount = 1;
    descWrites[2].pImageInfo = &imageInfo[1];

    m_devFuncs->vkUpdateDescriptorSets(dev, static_cast<uint32_t>(descWrites.size()), descWrites.data(), 0, nullptr);
}

void Optimizer::createPipelineLayout() {
    VkDevice dev = m_window->device();

    // Pipeline cache
    VkPipelineCacheCreateInfo pipelineCacheInfo;
    memset(&pipelineCacheInfo, 0, sizeof(pipelineCacheInfo));
    pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VkResult err = m_devFuncs->vkCreatePipelineCache(dev, &pipelineCacheInfo, nullptr, &m_pipelineCache);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create pipeline cache");

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    memset(&pipelineLayoutInfo, 0, sizeof(pipelineLayoutInfo));
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 2;
    pipelineLayoutInfo.pSetLayouts = m_computeDescSetLayout;
    err = m_devFuncs->vkCreatePipelineLayout(dev, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create pipeline layout");
}

void Optimizer::createComputePipelineLayout() {
    VkDevice dev = m_window->device();

    VkPipelineLayoutCreateInfo m_computePipelineLayoutInfo{};
    m_computePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    m_computePipelineLayoutInfo.setLayoutCount = 2;
    m_computePipelineLayoutInfo.pSetLayouts = m_computeDescSetLayout;

    if (m_devFuncs->vkCreatePipelineLayout(dev, &m_computePipelineLayoutInfo, nullptr, &m_computePipelineLayout) != VK_SUCCESS) {
        m_window->crash("failed to create compute pipeline layout!");
    }
}

void Optimizer::createComputeUniformBuffer() {
    VkDevice dev = m_window->device();
    const VkPhysicalDeviceLimits *pdevLimits = &m_window->physicalDeviceProperties()->limits;
    const VkDeviceSize uniAlign = pdevLimits->minUniformBufferOffsetAlignment;

    VkBufferCreateInfo uniformBufInfo{};
    uniformBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    const VkDeviceSize uniformAllocSize = m_window->aligned(18*sizeof(float), uniAlign);
    m_dynamicAlignment = uniformAllocSize;
    uniformBufInfo.size = MAX_CONSTRAINTS * uniformAllocSize;
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

    memset(&m_computeUniformBufInfo, 0, sizeof(m_computeUniformBufInfo));
    m_computeUniformBufInfo.buffer = m_computeUniformBuf;
    m_computeUniformBufInfo.range = uniformAllocSize;
    m_devFuncs->vkUnmapMemory(dev, m_computeUniformBufMem);
}

void Optimizer::createOptimizeSmoothPipeline() {
    VkDevice dev = m_window->device();
    VkShaderModule computeShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                                "/assets/shaders/smooth_comp.spv");

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_computePipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (m_devFuncs->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_optimizeSmoothPipeline) != VK_SUCCESS) {
        m_window->crash("failed to create compute pipeline!");
    }

    m_devFuncs->vkDestroyShaderModule(dev, computeShaderModule, nullptr);
}

void Optimizer::createOptimizeRestrictPipeline() {
    VkDevice dev = m_window->device();
    VkShaderModule computeShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                                "/assets/shaders/restrict_comp.spv");

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_computePipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (m_devFuncs->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_optimizeRestrictPipeline) != VK_SUCCESS) {
        m_window->crash("failed to create compute pipeline!");
    }

    m_devFuncs->vkDestroyShaderModule(dev, computeShaderModule, nullptr);
}

void Optimizer::createLinePipeline() {
    VkDevice dev = m_window->device();

    // Shaders
    VkShaderModule vertShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                             "/assets/shaders/init_vert.spv");
    VkShaderModule fragShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                             "/assets/shaders/init_frag.spv");
    VkShaderModule controlShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                                "/assets/shaders/init_tesc.spv");
    VkShaderModule evaluationShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                                   "/assets/shaders/init_tese.spv");

    // Graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo;
    memset(&pipelineInfo, 0, sizeof(pipelineInfo));
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    VkPipelineShaderStageCreateInfo shaderStages[4] = {
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_VERTEX_BIT,
            vertShaderModule,
            "main",
            nullptr
        },
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            fragShaderModule,
            "main",
            nullptr
        },
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
            controlShaderModule,
            "main",
            nullptr
        },
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
            evaluationShaderModule,
            "main",
            nullptr
        }
    };
    pipelineInfo.stageCount = 4;
    pipelineInfo.pStages = shaderStages;

    static VkVertexInputBindingDescription vertexBindingDesc = {
        0, // binding
        4*sizeof(float),
        VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkVertexInputAttributeDescription vertexAttrDesc[] = {{0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
                                                          {1, 0, VK_FORMAT_R32G32_SFLOAT, 2*sizeof(float)}};
    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.pNext = nullptr;
    vertexInputInfo.flags = 0;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = vertexAttrDesc;
    pipelineInfo.pVertexInputState = &vertexInputInfo;

    VkPipelineInputAssemblyStateCreateInfo ia;
    memset(&ia, 0, sizeof(ia));
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    pipelineInfo.pInputAssemblyState = &ia;

    // The viewport and scissor will be set dynamically via vkCmdSetViewport/Scissor.
    // This way the pipeline does not need to be touched when resizing the window.
    VkPipelineViewportStateCreateInfo vp;
    memset(&vp, 0, sizeof(vp));
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    pipelineInfo.pViewportState = &vp;

    VkPipelineRasterizationStateCreateInfo rs;
    memset(&rs, 0, sizeof(rs));
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;
    pipelineInfo.pRasterizationState = &rs;

    VkPipelineTessellationStateCreateInfo ts{};
    ts.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    ts.patchControlPoints = 2;
    pipelineInfo.pTessellationState = &ts;

    VkPipelineMultisampleStateCreateInfo ms;
    memset(&ms, 0, sizeof(ms));
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    // Enable multisampling.
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipelineInfo.pMultisampleState = &ms;

    VkPipelineDepthStencilStateCreateInfo ds;
    memset(&ds, 0, sizeof(ds));
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineInfo.pDepthStencilState = &ds;

    VkPipelineColorBlendStateCreateInfo cb;
    memset(&cb, 0, sizeof(cb));
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

    VkPipelineColorBlendAttachmentState att{};
    att.colorWriteMask = 0xF;
    cb.attachmentCount = 1;
    cb.pAttachments = &att;
    pipelineInfo.pColorBlendState = &cb;

    VkDynamicState dynEnable[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn;
    memset(&dyn, 0, sizeof(dyn));
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = sizeof(dynEnable) / sizeof(VkDynamicState);
    dyn.pDynamicStates = dynEnable;
    pipelineInfo.pDynamicState = &dyn;

    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;

    VkResult err = m_devFuncs->vkCreateGraphicsPipelines(dev, m_pipelineCache, 1, &pipelineInfo, nullptr, &m_linePipeline);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create graphics pipeline");

    if (vertShaderModule)
        m_devFuncs->vkDestroyShaderModule(dev, vertShaderModule, nullptr);
    if (fragShaderModule)
        m_devFuncs->vkDestroyShaderModule(dev, fragShaderModule, nullptr);
    if (controlShaderModule)
        m_devFuncs->vkDestroyShaderModule(dev, controlShaderModule, nullptr);
    if (evaluationShaderModule)
        m_devFuncs->vkDestroyShaderModule(dev, evaluationShaderModule, nullptr);
}

void Optimizer::createOptimizeProlongPipeline() {
    VkDevice dev = m_window->device();
    VkShaderModule computeShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                                "/assets/shaders/prolong_comp.spv");

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_computePipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (m_devFuncs->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_optimizeProlongPipeline) != VK_SUCCESS) {
        m_window->crash("failed to create compute pipeline!");
    }

    m_devFuncs->vkDestroyShaderModule(dev, computeShaderModule, nullptr);
}

void Optimizer::createOptimizeInitPipeline() {
    VkDevice dev = m_window->device();
    VkShaderModule computeShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                                "/assets/shaders/init_comp.spv");

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_computePipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (m_devFuncs->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_optimizeInitPipeline) != VK_SUCCESS) {
        m_window->crash("failed to create compute pipeline!");
    }

    m_devFuncs->vkDestroyShaderModule(dev, computeShaderModule, nullptr);
}

void Optimizer::createOptimizeFinalizePipeline() {
    VkDevice dev = m_window->device();
    VkShaderModule computeShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                                "/assets/shaders/finalize_comp.spv");

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_computePipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (m_devFuncs->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_optimizeFinalizePipeline) != VK_SUCCESS) {
        m_window->crash("failed to create compute pipeline!");
    }

    m_devFuncs->vkDestroyShaderModule(dev, computeShaderModule, nullptr);
}

void Optimizer::optimizeInitTexture() {
    VkDevice dev = m_window->device();

    std::array<VkDescriptorImageInfo, 2> imageInfo{};
    imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo[0].imageView = m_directionBackup->getImageView();
    imageInfo[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo[1].imageView = m_buffer[0]->getImageView();

    std::array<VkWriteDescriptorSet, 2> descWrites{};

    descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[0].dstSet = m_computeDescSet[1];
    descWrites[0].dstBinding = 0;
    descWrites[0].dstArrayElement = 0;
    descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descWrites[0].descriptorCount = 1;
    descWrites[0].pImageInfo = &imageInfo[0];

    descWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[1].dstSet = m_computeDescSet[1];
    descWrites[1].dstBinding = 1;
    descWrites[1].dstArrayElement = 0;
    descWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descWrites[1].descriptorCount = 1;
    descWrites[1].pImageInfo = &imageInfo[1];

    m_devFuncs->vkUpdateDescriptorSets(dev, static_cast<uint32_t>(descWrites.size()), descWrites.data(), 0, nullptr);

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
    m_devFuncs->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_optimizeInitPipeline);
    uint32_t dynamicOffset = 0;
    m_devFuncs->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 2, m_computeDescSet, 1, &dynamicOffset);
    m_devFuncs->vkCmdDispatch(commandBuffer, ceil(m_buffer[0]->getWidth()/16.0), ceil(m_buffer[0]->getHeight()/16.0), 1);
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
}


void Optimizer::optimizeFinalize() {
    VkDevice dev = m_window->device();

    std::array<VkDescriptorImageInfo, 2> imageInfo{};
    imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo[0].imageView = m_buffer[0]->getImageView();
    imageInfo[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo[1].imageView = m_anisotropy->getDir()->getImageView();

    std::array<VkWriteDescriptorSet, 2> descWrites{};

    descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[0].dstSet = m_computeDescSet[1];
    descWrites[0].dstBinding = 0;
    descWrites[0].dstArrayElement = 0;
    descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descWrites[0].descriptorCount = 1;
    descWrites[0].pImageInfo = &imageInfo[0];

    descWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[1].dstSet = m_computeDescSet[1];
    descWrites[1].dstBinding = 1;
    descWrites[1].dstArrayElement = 0;
    descWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descWrites[1].descriptorCount = 1;
    descWrites[1].pImageInfo = &imageInfo[1];

    m_devFuncs->vkUpdateDescriptorSets(dev, static_cast<uint32_t>(descWrites.size()), descWrites.data(), 0, nullptr);

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
    m_devFuncs->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_optimizeFinalizePipeline);
    uint32_t dynamicOffset = 0;
    m_devFuncs->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 2, m_computeDescSet, 1, &dynamicOffset);
    m_devFuncs->vkCmdDispatch(commandBuffer, ceil(m_buffer[0]->getWidth()/16.0), ceil(m_buffer[0]->getHeight()/16.0), 1);
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

    m_anisotropy->getDir()->generateMipmaps();
    m_anisotropy->updateAnisotropyTextureMap();
}

void Optimizer::optimizeSmooth(uint32_t targetLod, uint32_t iterations) {
    VkDevice dev = m_window->device();

    std::array<VkImageView, 2> imageView;
    std::array<VkImageViewCreateInfo, 2> viewInfo{};
    viewInfo[0].sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo[0].image = m_buffer[0]->getImage();
    viewInfo[0].viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo[0].subresourceRange.baseMipLevel = targetLod;
    viewInfo[0].subresourceRange.levelCount = 1;
    viewInfo[0].subresourceRange.baseArrayLayer = 0;
    viewInfo[0].subresourceRange.layerCount = 1;

    viewInfo[1].sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo[1].image = m_buffer[1]->getImage();
    viewInfo[1].viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo[1].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo[1].subresourceRange.baseMipLevel = targetLod;
    viewInfo[1].subresourceRange.levelCount = 1;
    viewInfo[1].subresourceRange.baseArrayLayer = 0;
    viewInfo[1].subresourceRange.layerCount = 1;

    if (m_devFuncs->vkCreateImageView(dev, &viewInfo[0], nullptr, &imageView[0]) != VK_SUCCESS) {
        m_window->crash("failed to create texture image view!");
    }

    if (m_devFuncs->vkCreateImageView(dev, &viewInfo[1], nullptr, &imageView[1]) != VK_SUCCESS) {
        m_window->crash("failed to create texture image view!");
    }

    std::array<VkDescriptorImageInfo, 2> imageInfo{};
    imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo[0].imageView = imageView[0];
    imageInfo[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo[1].imageView = imageView[1];

    std::array<VkWriteDescriptorSet, 2> descWrites{};

    descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[0].dstSet = m_computeDescSet[1];
    descWrites[0].dstBinding = 0;
    descWrites[0].dstArrayElement = 0;
    descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descWrites[0].descriptorCount = 1;
    descWrites[0].pImageInfo = &imageInfo[0];

    descWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[1].dstSet = m_computeDescSet[1];
    descWrites[1].dstBinding = 1;
    descWrites[1].dstArrayElement = 0;
    descWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descWrites[1].descriptorCount = 1;
    descWrites[1].pImageInfo = &imageInfo[1];

    m_devFuncs->vkUpdateDescriptorSets(dev, static_cast<uint32_t>(descWrites.size()), descWrites.data(), 0, nullptr);

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
    m_devFuncs->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_optimizeSmoothPipeline);
    float *p;
    VkResult err = m_devFuncs->vkMapMemory(dev, m_computeUniformBufMem, 0, MAX_CONSTRAINTS*m_dynamicAlignment, 0, reinterpret_cast<void **>(&p));
    if (err != VK_SUCCESS)
        m_window->crash("Failed to map memory");

    for (uint32_t i = 0; i < iterations; i++) {
        float *curr = p + m_dynamicAlignment/4 * i;
        curr[6] = i % 2;
        curr[7] = m_optimizationMethod;
        curr[8] = m_anisotropy->getDir()->getHeight()/4.;
        curr[9] = m_angleOffset;
        curr[10] = m_newPhaseMethod;

        uint32_t dynamicOffset = i * static_cast<uint32_t>(m_dynamicAlignment);
        m_devFuncs->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 2, m_computeDescSet, 1, &dynamicOffset);
        m_devFuncs->vkCmdDispatch(commandBuffer, ceil((m_buffer[0]->getWidth() >> targetLod)/16.0), ceil((m_buffer[0]->getHeight() >> targetLod)/16.0), 1);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_buffer[i%2]->getImage();
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = targetLod;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        m_devFuncs->vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        barrier.image = m_buffer[(i+1)%2]->getImage();
        m_devFuncs->vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    m_devFuncs->vkUnmapMemory(dev, m_computeUniformBufMem);
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

    m_devFuncs->vkDestroyImageView(dev, imageView[0], nullptr);
    m_devFuncs->vkDestroyImageView(dev, imageView[1], nullptr);
}

void Optimizer::optimizeRestrict(uint32_t destLod) {
    VkDevice dev = m_window->device();

    std::array<VkImageView, 2> imageView;
    std::array<VkImageViewCreateInfo, 2> viewInfo{};
    viewInfo[0].sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo[0].image = m_buffer[0]->getImage();
    viewInfo[0].viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo[0].subresourceRange.baseMipLevel = destLod-1;
    viewInfo[0].subresourceRange.levelCount = 1;
    viewInfo[0].subresourceRange.baseArrayLayer = 0;
    viewInfo[0].subresourceRange.layerCount = 1;

    viewInfo[1].sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo[1].image = m_buffer[0]->getImage();
    viewInfo[1].viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo[1].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo[1].subresourceRange.baseMipLevel = destLod;
    viewInfo[1].subresourceRange.levelCount = 1;
    viewInfo[1].subresourceRange.baseArrayLayer = 0;
    viewInfo[1].subresourceRange.layerCount = 1;

    if (m_devFuncs->vkCreateImageView(dev, &viewInfo[0], nullptr, &imageView[0]) != VK_SUCCESS) {
        m_window->crash("failed to create texture image view!");
    }

    if (m_devFuncs->vkCreateImageView(dev, &viewInfo[1], nullptr, &imageView[1]) != VK_SUCCESS) {
        m_window->crash("failed to create texture image view!");
    }

    std::array<VkDescriptorImageInfo, 2> imageInfo{};
    imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo[0].imageView = imageView[0];
    imageInfo[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo[1].imageView = imageView[1];

    std::array<VkWriteDescriptorSet, 2> descWrites{};

    descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[0].dstSet = m_computeDescSet[1];
    descWrites[0].dstBinding = 0;
    descWrites[0].dstArrayElement = 0;
    descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descWrites[0].descriptorCount = 1;
    descWrites[0].pImageInfo = &imageInfo[0];

    descWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[1].dstSet = m_computeDescSet[1];
    descWrites[1].dstBinding = 1;
    descWrites[1].dstArrayElement = 0;
    descWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descWrites[1].descriptorCount = 1;
    descWrites[1].pImageInfo = &imageInfo[1];

    m_devFuncs->vkUpdateDescriptorSets(dev, static_cast<uint32_t>(descWrites.size()), descWrites.data(), 0, nullptr);

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
    m_devFuncs->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_optimizeRestrictPipeline);
    uint32_t dynamicOffset = 0;
    m_devFuncs->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 2, m_computeDescSet, 1, &dynamicOffset);

    float *p;
    VkResult err = m_devFuncs->vkMapMemory(dev, m_computeUniformBufMem, 0, MAX_CONSTRAINTS*m_dynamicAlignment, 0, reinterpret_cast<void **>(&p));
    if (err != VK_SUCCESS)
        m_window->crash("Failed to map memory");
    p[7] = m_optimizationMethod;
    m_devFuncs->vkUnmapMemory(dev, m_computeUniformBufMem);

    m_devFuncs->vkCmdDispatch(commandBuffer, ceil((m_buffer[0]->getWidth() >> destLod)/16.0), ceil((m_buffer[0]->getHeight() >> destLod)/16.0), 1);
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

    m_devFuncs->vkDestroyImageView(dev, imageView[0], nullptr);
    m_devFuncs->vkDestroyImageView(dev, imageView[1], nullptr);
}

void Optimizer::optimizeProlong(uint32_t destLod) {
    VkDevice dev = m_window->device();

    std::array<VkImageView, 2> imageView;
    std::array<VkImageViewCreateInfo, 2> viewInfo{};
    viewInfo[0].sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo[0].image = m_buffer[0]->getImage();
    viewInfo[0].viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo[0].subresourceRange.baseMipLevel = destLod+1;
    viewInfo[0].subresourceRange.levelCount = 1;
    viewInfo[0].subresourceRange.baseArrayLayer = 0;
    viewInfo[0].subresourceRange.layerCount = 1;

    viewInfo[1].sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo[1].image = m_buffer[0]->getImage();
    viewInfo[1].viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo[1].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo[1].subresourceRange.baseMipLevel = destLod;
    viewInfo[1].subresourceRange.levelCount = 1;
    viewInfo[1].subresourceRange.baseArrayLayer = 0;
    viewInfo[1].subresourceRange.layerCount = 1;

    if (m_devFuncs->vkCreateImageView(dev, &viewInfo[0], nullptr, &imageView[0]) != VK_SUCCESS) {
        m_window->crash("failed to create texture image view!");
    }

    if (m_devFuncs->vkCreateImageView(dev, &viewInfo[1], nullptr, &imageView[1]) != VK_SUCCESS) {
        m_window->crash("failed to create texture image view!");
    }

    std::array<VkDescriptorImageInfo, 2> imageInfo{};
    imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo[0].imageView = imageView[0];
    imageInfo[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo[1].imageView = imageView[1];

    std::array<VkWriteDescriptorSet, 2> descWrites{};

    descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[0].dstSet = m_computeDescSet[1];
    descWrites[0].dstBinding = 0;
    descWrites[0].dstArrayElement = 0;
    descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descWrites[0].descriptorCount = 1;
    descWrites[0].pImageInfo = &imageInfo[0];

    descWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[1].dstSet = m_computeDescSet[1];
    descWrites[1].dstBinding = 1;
    descWrites[1].dstArrayElement = 0;
    descWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descWrites[1].descriptorCount = 1;
    descWrites[1].pImageInfo = &imageInfo[1];

    m_devFuncs->vkUpdateDescriptorSets(dev, static_cast<uint32_t>(descWrites.size()), descWrites.data(), 0, nullptr);

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
    m_devFuncs->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_optimizeProlongPipeline);
    uint32_t dynamicOffset = 0;
    m_devFuncs->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 2, m_computeDescSet, 1, &dynamicOffset);
    m_devFuncs->vkCmdDispatch(commandBuffer, ceil((m_buffer[0]->getWidth() >> destLod)/16.0), ceil((m_buffer[0]->getHeight() >> destLod)/16.0), 1);
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

    m_devFuncs->vkDestroyImageView(dev, imageView[0], nullptr);
    m_devFuncs->vkDestroyImageView(dev, imageView[1], nullptr);
}

void Optimizer::optimize() {
    optimize(m_iteration);
}

void Optimizer::optimize(uint32_t iteration) {
    if (m_anisotropy->getDir()->getWidth() != m_buffer[0]->getWidth() || m_anisotropy->getDir()->getHeight() != m_buffer[0]->getHeight()) {
        delete m_buffer[0];
        delete m_buffer[1];
        delete m_depth;
        delete m_frameImage;
        m_buffer[0] = new Texture(m_anisotropy->getDir()->getWidth(), m_anisotropy->getDir()->getHeight(), m_window, 1, VK_FORMAT_R16G16B16A16_SFLOAT , true);
        m_buffer[1] = new Texture(m_anisotropy->getDir()->getWidth(), m_anisotropy->getDir()->getHeight(), m_window, 1, VK_FORMAT_R16G16B16A16_SFLOAT , true);
        m_frameImage = new Texture(m_anisotropy->getDir()->getWidth(), m_anisotropy->getDir()->getHeight(), m_window, 1, VK_FORMAT_R16G16B16A16_SFLOAT , true, true, true);
        m_depth = new Texture(m_anisotropy->getDir()->getWidth(), m_anisotropy->getDir()->getHeight(), m_window, 1, VK_FORMAT_D32_SFLOAT, false, true, false, true);

        m_devFuncs->vkDestroyFramebuffer(m_window->device(), m_frameBuffer, nullptr);
        std::array<VkImageView, 2> attachments = {
            m_frameImage->getImageView(),
            m_depth->getImageView()
        };

        VkFramebufferCreateInfo framebufferInfo {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = attachments.size();
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_frameImage->getWidth();
        framebufferInfo.height =  m_frameImage->getHeight();
        framebufferInfo.layers = 1;

        m_devFuncs->vkCreateFramebuffer(m_window->device(), &framebufferInfo, NULL, &m_frameBuffer);
    }

    if (m_constraints.empty() && m_directionBackup) {
        optimizeInitTexture();
    } else {
        optimizeInit();
    }
    const int N = m_buffer[0]->getMipLevels()-1;
    for (int i = 0; i < N; i++) {
        optimizeRestrict(i+1);
    }

    for (int i = N; i > 0; i--) {
        optimizeSmooth(i, iteration);
        optimizeProlong(i-1);
    }
    optimizeSmooth(0, iteration);
    optimizeFinalize();
}

void Optimizer::createRenderPass() {
    VkDevice dev = m_window->device();

    // Renderpass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency depth_dependency = {};
    depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    depth_dependency.dstSubpass = 0;
    depth_dependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depth_dependency.srcAccessMask = 0;
    depth_dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depth_dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency dependencies[2] = { dependency, depth_dependency };

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 2;
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = dependencies;

    if (m_devFuncs->vkCreateRenderPass(dev, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        m_window->crash("failed to create render pass!");
    }
}

void Optimizer::save(const QString &path) {
#ifdef _WIN32
    std::ofstream fs(path.toStdWString().c_str(), std::ios::binary);
#else
    std::ofstream fs(path.toStdString().c_str(), std::ios::binary);
#endif
    uint32_t val = 0;
    fs.write(reinterpret_cast<const char*>(&val), sizeof(val));
    val = m_anisotropy->getDir()->getWidth();
    fs.write(reinterpret_cast<const char*>(&val), sizeof(val));
    val = m_anisotropy->getDir()->getHeight();
    fs.write(reinterpret_cast<const char*>(&val), sizeof(val));
    val = m_constraints.size();
    fs.write(reinterpret_cast<const char*>(&val), sizeof(val));
    for (auto &rect : m_constraints) {
        fs.write(reinterpret_cast<const char*>(&rect), offsetof(Constraint, lines));
        uint32_t lineCount = rect.lines.size();
        fs.write(reinterpret_cast<const char*>(&lineCount), sizeof(lineCount));
        for (auto &line : rect.lines) {
            fs.write(reinterpret_cast<const char*>(&line), sizeof(line));
        }
    }
    fs.close();
}

void Optimizer::load(const QString &path) {
#if defined(__GNUC__) && !defined(__INTEL_COMPILER) && (((__GNUC__ * 100) + __GNUC_MINOR__) >= 800)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
#ifdef _WIN32
    std::ifstream fs(path.toStdWString().c_str(), std::ios::binary);
#else
    std::ifstream fs(path.toStdString().c_str(), std::ios::binary);
#endif
    std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(fs), {});

    uint32_t width = ((uint32_t *)buffer.data())[1];
    uint32_t height = ((uint32_t *)buffer.data())[2];
    m_window->getRender()->newAnisoDirTexture(width, height);

    m_changesQueue.clear();
    m_currChange = 0;
    m_constraints.clear();
    m_listWidget->clear();

    uint32_t constraintCount = ((uint32_t *)buffer.data())[3];
    char *curr = (char*)(buffer.data()+4*sizeof(uint32_t));
    for (size_t i = 0; i < constraintCount; i ++) {
        Constraint constraint{};
        memcpy(&constraint, curr, offsetof(Constraint, lines));
        curr += offsetof(Constraint, lines);
        uint32_t lineCount = *(uint32_t*)(curr);
        curr += sizeof(uint32_t);
        for (uint32_t j = 0; j < lineCount; j++) {
            Constraint::Line line{};
            memcpy(&line, curr, sizeof(line));
            curr += sizeof(line);
            constraint.lines.push_back(line);
        }
        m_maxId = std::max(m_maxId, constraint.id);
        m_constraints.push_back(constraint);
        new QListWidgetItem((lineCount ? "Line " : constraint.tesselationLod == 1 ? "Rectangle " : "Ellipse ")+QString::number(constraint.id), m_listWidget);
    }
#if defined(__GNUC__) && !defined(__INTEL_COMPILER) && (((__GNUC__ * 100) + __GNUC_MINOR__) >= 800)
#pragma GCC diagnostic pop
#endif
    optimize(m_iteration);
    commitChange();
 }

void Optimizer::addRectangle(float x0, float y0, float x1, float y1) {
    if (abs(x0-x1) < 0.001 && abs(y0-y1) < 0.005)
        return;

    m_constraints.push_back({(x0+x1)/2, 1.0f-(y0+y1)/2, abs(x0-x1), abs(y0-y1), 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, ++m_maxId, std::vector<Constraint::Line>()});
    new QListWidgetItem("Rectangle "+QString::number(m_maxId), m_listWidget);
    optimize(m_iteration);
    commitChange();
}

void Optimizer::addEllipse(float x0, float y0, float x1, float y1) {
    if (abs(x0-x1) < 0.001 && abs(y0-y1) < 0.005)
        return;

    m_constraints.push_back({(x0+x1)/2, 1.0f-(y0+y1)/2, abs(x0-x1), abs(y0-y1), 1, 0, 0, 0, 0, 0, 0, 1, 1, 32, 1, ++m_maxId, std::vector<Constraint::Line>()});
    new QListWidgetItem("Ellipse "+QString::number(m_maxId), m_listWidget);
    optimize(m_iteration);
    commitChange();
}


void Optimizer::addLine(const std::vector<std::array<float, 2>> &pointList) {
    std::vector<Constraint::Line> lines;
    float x0 = pointList[0][0], x1 = pointList[0][0], y0 = pointList[0][1], y1 = pointList[0][1];
    for (uint32_t i = 0; i < pointList.size()-1; i++) {
        lines.push_back({pointList[i][0], pointList[i][1], pointList[i+1][0], pointList[i+1][1]});
        x0 = std::min(x0, pointList[i+1][0]);
        x1 = std::max(x1, pointList[i+1][0]);
        y0 = std::min(y0, pointList[i+1][1]);
        y1 = std::max(y1, pointList[i+1][1]);
    }

    m_constraints.push_back({(x0+x1)/2, 1.0f-(y0+y1)/2, 1, 1, 1, 0, 0, 0, 0, (x0+x1)/2, 1.0f-(y0+y1)/2, abs(x0-x1), abs(y0-y1), 1, 0, ++m_maxId, std::move(lines)});
    new QListWidgetItem("Line "+QString::number(m_maxId), m_listWidget);
    optimize(m_iteration);
    commitChange();
}

void Optimizer::setAngleOffset(float val) {
    m_angleOffset = val;
    optimize();
}

void Optimizer::clear() {
    m_changesQueue.clear();
    m_currChange = 0;
    m_constraints.clear();
    m_listWidget->clear();
    m_maxId = 0;

    optimize(m_iteration);
    commitChange();
}

void Optimizer::bakeLine(Constraint &rect) {
    for (uint32_t i = 0; i < rect.lines.size(); i++) {
        float x = rect.lines[i].x1 - rect.lineCenterX;
        float y = 1.0-rect.lines[i].y1 - rect.lineCenterY;

        x = x*rect.width;
        y = y*rect.height;
        float tmp = x;
        x = x - rect.skewH*y;
        y = (1.0f+rect.skewH*rect.skewV)*y-rect.skewV*tmp;
        tmp = x;
        x = cosf(rect.rotAngle)*x + sinf(rect.rotAngle)*y;
        y = -sinf(rect.rotAngle)*tmp + cosf(rect.rotAngle)*y;
        x += rect.centerX;
        y += rect.centerY;
        rect.lines[i].x1 = x;
        rect.lines[i].y1 = 1.0-y;

        if (i > 0) {
            rect.lines[i].x0 = rect.lines[i-1].x1;
            rect.lines[i].y0 = rect.lines[i-1].y1;
        } else {
            float x = rect.lines[i].x0 - rect.lineCenterX;
            float y = 1.0-rect.lines[i].y0 - rect.lineCenterY;

            x = x*rect.width;
            y = y*rect.height;
            float tmp = x;
            x = x - rect.skewH*y;
            y = (1.0f+rect.skewH*rect.skewV)*y-rect.skewV*tmp;
            tmp = x;
            x = cosf(rect.rotAngle)*x + sinf(rect.rotAngle)*y;
            y = -sinf(rect.rotAngle)*tmp + cosf(rect.rotAngle)*y;
            x += rect.centerX;
            y += rect.centerY;
            rect.lines[i].x0 = x;
            rect.lines[i].y0 = 1.0-y;
        }
    }
}

void Optimizer::recreateLineAABB(Constraint &rect) {
    auto &points = rect.lines;
    float x0 = points[0].x0, x1 = points[0].x0, y0 = points[0].y0, y1 = points[0].y0;
    for (uint32_t i = 0; i < points.size(); i++) {
        x0 = std::min(x0, points[i].x1);
        x1 = std::max(x1, points[i].x1);
        y0 = std::min(y0, points[i].y1);
        y1 = std::max(y1, points[i].y1);
    }

    rect.centerX = (x0+x1)/2;
    rect.centerY = 1.0f-(y0+y1)/2;
    rect.width = 1.0f;
    rect.height = 1.0f;
    rect.rotAngle = 0.0f;
    rect.lineCenterX = (x0+x1)/2;
    rect.lineCenterY = 1.0f-(y0+y1)/2;
    rect.lineWidth = abs(x0-x1);
    rect.lineHeight = abs(y0-y1);
    rect.skewH = 0;
    rect.skewV = 0;
}

void Optimizer::movePoint(uint32_t id, uint32_t offset, float x, float y) {
    auto &rect = m_constraints[id];
    bakeLine(rect);

    if (offset == rect.lines.size()) {
        rect.lines[offset-1].x1 = x;
        rect.lines[offset-1].y1 = y;
    } else {
        rect.lines[offset].x0 = x;
        rect.lines[offset].y0 = y;
        if (offset > 0) {
            rect.lines[offset-1].x1 = x;
            rect.lines[offset-1].y1 = y;
        }
    }

    recreateLineAABB(rect);

    if (m_window->getOptimizeOnMove()) {
        optimize(m_iterationOnMove);
    }
}

void Optimizer::addPoint(uint32_t id, uint32_t offset, float x, float y) {
    auto &rect = m_constraints[id];
    bakeLine(rect);

    Constraint::Line line{x, y, rect.lines[offset].x1, rect.lines[offset].y1};
    rect.lines[offset].x1 = x;
    rect.lines[offset].y1 = y;
    rect.lines.insert(rect.lines.begin()+offset+1, line);

    recreateLineAABB(rect);
    commitChange();
}


void Optimizer::duplicateRectangle(uint32_t id) {
    m_constraints.push_back(m_constraints[id]);
    new QListWidgetItem((m_constraints[id].tesselationLod > 1 ? "Ellipse " : m_constraints[id].lines.empty() ? "Rectangle " : "Line ")+QString::number(++m_maxId), m_listWidget);
    commitChange();
}

void Optimizer::rotateRectangle(uint32_t id, float angle, bool opt) {
    if (fabs(m_constraints[id].rotAngle-fmod(angle+M_PI, 2*M_PI)+M_PI) > 0.001) {
        m_constraints[id].rotAngle = angle - 2*M_PI * floor((angle+M_PI)/2/M_PI);
        if (opt) {
            optimize(m_iteration);
            commitChange();
        } else if (m_window->getOptimizeOnMove()) {
            optimize(m_iterationOnMove);
        }
    }
}

void Optimizer::setAlignPhase(uint32_t id, bool val) {
    if (m_constraints[id].alignPhases != val) {
        m_constraints[id].alignPhases = val;
        optimize(m_iteration);
        commitChange();
    }
}

void Optimizer::moveRectangleX(uint32_t id, float x, bool opt) {
    if (fabs(m_constraints[id].centerX - x) > 0.001) {
        m_constraints[id].centerX = x;
        if (opt) {
            optimize(m_iteration);
            commitChange();
        } else if (m_window->getOptimizeOnMove()) {
            optimize(m_iterationOnMove);
        }
    }
}

void Optimizer::moveRectangleY(uint32_t id, float y, bool opt) {
    if (fabs(m_constraints[id].centerY - y) > 0.001) {
        m_constraints[id].centerY = y;
        if (opt) {
            optimize(m_iteration);
            commitChange();
        } else if (m_window->getOptimizeOnMove()) {
            optimize(m_iterationOnMove);
        }
    }
}

void Optimizer::scaleRectangleX(uint32_t id, float x, bool opt) {
    if (fabs(m_constraints[id].width - x) > 0.001) {
        m_constraints[id].width = x;
        if (opt) {
            optimize(m_iteration);
            commitChange();
        } else if (m_window->getOptimizeOnMove()) {
            optimize(m_iterationOnMove);
        }
    }
}

void Optimizer::scaleRectangleY(uint32_t id, float y, bool opt) {
    if (fabs(m_constraints[id].height - y) > 0.001) {
        m_constraints[id].height = y;
        if (opt) {
            optimize(m_iteration);
            commitChange();
        } else if (m_window->getOptimizeOnMove()) {
            optimize(m_iterationOnMove);
        }
    }
}

void Optimizer::skewRectangleX(uint32_t id, float x) {
    if (fabs(m_constraints[id].skewH - x) > 0.001) {
        m_constraints[id].skewH = x;
        optimize(m_iteration);
        commitChange();
    }
}

void Optimizer::skewRectangleY(uint32_t id, float y) {
    if (fabs(m_constraints[id].skewV - y) > 0.001) {
        m_constraints[id].skewV = y;
        optimize(m_iteration);
        commitChange();
    }
}

void Optimizer::fillRectangle(uint32_t id, float angle, bool opt) {
    if (fabs(m_constraints[id].dirX - cos(angle)) > 0.001 && fabs(m_constraints[id].dirY - cos(angle)) > 0.001) {
        m_constraints[id].dirX = cos(angle);
        m_constraints[id].dirY = sin(angle);
        if (opt) {
            optimize(m_iteration);
            commitChange();
        } else if (m_window->getOptimizeOnMove()) {
            optimize(m_iterationOnMove);
        }
    }
}

void Optimizer::removeRectangle(uint32_t id) {
    if (!m_constraints.empty()) {
        m_listWidget->removeItemWidget(m_listWidget->takeItem(id));
        if (m_maxId == m_constraints[id].id) {
            m_maxId--;
        }
        m_constraints.erase(m_constraints.begin()+id);
        optimize(m_iteration);
        commitChange();
    }
}

const std::vector<Optimizer::Constraint>& Optimizer::getConstraints() {
    return m_constraints;
}

void Optimizer::optimizeInit() {
    VkDevice dev = m_window->device();
    m_frameImage->clear(0, 0, 0, 0);

    uint32_t drawn = 0;
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_window->graphicsCommandPool();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cb;
    m_devFuncs->vkAllocateCommandBuffers(dev, &allocInfo, &cb);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    m_devFuncs->vkBeginCommandBuffer(cb, &beginInfo);

    VkClearColorValue clearColor = {{ 0.0, 0.0, 0.0, 0.0 }};
    VkClearDepthStencilValue clearDS = { 1, 0 };
    VkClearValue clearValues[2];
    memset(clearValues, 0, sizeof(clearValues));
    clearValues[0].color = clearColor;
    clearValues[1].depthStencil = clearDS;

    VkRenderPassBeginInfo rp_begin {};
    rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.pNext = NULL;
    rp_begin.renderPass = m_renderPass;
    rp_begin.framebuffer = m_frameBuffer;
    rp_begin.renderArea.offset.x = 0;
    rp_begin.renderArea.offset.y = 0;
    rp_begin.renderArea.extent.width = m_frameImage->getWidth();
    rp_begin.renderArea.extent.height = m_frameImage->getHeight();
    rp_begin.clearValueCount = 2;
    rp_begin.pClearValues = clearValues;

    m_devFuncs->vkCmdBeginRenderPass(cb, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport;
    viewport.height = m_frameImage->getHeight();
    viewport.width = m_frameImage->getWidth();
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    viewport.x = 0;
    viewport.y = 0;
    m_devFuncs->vkCmdSetViewport(cb, 0, 1, &viewport);

    VkRect2D scissor;
    scissor.extent.width = m_frameImage->getWidth();
    scissor.extent.height = m_frameImage->getHeight();
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    m_devFuncs->vkCmdSetScissor(cb, 0, 1, &scissor);

    m_devFuncs->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_linePipeline);
    float *p;
    VkResult err = m_devFuncs->vkMapMemory(dev, m_computeUniformBufMem, 0, MAX_CONSTRAINTS*m_dynamicAlignment, 0, reinterpret_cast<void **>(&p));
    if (err != VK_SUCCESS)
        m_window->crash("Failed to map memory");

    static const VkDeviceSize zero = 0;
    m_devFuncs->vkCmdBindVertexBuffers(cb, 0, 1, &m_meshBuf, &zero);

    for (const auto &data : m_constraints) {
        if (data.lines.empty()) {
            float *curr = p + m_dynamicAlignment/4 * ++drawn;
            *curr++ = data.centerX;
            *curr++ = data.centerY;
            *curr++ = data.width;
            *curr++ = data.height;
            *curr++ = data.dirX;
            *curr++ = data.dirY;
            *curr++ = data.rotAngle;
            *curr++ = data.skewH;
            *curr++ = data.skewV;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = -1;
            *curr++ = float(m_anisotropy->getDir()->getHeight())/m_anisotropy->getDir()->getWidth();
            *curr++ = data.tesselationLod;
            *curr++ = data.alignPhases;

            uint32_t dynamicOffset = drawn * static_cast<uint32_t>(m_dynamicAlignment);
            m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 2, m_computeDescSet, 1, &dynamicOffset);
            m_devFuncs->vkCmdDraw(cb, 8, 1, 0, 0);
        } else {
            for (uint32_t j = 0; j < data.lines.size(); j++) {
                float *curr = p + m_dynamicAlignment/4 * ++drawn;
                *curr++ = data.centerX;
                *curr++ = data.centerY;
                *curr++ = data.width;
                *curr++ = data.height;
                *curr++ = data.dirX;
                *curr++ = data.dirY;
                *curr++ = data.rotAngle;
                *curr++ = data.skewH;
                *curr++ = data.skewV;

                auto line = data.lines[j];
                float lineLength = sqrt((line.x0-line.x1)*(line.x0-line.x1) + (line.y0-line.y1)*(line.y0-line.y1));
                *curr++ = (line.x0+line.x1)/2;
                *curr++ = 1.0-(line.y0+line.y1)/2;
                *curr++ = atan2(line.y0-line.y1, line.x0-line.x1);
                *curr++ = data.lineCenterX;
                *curr++ = data.lineCenterY;
                *curr++ = lineLength;
                *curr++ = float(m_anisotropy->getDir()->getHeight())/m_anisotropy->getDir()->getWidth();
                *curr++ = data.tesselationLod;
                *curr++ = data.alignPhases;

                uint32_t dynamicOffset = drawn * static_cast<uint32_t>(m_dynamicAlignment);
                m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 2, m_computeDescSet, 1, &dynamicOffset);
                m_devFuncs->vkCmdDraw(cb, 2, 1, 8, 0);
            }
        }
    }
    m_devFuncs->vkUnmapMemory(dev, m_computeUniformBufMem);

    m_devFuncs->vkCmdEndRenderPass(cb);
    m_devFuncs->vkEndCommandBuffer(cb);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cb;

    m_devFuncs->vkQueueSubmit(m_window->graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    m_devFuncs->vkQueueWaitIdle(m_window->graphicsQueue());
    m_devFuncs->vkFreeCommandBuffers(dev, m_window->graphicsCommandPool(), 1, &cb);
    m_buffer[0]->blitTextureImage(*m_frameImage);
}

void Optimizer::createMeshData() {
    VkDevice dev = m_window->device();

    float data[] = {-1.0, -1.0, 0.0, 1.0,
                    -1.0, 1.0, 0.0, 1.0,
                    -1.0, 1.0, 1.0, 0.0,
                    1.0, 1.0, 1.0, 0.0,
                    1.0, 1.0, 0.0, -1.0,
                    1.0, -1.0, 0.0, -1.0,
                    1.0, -1.0, -1.0, 0.0,
                    -1.0, -1.0, -1.0, 0.0,
                    -1.0, 0.0, 1.0, 0.0,
                    1.0, 0.0, 1.0, 0.0,
                    };

    VkBufferCreateInfo meshBufInfo;
    memset(&meshBufInfo, 0, sizeof(meshBufInfo));
    meshBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    meshBufInfo.size = sizeof(data);
    meshBufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VkResult err = m_devFuncs->vkCreateBuffer(dev, &meshBufInfo, nullptr, &m_meshBuf);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create buffer");

    VkMemoryRequirements meshMemReq;
    m_devFuncs->vkGetBufferMemoryRequirements(dev, m_meshBuf, &meshMemReq);

    VkMemoryAllocateInfo meshMemAllocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        meshMemReq.size,
        m_window->findMemoryType(meshMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    };

    err = m_devFuncs->vkAllocateMemory(dev, &meshMemAllocInfo, nullptr, &m_meshBufMem);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to allocate memory");

    err = m_devFuncs->vkBindBufferMemory(dev, m_meshBuf, m_meshBufMem, 0);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to bind buffer memory");

    void *p;
    err = m_devFuncs->vkMapMemory(dev, m_meshBufMem, 0, meshMemReq.size, 0, &p);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to map memory");

    memcpy(p, data, sizeof(data));
    m_devFuncs->vkUnmapMemory(dev, m_meshBufMem);
}

bool Optimizer::canUndo() {
    return m_currChange > 1;
}

bool Optimizer::canRedo() {
    return m_currChange < m_changesQueue.size();
}

void Optimizer::commitChange() {
    if (m_currChange != m_changesQueue.size()) {
        m_changesQueue.resize(m_currChange);
    }

    if (m_changesQueue.size() > m_maxUndoCount) {
        m_changesQueue.pop_front();
    }

    m_changesQueue.push_back(m_constraints);
    m_currChange = m_changesQueue.size();
    m_window->dirtyUndoMenu();
}

void Optimizer::revertChange() {
    if (m_currChange > 1) {
        m_constraints = m_changesQueue[m_currChange-2];
        m_currChange--;
        m_listWidget->clear();
        m_maxId = 0;
        for (auto &rect : m_constraints) {
            new QListWidgetItem((rect.tesselationLod > 1 ? "Ellipse " : rect.lines.empty() ? "Rectangle " : "Line ")+QString::number(rect.id), m_listWidget);
            m_maxId = std::max(rect.id, m_maxId);
        }
        optimize(m_iteration);
    }
    m_window->dirtyUndoMenu();
}

void Optimizer::redoChange() {
    if (m_currChange < m_changesQueue.size()) {
        m_constraints = m_changesQueue[m_currChange];
        m_currChange++;
        m_listWidget->clear();
        m_maxId = 0;
        for (auto &rect : m_constraints) {
            new QListWidgetItem((rect.tesselationLod > 1 ? "Ellipse " : rect.lines.empty() ? "Rectangle " : "Line ")+QString::number(rect.id), m_listWidget);
            m_maxId = std::max(rect.id, m_maxId);
        }
        optimize(m_iteration);
    }
    m_window->dirtyUndoMenu();
}

void Optimizer::setIteration(uint32_t val) {
    m_iteration = val;
    optimize();
}

void Optimizer::setIterationOnMove(uint32_t val) {
    m_iterationOnMove = val;
    optimize();
}

void Optimizer::setMethod(Method val) {
    m_optimizationMethod = val;
    optimize();
}

void Optimizer::setNewPhaseMethod(bool val) {
    m_newPhaseMethod = val;
    optimize();
}

void Optimizer::setDirectionTexture(Texture *tex) {
    delete m_directionBackup;
    m_directionBackup = Texture::createFromTexture(*tex);
    optimize();
}

uint32_t Optimizer::getWidth() {
    return m_anisotropy->getDir()->getWidth();
}

uint32_t Optimizer::getHeight() {
    return m_anisotropy->getDir()->getHeight();
}
