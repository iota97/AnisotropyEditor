#include "constraints.h"
#include <QCoreApplication>

ConstraintsRenderer::ConstraintsRenderer(VulkanWindow *window) : m_window(window) {
    VkDevice dev = m_window->device();
    m_devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    m_atlas = new Texture(QCoreApplication::applicationDirPath()+"/assets/textures/atlas.png", m_window);

    createRectData();
    createHandleData();
    createUniformBuffer();
    createDescriptors();
    createPipelineLayout();
    createPipeline();
    createHandlePipeline();
}

ConstraintsRenderer::~ConstraintsRenderer() {
    VkDevice dev = m_window->device();
    delete m_atlas;

    if (m_rectBuf) {
        m_devFuncs->vkDestroyBuffer(dev, m_rectBuf, nullptr);
        m_rectBuf = VK_NULL_HANDLE;
    }

    if (m_rectBufMem) {
        m_devFuncs->vkFreeMemory(dev, m_rectBufMem, nullptr);
        m_rectBufMem = VK_NULL_HANDLE;
    }

    if (m_handleBuf) {
        m_devFuncs->vkDestroyBuffer(dev, m_handleBuf, nullptr);
        m_handleBuf = VK_NULL_HANDLE;
    }

    if (m_handleBufMem) {
        m_devFuncs->vkFreeMemory(dev, m_handleBufMem, nullptr);
        m_handleBufMem = VK_NULL_HANDLE;
    }

    if (m_pipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }

    if (m_noStencilPipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_noStencilPipeline, nullptr);
        m_noStencilPipeline = VK_NULL_HANDLE;
    }

    if (m_handlePipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_handlePipeline, nullptr);
        m_handlePipeline = VK_NULL_HANDLE;
    }

    if (m_pipelineLayout) {
        m_devFuncs->vkDestroyPipelineLayout(dev, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }

    if (m_pipelineCache) {
        m_devFuncs->vkDestroyPipelineCache(dev, m_pipelineCache, nullptr);
        m_pipelineCache = VK_NULL_HANDLE;
    }

    if (m_descSetLayout) {
        m_devFuncs->vkDestroyDescriptorSetLayout(dev, m_descSetLayout, nullptr);
        m_descSetLayout = VK_NULL_HANDLE;
    }

    if (m_descPool) {
        m_devFuncs->vkDestroyDescriptorPool(dev, m_descPool, nullptr);
        m_descPool = VK_NULL_HANDLE;
    }

    if (m_uniformBuf) {
        m_devFuncs->vkDestroyBuffer(dev, m_uniformBuf, nullptr);
        m_uniformBuf = VK_NULL_HANDLE;
    }

    if (m_uniformBufMem) {
        m_devFuncs->vkFreeMemory(dev, m_uniformBufMem, nullptr);
        m_uniformBufMem = VK_NULL_HANDLE;
    }
}

void ConstraintsRenderer::createRectData() {
    VkDevice dev = m_window->device();

    float data[24] = {-0.15, 0.0, 0.15, 0.0,
                      -1.0, -1.0, -1.0, 1.0,
                      -1.0, 1.0, 1.0, 1.0,
                      1.0, 1.0, 1.0, -1.0,
                      1.0, -1.0, -1.0, -1.0,
                      -1.0, 0.0, 1.0, 0.0};

    VkBufferCreateInfo meshBufInfo;
    memset(&meshBufInfo, 0, sizeof(meshBufInfo));
    meshBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    meshBufInfo.size = sizeof(data);
    meshBufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VkResult err = m_devFuncs->vkCreateBuffer(dev, &meshBufInfo, nullptr, &m_rectBuf);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create buffer");

    VkMemoryRequirements meshMemReq;
    m_devFuncs->vkGetBufferMemoryRequirements(dev, m_rectBuf, &meshMemReq);

    VkMemoryAllocateInfo meshMemAllocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        meshMemReq.size,
        m_window->findMemoryType(meshMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    };

    err = m_devFuncs->vkAllocateMemory(dev, &meshMemAllocInfo, nullptr, &m_rectBufMem);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to allocate memory");

    err = m_devFuncs->vkBindBufferMemory(dev, m_rectBuf, m_rectBufMem, 0);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to bind buffer memory");

    void *p;
    err = m_devFuncs->vkMapMemory(dev, m_rectBufMem, 0, meshMemReq.size, 0, &p);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to map memory");

    memcpy(p, data, sizeof(data));
    m_devFuncs->vkUnmapMemory(dev, m_rectBufMem);
}

void ConstraintsRenderer::createHandleData() {
    VkDevice dev = m_window->device();

    float data[8] = {-1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, -1.0};

    VkBufferCreateInfo meshBufInfo;
    memset(&meshBufInfo, 0, sizeof(meshBufInfo));
    meshBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    meshBufInfo.size = sizeof(data);
    meshBufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VkResult err = m_devFuncs->vkCreateBuffer(dev, &meshBufInfo, nullptr, &m_handleBuf);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create buffer");

    VkMemoryRequirements meshMemReq;
    m_devFuncs->vkGetBufferMemoryRequirements(dev, m_handleBuf, &meshMemReq);

    VkMemoryAllocateInfo meshMemAllocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        meshMemReq.size,
        m_window->findMemoryType(meshMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    };

    err = m_devFuncs->vkAllocateMemory(dev, &meshMemAllocInfo, nullptr, &m_handleBufMem);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to allocate memory");

    err = m_devFuncs->vkBindBufferMemory(dev, m_handleBuf, m_handleBufMem, 0);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to bind buffer memory");

    void *p;
    err = m_devFuncs->vkMapMemory(dev, m_handleBufMem, 0, meshMemReq.size, 0, &p);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to map memory");

    memcpy(p, data, sizeof(data));
    m_devFuncs->vkUnmapMemory(dev, m_handleBufMem);
}

void ConstraintsRenderer::createPipeline() {
    VkDevice dev = m_window->device();

    // Shaders
    VkShaderModule vertShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                             "/assets/shaders/constraint_vert.spv");
    VkShaderModule fragShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                             "/assets/shaders/constraint_frag.spv");
    VkShaderModule controlShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                             "/assets/shaders/constraint_tesc.spv");
    VkShaderModule evaluationShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                             "/assets/shaders/constraint_tese.spv");
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
        2*sizeof(float),
        VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkVertexInputAttributeDescription vertexAttrDesc[] = {{0, 0, VK_FORMAT_R32G32_SFLOAT, 0}};
    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.pNext = nullptr;
    vertexInputInfo.flags = 0;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;
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

    VkPipelineTessellationStateCreateInfo ts{};
    ts.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    ts.patchControlPoints = 2;
    pipelineInfo.pTessellationState = &ts;

    VkPipelineRasterizationStateCreateInfo rs;
    memset(&rs, 0, sizeof(rs));
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 2.0f;
    pipelineInfo.pRasterizationState = &rs;

    VkPipelineMultisampleStateCreateInfo ms;
    memset(&ms, 0, sizeof(ms));
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    // Enable multisampling.
    ms.rasterizationSamples = m_window->sampleCountFlagBits();
    pipelineInfo.pMultisampleState = &ms;

    VkPipelineDepthStencilStateCreateInfo ds;
    memset(&ds, 0, sizeof(ds));
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    ds.stencilTestEnable = VK_TRUE;
    ds.back.compareOp = VK_COMPARE_OP_EQUAL;
    ds.back.failOp = VK_STENCIL_OP_KEEP;
    ds.back.depthFailOp = VK_STENCIL_OP_KEEP;
    ds.back.passOp = VK_STENCIL_OP_KEEP;
    ds.back.compareMask = 0xff;
    ds.back.writeMask = 0xff;
    ds.back.reference = 1;
    ds.front = ds.back;
    pipelineInfo.pDepthStencilState = &ds;

    VkPipelineColorBlendStateCreateInfo cb;
    memset(&cb, 0, sizeof(cb));
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

    VkPipelineColorBlendAttachmentState att{};
    att.blendEnable = VK_TRUE;
    att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att.colorBlendOp = VK_BLEND_OP_ADD;
    att.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att.alphaBlendOp = VK_BLEND_OP_ADD;
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
    pipelineInfo.renderPass = m_window->defaultRenderPass();

    VkResult err = m_devFuncs->vkCreateGraphicsPipelines(dev, m_pipelineCache, 1, &pipelineInfo, nullptr, &m_pipeline);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create graphics pipeline");

    ds.stencilTestEnable = VK_FALSE;
    err = m_devFuncs->vkCreateGraphicsPipelines(dev, m_pipelineCache, 1, &pipelineInfo, nullptr, &m_noStencilPipeline);
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

void ConstraintsRenderer::createHandlePipeline() {
    VkDevice dev = m_window->device();

    // Shaders
    VkShaderModule vertShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                             "/assets/shaders/handle_vert.spv");
    VkShaderModule fragShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                             "/assets/shaders/handle_frag.spv");
    // Graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo;
    memset(&pipelineInfo, 0, sizeof(pipelineInfo));
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    VkPipelineShaderStageCreateInfo shaderStages[2] = {
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
        }
    };
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;

    static VkVertexInputBindingDescription vertexBindingDesc = {
        0, // binding
        2*sizeof(float),
        VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkVertexInputAttributeDescription vertexAttrDesc[] = {{0, 0, VK_FORMAT_R32G32_SFLOAT, 0}};
    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.pNext = nullptr;
    vertexInputInfo.flags = 0;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;
    vertexInputInfo.pVertexAttributeDescriptions = vertexAttrDesc;

    pipelineInfo.pVertexInputState = &vertexInputInfo;

    VkPipelineInputAssemblyStateCreateInfo ia;
    memset(&ia, 0, sizeof(ia));
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
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
    pipelineInfo.pRasterizationState = &rs;

    VkPipelineMultisampleStateCreateInfo ms;
    memset(&ms, 0, sizeof(ms));
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    // Enable multisampling.
    ms.rasterizationSamples = m_window->sampleCountFlagBits();
    pipelineInfo.pMultisampleState = &ms;

    VkPipelineDepthStencilStateCreateInfo ds;
    memset(&ds, 0, sizeof(ds));
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    ds.stencilTestEnable = VK_FALSE;
    ds.back.compareOp = VK_COMPARE_OP_EQUAL;
    ds.back.failOp = VK_STENCIL_OP_KEEP;
    ds.back.depthFailOp = VK_STENCIL_OP_KEEP;
    ds.back.passOp = VK_STENCIL_OP_KEEP;
    ds.back.compareMask = 0xff;
    ds.back.writeMask = 0xff;
    ds.back.reference = 1;
    ds.front = ds.back;
    pipelineInfo.pDepthStencilState = &ds;

    VkPipelineColorBlendStateCreateInfo cb;
    memset(&cb, 0, sizeof(cb));
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

    VkPipelineColorBlendAttachmentState att{};
    att.blendEnable = VK_TRUE;
    att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att.colorBlendOp = VK_BLEND_OP_ADD;
    att.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att.alphaBlendOp = VK_BLEND_OP_ADD;
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
    pipelineInfo.renderPass = m_window->defaultRenderPass();

    VkResult err = m_devFuncs->vkCreateGraphicsPipelines(dev, m_pipelineCache, 1, &pipelineInfo, nullptr, &m_handlePipeline);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create graphics pipeline");

    if (vertShaderModule)
        m_devFuncs->vkDestroyShaderModule(dev, vertShaderModule, nullptr);
    if (fragShaderModule)
        m_devFuncs->vkDestroyShaderModule(dev, fragShaderModule, nullptr);
}

void ConstraintsRenderer::createPipelineLayout() {
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
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descSetLayout;
    err = m_devFuncs->vkCreatePipelineLayout(dev, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create pipeline layout");
}

void ConstraintsRenderer::createUniformBuffer() {
    VkDevice dev = m_window->device();

    const int concurrentFrameCount = m_window->concurrentFrameCount();
    const VkPhysicalDeviceLimits *pdevLimits = &m_window->physicalDeviceProperties()->limits;
    const VkDeviceSize uniAlign = pdevLimits->minUniformBufferOffsetAlignment;

    VkBufferCreateInfo uniformBufInfo;
    memset(&uniformBufInfo, 0, sizeof(uniformBufInfo));
    uniformBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    const VkDeviceSize uniformAllocSize = m_window->aligned(34*sizeof(float), uniAlign);
    m_dynamicAlignment = uniformAllocSize;
    uniformBufInfo.size = MAX_CONSTRAINTS * concurrentFrameCount * uniformAllocSize;
    uniformBufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VkResult err = m_devFuncs->vkCreateBuffer(dev, &uniformBufInfo, nullptr, &m_uniformBuf);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create buffer");

    VkMemoryRequirements uniformMemReq;
    m_devFuncs->vkGetBufferMemoryRequirements(dev, m_uniformBuf, &uniformMemReq);

    VkMemoryAllocateInfo uniformMemAllocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        uniformMemReq.size,
        m_window->findMemoryType(uniformMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    };

    err = m_devFuncs->vkAllocateMemory(dev, &uniformMemAllocInfo, nullptr, &m_uniformBufMem);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to allocate memory");

    err = m_devFuncs->vkBindBufferMemory(dev, m_uniformBuf, m_uniformBufMem, 0);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to bind buffer memory");

    quint8 *p;
    err = m_devFuncs->vkMapMemory(dev, m_uniformBufMem, 0, uniformMemReq.size, 0, reinterpret_cast<void **>(&p));
    if (err != VK_SUCCESS)
        m_window->crash("Failed to map memory");

    memset(m_uniformBufInfo, 0, sizeof(m_uniformBufInfo));
    for (int i = 0; i < concurrentFrameCount; ++i) {
        const VkDeviceSize offset = i * MAX_CONSTRAINTS * uniformAllocSize;
        m_uniformBufInfo[i].buffer = m_uniformBuf;
        m_uniformBufInfo[i].offset = offset;
        m_uniformBufInfo[i].range = uniformAllocSize;
    }
    m_devFuncs->vkUnmapMemory(dev, m_uniformBufMem);
}

void ConstraintsRenderer::createDescriptors() {
    VkDevice dev = m_window->device();
    const int concurrentFrameCount = m_window->concurrentFrameCount();

    // Set up descriptor set and its layout.
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(concurrentFrameCount);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(concurrentFrameCount);

    VkDescriptorPoolCreateInfo descPoolInfo;
    memset(&descPoolInfo, 0, sizeof(descPoolInfo));
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descPoolInfo.pPoolSizes = poolSizes.data();
    descPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    descPoolInfo.maxSets = static_cast<uint32_t>(concurrentFrameCount);
    VkResult err = m_devFuncs->vkCreateDescriptorPool(dev, &descPoolInfo, nullptr, &m_descPool);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create descriptor pool");

    VkDescriptorSetLayoutBinding uboLayoutBinding = {
        0, // binding
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        1,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
        nullptr
    };
    VkDescriptorSetLayoutBinding atlasBinding = {
        1, // binding
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        1,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        nullptr
    };

    std::array<VkDescriptorBindingFlags, 2> flags{0};
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
    bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlags.pNext = nullptr;
    bindingFlags.pBindingFlags = flags.data();
    bindingFlags.bindingCount = static_cast<uint32_t>(flags.size());;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, atlasBinding};
    VkDescriptorSetLayoutCreateInfo descLayoutInfo{};
    descLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    descLayoutInfo.pBindings = bindings.data();
    descLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    descLayoutInfo.pNext = &bindingFlags;

    err = m_devFuncs->vkCreateDescriptorSetLayout(dev, &descLayoutInfo, nullptr, &m_descSetLayout);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create descriptor set layout");

    for (int i = 0; i < concurrentFrameCount; ++i) {
        VkDescriptorSetAllocateInfo descSetAllocInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            m_descPool,
            1,
            &m_descSetLayout
        };
        err = m_devFuncs->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_descSet[i]);
        if (err != VK_SUCCESS)
            m_window->crash("Failed to allocate descriptor set");

        std::array<VkDescriptorImageInfo, 1> imageInfo{};
        imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo[0].imageView = m_atlas->getImageView();
        imageInfo[0].sampler = m_atlas->getTextureSampler();

        std::array<VkWriteDescriptorSet, 2> descWrites{};

        descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrites[0].dstSet = m_descSet[i];
        descWrites[0].dstBinding = 0;
        descWrites[0].dstArrayElement = 0;
        descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        descWrites[0].descriptorCount = 1;
        descWrites[0].pBufferInfo = &m_uniformBufInfo[i];
        descWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrites[1].dstSet = m_descSet[i];
        descWrites[1].dstBinding = 1;
        descWrites[1].dstArrayElement = 0;
        descWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descWrites[1].descriptorCount = 1;
        descWrites[1].pImageInfo = &imageInfo[0];

        m_devFuncs->vkUpdateDescriptorSets(dev, static_cast<uint32_t>(descWrites.size()), descWrites.data(), 0, nullptr);
    }
}

void ConstraintsRenderer::draw(VkCommandBuffer cb, Optimizer *opt, const QMatrix4x4& mvp, float scale,
                               const QPointF& lastDown, const QPointF& currentDown, const std::vector<std::array<float, 2>> &pointList) {
    VkDevice dev = m_window->device();
    m_devFuncs->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    static const VkDeviceSize zero = 0;
    m_devFuncs->vkCmdBindVertexBuffers(cb, 0, 1, &m_rectBuf, &zero);

    float *p;
    VkResult err = m_devFuncs->vkMapMemory(dev, m_uniformBufMem, m_uniformBufInfo[m_window->currentFrame()].offset,
                                           MAX_CONSTRAINTS*m_dynamicAlignment, 0, reinterpret_cast<void **>(&p));
    if (err != VK_SUCCESS)
        m_window->crash("Failed to map memory");

    auto &constraints = opt->getConstraints();

    int selected = m_window->getConstraintWidgetList()->currentRow();
    uint32_t drawn = 0;
    for (uint32_t i = 0; i < constraints.size(); ++i) {
        if (constraints[i].lines.empty()) {
            float *curr = p + m_dynamicAlignment/4 * ++drawn;
            memcpy(curr, mvp.constData(), 16 * sizeof(float));
            curr += 16;
            *curr++ = constraints[i].centerX;
            *curr++ = constraints[i].centerY;
            *curr++ = constraints[i].width;
            *curr++ = constraints[i].height;
            *curr++ = constraints[i].dirX;
            *curr++ = constraints[i].dirY;
            *curr++ = constraints[i].rotAngle;
            *curr++ = constraints[i].skewH;
            *curr++ = constraints[i].skewV;
            *curr++ = (int)i == selected ? 2 : 0;
            *curr++ = scale;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = -1;
            *curr++ = constraints[i].tesselationLod;

            uint32_t dynamicOffset = drawn * static_cast<uint32_t>(m_dynamicAlignment);
            m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descSet[m_window->currentFrame()], 1, &dynamicOffset);
            m_devFuncs->vkCmdDraw(cb, 8, 1, 2, 0);
        } else {
            for (uint32_t j = 0; j < constraints[i].lines.size(); j++) {
                float *curr = p + m_dynamicAlignment/4 * ++drawn;
                memcpy(curr, mvp.constData(), 16 * sizeof(float));
                curr += 16;
                *curr++ = constraints[i].centerX;
                *curr++ = constraints[i].centerY;
                *curr++ = constraints[i].width;
                *curr++ = constraints[i].height;
                *curr++ = constraints[i].dirX;
                *curr++ = constraints[i].dirY;
                *curr++ = constraints[i].rotAngle;
                *curr++ = constraints[i].skewH;
                *curr++ = constraints[i].skewV;
                *curr++ = (int)i == selected ? 2 : 0;
                *curr++ = scale;
                auto line = constraints[i].lines[j];
                float lineLength = sqrt((line.x0-line.x1)*(line.x0-line.x1) + (line.y0-line.y1)*(line.y0-line.y1));
                *curr++ = (line.x0+line.x1)/2;
                *curr++ = 1.0-(line.y0+line.y1)/2;
                *curr++ = atan2(line.y0-line.y1, line.x0-line.x1);
                *curr++ = constraints[i].lineCenterX;
                *curr++ = constraints[i].lineCenterY;
                *curr++ = lineLength;
                *curr++ = 1;

                uint32_t dynamicOffset = drawn * static_cast<uint32_t>(m_dynamicAlignment);
                m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descSet[m_window->currentFrame()], 1, &dynamicOffset);
                m_devFuncs->vkCmdDraw(cb, 2, 1, 10, 0);
            }
        }
    }

    if (lastDown != currentDown) {
        m_devFuncs->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_noStencilPipeline);

        if (pointList.empty()) {
            float *curr = p + m_dynamicAlignment/4 * ++drawn;
            memcpy(curr, mvp.constData(), 16 * sizeof(float));
            curr += 16;

            qreal data[9] = {(lastDown.x()+currentDown.x())/2, 1.0f-(lastDown.y()+currentDown.y())/2,
                             abs(lastDown.x()-currentDown.x()), abs(lastDown.y()-currentDown.y()), 0, 0, 0, 0, 0};

            *curr++ = data[0];
            *curr++ = data[1];
            *curr++ = data[2];
            *curr++ = data[3];
            *curr++ = data[4];
            *curr++ = data[5];
            *curr++ = data[6];
            *curr++ = data[7];
            *curr++ = data[8];
            *curr++ = 1;
            *curr++ = scale;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = -1;
            *curr++ = m_window->getTool() == VulkanWindow::Tools::Rectangle ? 1 : 32;

            uint32_t dynamicOffset = drawn * static_cast<uint32_t>(m_dynamicAlignment);
            m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descSet[m_window->currentFrame()], 1, &dynamicOffset);
            m_devFuncs->vkCmdDraw(cb, 8, 1, 2, 0);
        } else {
            for (uint32_t i = 0; i < pointList.size(); i++) {
                float *curr = p + m_dynamicAlignment/4 * ++drawn;
                memcpy(curr, mvp.constData(), 16 * sizeof(float));
                curr += 16;

                Optimizer::Constraint::Line line = {pointList[pointList.size()-1][0], pointList[pointList.size()-1][1], (float)currentDown.x(), (float)currentDown.y()};
                if (i < pointList.size()-1) {
                    line = {pointList[i][0], pointList[i][1], pointList[i+1][0], pointList[i+1][1]};
                }

                float lineLength = sqrt((line.x0-line.x1)*(line.x0-line.x1) + (line.y0-line.y1)*(line.y0-line.y1));
                *curr++ = (line.x0+line.x1)/2;
                *curr++ = 1.0-(line.y0+line.y1)/2;
                *curr++ = lineLength;
                *curr++ = 1.0;
                *curr++ = 1.0;
                *curr++ = 0.0;
                *curr++ = atan2(line.y0-line.y1, line.x0-line.x1);
                *curr++ = 0.0;
                *curr++ = 0.0;
                *curr++ = 1;
                *curr++ = scale;
                *curr++ = 0;
                *curr++ = 0;
                *curr++ = 0;
                *curr++ = 0;
                *curr++ = 0;
                *curr++ = -1;
                *curr++ = 1;

                uint32_t dynamicOffset = drawn * static_cast<uint32_t>(m_dynamicAlignment);
                m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descSet[m_window->currentFrame()], 1, &dynamicOffset);
                m_devFuncs->vkCmdDraw(cb, 2, 1, 10, 0);
            }
        }
    }

    if (m_window->getTool() == VulkanWindow::Tools::Select && selected != -1) {
        m_devFuncs->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_noStencilPipeline);

        if (m_window->getSelectedLine() == -1) {
            float *curr = p + m_dynamicAlignment/4 * ++drawn;
            memcpy(curr, mvp.constData(), 16 * sizeof(float));
            curr += 16;
            *curr++ = constraints[selected].centerX;
            *curr++ = constraints[selected].centerY;
            *curr++ = constraints[selected].width * constraints[selected].lineWidth;
            *curr++ = constraints[selected].height * constraints[selected].lineHeight;
            *curr++ = constraints[selected].dirX;
            *curr++ = constraints[selected].dirY;
            *curr++ = constraints[selected].rotAngle;
            *curr++ = constraints[selected].skewH;
            *curr++ = constraints[selected].skewV;
            *curr++ = 3;
            *curr++ = scale;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = -1;
            *curr++ = 1;

            uint32_t dynamicOffset = drawn * static_cast<uint32_t>(m_dynamicAlignment);
            m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descSet[m_window->currentFrame()], 1, &dynamicOffset);
            m_devFuncs->vkCmdDraw(cb, 8, 1, 2, 0);
        }

        if (constraints[selected].tesselationLod > 1) {
            float *curr = p + m_dynamicAlignment/4 * ++drawn;
            memcpy(curr, mvp.constData(), 16 * sizeof(float));
            curr += 16;
            *curr++ = constraints[selected].centerX;
            *curr++ = constraints[selected].centerY;
            *curr++ = constraints[selected].width * constraints[selected].lineWidth;
            *curr++ = constraints[selected].height * constraints[selected].lineHeight;
            *curr++ = constraints[selected].dirX;
            *curr++ = constraints[selected].dirY;
            *curr++ = constraints[selected].rotAngle;
            *curr++ = constraints[selected].skewH;
            *curr++ = constraints[selected].skewV;
            *curr++ = 3;
            *curr++ = scale;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = -1;
            *curr++ = constraints[selected].tesselationLod;

            uint32_t dynamicOffset = drawn * static_cast<uint32_t>(m_dynamicAlignment);
            m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descSet[m_window->currentFrame()], 1, &dynamicOffset);
            m_devFuncs->vkCmdDraw(cb, 8, 1, 2, 0);
        }

        for (uint32_t j = 0; j < constraints[selected].lines.size(); j++) {
            float *curr = p + m_dynamicAlignment/4 * ++drawn;
            memcpy(curr, mvp.constData(), 16 * sizeof(float));
            curr += 16;
            *curr++ = constraints[selected].centerX;
            *curr++ = constraints[selected].centerY;
            *curr++ = constraints[selected].width;
            *curr++ = constraints[selected].height;
            *curr++ = constraints[selected].dirX;
            *curr++ = constraints[selected].dirY;
            *curr++ = constraints[selected].rotAngle;
            *curr++ = constraints[selected].skewH;
            *curr++ = constraints[selected].skewV;
            *curr++ = 3;
            *curr++ = scale;
            auto line = constraints[selected].lines[j];
            float lineLength = sqrt((line.x0-line.x1)*(line.x0-line.x1) + (line.y0-line.y1)*(line.y0-line.y1));
            *curr++ = (line.x0+line.x1)/2;
            *curr++ = 1.0-(line.y0+line.y1)/2;
            *curr++ = atan2(line.y0-line.y1, line.x0-line.x1);
            *curr++ = constraints[selected].lineCenterX;
            *curr++ = constraints[selected].lineCenterY;
            *curr++ = lineLength;
            *curr++ = 1;

            uint32_t dynamicOffset = drawn * static_cast<uint32_t>(m_dynamicAlignment);
            m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descSet[m_window->currentFrame()], 1, &dynamicOffset);
            m_devFuncs->vkCmdDraw(cb, 2, 1, 10, 0);
        }

        drawHandles(cb, drawn, mvp, scale, p, selected, constraints);
    }

    m_devFuncs->vkUnmapMemory(dev, m_uniformBufMem);
}

void ConstraintsRenderer::drawHandles(VkCommandBuffer cb, uint32_t &drawn, const QMatrix4x4& mvp, float scale, float *p, int selected, const std::vector<Optimizer::Constraint>& constraints) {
    static const VkDeviceSize zero = 0;

    m_devFuncs->vkCmdBindVertexBuffers(cb, 0, 1, &m_handleBuf, &zero);
    m_devFuncs->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_handlePipeline);

    if (m_window->getSelectedLine() == -1) {
        static float transf[8][3]{{1,1, 0}, {1,0, 0}, {1,-1, M_PI/2}, {0,1, M_PI/2}, {0,-1, M_PI/2}, {-1,1, -M_PI/2}, {-1,0, 0}, {-1,-1, M_PI}};
        for (uint32_t i = 0; i < 8; i++) {
            float *curr = p + m_dynamicAlignment/4 * ++drawn;
            memcpy(curr, mvp.constData(), 16 * sizeof(float));
            curr += 16;

            float w = fabs(constraints[selected].width * constraints[selected].lineWidth);
            float h = fabs(constraints[selected].height * constraints[selected].lineHeight);
            float x = ((1.0+constraints[selected].skewV*constraints[selected].skewH)*transf[i][0]*w - transf[i][1]*constraints[selected].skewH*h)/2;
            float y = (transf[i][1]*h - transf[i][0]*constraints[selected].skewV*w)/2;
            *curr++ = constraints[selected].centerX + x*cosf(constraints[selected].rotAngle) + y*sinf(constraints[selected].rotAngle);
            *curr++ = constraints[selected].centerY - x*sinf(constraints[selected].rotAngle) + y*cosf(constraints[selected].rotAngle);
            *curr++ = 0.07/scale;
            *curr++ = 0.07/scale;
            *curr++ = transf[i][0] == 0 || transf[i][1] == 0 ? 1 : 2;
            *curr++ = 0;
            *curr++ = constraints[selected].rotAngle + (transf[i][0] == 0 && transf[i][1] == 0 ? atan2(constraints[selected].dirY, constraints[selected].dirX) : transf[i][2]);
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 1;
            *curr++ = scale;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = -1;

            uint32_t dynamicOffset = drawn * static_cast<uint32_t>(m_dynamicAlignment);
            m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descSet[m_window->currentFrame()], 1, &dynamicOffset);
            m_devFuncs->vkCmdDraw(cb, 4, 1, 0, 0);
        }
    }

    for (uint32_t i = 0; i < constraints[selected].lines.size(); i++) {
        float *curr = p + m_dynamicAlignment/4 * ++drawn;
        memcpy(curr, mvp.constData(), 16 * sizeof(float));
        curr += 16;

        auto line = constraints[selected].lines[i];
        float w = fabs(constraints[selected].width * constraints[selected].lineWidth);
        float h = fabs(constraints[selected].height * constraints[selected].lineHeight);
        float x = 2*(line.x0 - constraints[selected].lineCenterX)/constraints[selected].lineWidth;
        float y = 2*(1.0-line.y0 - constraints[selected].lineCenterY)/constraints[selected].lineHeight;

        x = (constraints[selected].width > 0 ? 1 : -1)*((1.0+constraints[selected].skewV*constraints[selected].skewH)*x*w - y*constraints[selected].skewH*h)/2;
        y = (constraints[selected].height > 0 ? 1 : -1)*(y*h - x*constraints[selected].skewV*w)/2;
        *curr++ = constraints[selected].centerX + x*cosf(constraints[selected].rotAngle) + y*sinf(constraints[selected].rotAngle);
        *curr++ = constraints[selected].centerY - x*sinf(constraints[selected].rotAngle) + y*cosf(constraints[selected].rotAngle);
        *curr++ = 0.07/scale;
        *curr++ = 0.07/scale;
        *curr++ = 4;
        *curr++ = 0;
        *curr++ = constraints[selected].rotAngle;
        *curr++ = 0;
        *curr++ = 0;
        *curr++ = 1;
        *curr++ = scale;
        *curr++ = 0;
        *curr++ = 0;
        *curr++ = 0;
        *curr++ = 0;
        *curr++ = 0;
        *curr++ = -1;

        uint32_t dynamicOffset = drawn * static_cast<uint32_t>(m_dynamicAlignment);
        m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descSet[m_window->currentFrame()], 1, &dynamicOffset);
        m_devFuncs->vkCmdDraw(cb, 4, 1, 0, 0);

        if (i == constraints[selected].lines.size()-1) {
            float *curr = p + m_dynamicAlignment/4 * ++drawn;
            memcpy(curr, mvp.constData(), 16 * sizeof(float));
            curr += 16;

            auto line = constraints[selected].lines[i];
            float w = fabs(constraints[selected].width * constraints[selected].lineWidth);
            float h = fabs(constraints[selected].height * constraints[selected].lineHeight);
            float x = 2*(line.x1 - constraints[selected].lineCenterX)/constraints[selected].lineWidth;
            float y = 2*(1.0-line.y1 - constraints[selected].lineCenterY)/constraints[selected].lineHeight;

            x = (constraints[selected].width > 0 ? 1 : -1)*((1.0+constraints[selected].skewV*constraints[selected].skewH)*x*w - y*constraints[selected].skewH*h)/2;
            y = (constraints[selected].height > 0 ? 1 : -1)*(y*h - x*constraints[selected].skewV*w)/2;
            *curr++ = constraints[selected].centerX + x*cosf(constraints[selected].rotAngle) + y*sinf(constraints[selected].rotAngle);
            *curr++ = constraints[selected].centerY - x*sinf(constraints[selected].rotAngle) + y*cosf(constraints[selected].rotAngle);
            *curr++ = 0.07/scale;
            *curr++ = 0.07/scale;
            *curr++ = 4;
            *curr++ = 0;
            *curr++ = constraints[selected].rotAngle;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 1;
            *curr++ = scale;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = 0;
            *curr++ = -1;

            uint32_t dynamicOffset = drawn * static_cast<uint32_t>(m_dynamicAlignment);
            m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descSet[m_window->currentFrame()], 1, &dynamicOffset);
            m_devFuncs->vkCmdDraw(cb, 4, 1, 0, 0);
        }
    }
}
