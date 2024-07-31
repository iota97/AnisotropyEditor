#include "src/Render/montecarlo.h"
#include <QCoreApplication>
#include "src/Render/mesh.h"

MonteCarlo::MonteCarlo(VulkanWindow *window, VkPipelineCache &pipelineCache, VkPipelineLayout &pipelineLayout)
    : m_window(window), m_pipelineCache(pipelineCache), m_pipelineLayout(pipelineLayout) {
    VkDevice dev = m_window->device();
    m_devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    createMeshData();
    createRenderPass();
    createCopyRenderPass();
    createDescriptors();
    createResolvePipelineLayout();
    createResolvePipeline();
    createCopyPipeline();
}

MonteCarlo::~MonteCarlo() {
    VkDevice dev = m_window->device();

    delete m_result;
    delete m_temp;
    delete m_depth;

    if (m_renderPass) {
        m_devFuncs->vkDestroyRenderPass(dev, m_renderPass, nullptr);
    }
    if (m_frameBuffer) {
        m_devFuncs->vkDestroyFramebuffer(dev, m_frameBuffer, nullptr);
    }
    if (m_copyRenderPass) {
        m_devFuncs->vkDestroyRenderPass(dev, m_copyRenderPass, nullptr);
    }
    if (m_copyFrameBuffer) {
        m_devFuncs->vkDestroyFramebuffer(dev, m_copyFrameBuffer, nullptr);
    }
    if (m_pipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_pipeline, nullptr);
    }
    if (m_resolvePipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_resolvePipeline, nullptr);
    }
    if (m_copyPipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_copyPipeline, nullptr);
    }
    if (m_resolveDescSetLayout) {
        m_devFuncs->vkDestroyDescriptorSetLayout(dev, m_resolveDescSetLayout, nullptr);
    }
    if (m_resolveDescPool) {
        m_devFuncs->vkDestroyDescriptorPool(dev, m_resolveDescPool, nullptr);
    }
    if (m_resolvePipelineLayout) {
        m_devFuncs->vkDestroyPipelineLayout(dev, m_resolvePipelineLayout, nullptr);
    }
    if (m_meshBuf) {
        m_devFuncs->vkDestroyBuffer(dev, m_meshBuf, nullptr);
    }
    if (m_meshBufMem) {
        m_devFuncs->vkFreeMemory(dev, m_meshBufMem, nullptr);
    }
}

void MonteCarlo::createMeshData() {
    VkDevice dev = m_window->device();

    float data[12] = {-1.0, -1.0, 1.0, 1.0, 1.0, -1.0,
                      -1.0, -1.0, -1.0, 1.0, 1.0, 1.0};

    VkBufferCreateInfo meshBufInfo;
    memset(&meshBufInfo, 0, sizeof(meshBufInfo));
    meshBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    meshBufInfo.size = 12*sizeof(float);
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

    memcpy(p, data, 12*sizeof(float));
    m_devFuncs->vkUnmapMemory(dev, m_meshBufMem);
}

void MonteCarlo::createResolvePipelineLayout() {
    VkDevice dev = m_window->device();

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    memset(&pipelineLayoutInfo, 0, sizeof(pipelineLayoutInfo));
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_resolveDescSetLayout;
    VkResult err = m_devFuncs->vkCreatePipelineLayout(dev, &pipelineLayoutInfo, nullptr, &m_resolvePipelineLayout);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create pipeline layout");
}

void MonteCarlo::createRenderPass() {
    VkDevice dev = m_window->device();

    // Renderpass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
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

void MonteCarlo::createCopyRenderPass() {
    VkDevice dev = m_window->device();

    // Renderpass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
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
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
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

    if (m_devFuncs->vkCreateRenderPass(dev, &renderPassInfo, nullptr, &m_copyRenderPass) != VK_SUCCESS) {
        m_window->crash("failed to create render pass!");
    }
}

void MonteCarlo::createDescriptors() {
    VkDevice dev = m_window->device();
    const int concurrentFrameCount = m_window->concurrentFrameCount();

    // Set up descriptor set and its layout.
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
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
    VkResult err = m_devFuncs->vkCreateDescriptorPool(dev, &descPoolInfo, nullptr, &m_resolveDescPool);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create descriptor pool");

    VkDescriptorSetLayoutBinding tempSamplerBinding = {
        0, // binding
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        1,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        nullptr
    };

    VkDescriptorSetLayoutBinding resultSamplerBinding = {
        1, // binding
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        1,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        nullptr
    };

    std::array<VkDescriptorBindingFlags, 2> flags{VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT};
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
    bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlags.pNext = nullptr;
    bindingFlags.pBindingFlags = flags.data();
    bindingFlags.bindingCount = static_cast<uint32_t>(flags.size());;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {tempSamplerBinding,resultSamplerBinding};
    VkDescriptorSetLayoutCreateInfo descLayoutInfo{};
    descLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    descLayoutInfo.pBindings = bindings.data();
    descLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    descLayoutInfo.pNext = &bindingFlags;

    err = m_devFuncs->vkCreateDescriptorSetLayout(dev, &descLayoutInfo, nullptr, &m_resolveDescSetLayout);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create descriptor set layout");

    for (int i = 0; i < concurrentFrameCount; ++i) {
        VkDescriptorSetAllocateInfo descSetAllocInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            m_resolveDescPool,
            1,
            &m_resolveDescSetLayout
        };
        err = m_devFuncs->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_resolveDescSet[i]);
        if (err != VK_SUCCESS)
            m_window->crash("Failed to allocate descriptor set");
    }
}

void MonteCarlo::createResolvePipeline() {
    VkDevice dev = m_window->device();

    // Shaders
    VkShaderModule vertShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                        "/assets/shaders/resolve_vert.spv");
    VkShaderModule fragShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                        "/assets/shaders/resolve_frag.spv");
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
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;
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

    pipelineInfo.layout = m_resolvePipelineLayout;
    pipelineInfo.renderPass = m_window->defaultRenderPass();

    VkResult err = m_devFuncs->vkCreateGraphicsPipelines(dev, m_pipelineCache, 1, &pipelineInfo, nullptr, &m_resolvePipeline);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create graphics pipeline");

    if (vertShaderModule)
        m_devFuncs->vkDestroyShaderModule(dev, vertShaderModule, nullptr);
    if (fragShaderModule)
        m_devFuncs->vkDestroyShaderModule(dev, fragShaderModule, nullptr);
}

void MonteCarlo::createCopyPipeline() {
    VkDevice dev = m_window->device();

    // Shaders
    VkShaderModule vertShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                             "/assets/shaders/copy_vert.spv");
    VkShaderModule fragShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                                             "/assets/shaders/copy_frag.spv");
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
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;
    pipelineInfo.pRasterizationState = &rs;

    VkPipelineMultisampleStateCreateInfo ms;
    memset(&ms, 0, sizeof(ms));
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    // Enable multisampling.
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipelineInfo.pMultisampleState = &ms;

    VkPipelineDepthStencilStateCreateInfo ds;
    memset(&ds, 0, sizeof(ds));
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineInfo.pDepthStencilState = &ds;

    VkPipelineColorBlendStateCreateInfo cb;
    memset(&cb, 0, sizeof(cb));
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

    VkPipelineColorBlendAttachmentState att{};
    att.blendEnable = VK_TRUE;
    att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    att.colorBlendOp = VK_BLEND_OP_ADD;
    att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
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

    pipelineInfo.layout = m_resolvePipelineLayout;
    pipelineInfo.renderPass = m_copyRenderPass;

    VkResult err = m_devFuncs->vkCreateGraphicsPipelines(dev, m_pipelineCache, 1, &pipelineInfo, nullptr, &m_copyPipeline);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create graphics pipeline");

    if (vertShaderModule)
        m_devFuncs->vkDestroyShaderModule(dev, vertShaderModule, nullptr);
    if (fragShaderModule)
        m_devFuncs->vkDestroyShaderModule(dev, fragShaderModule, nullptr);
}

void MonteCarlo::setPipeline(const std::string &name) {
    VkDevice dev = m_window->device();
    if (m_pipeline) {
        m_devFuncs->vkQueueWaitIdle(m_window->graphicsQueue());
        m_devFuncs->vkDestroyPipeline(dev, m_pipeline, nullptr);
    }

    // Shaders
    VkShaderModule vertShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                        "/assets/shaders/"+QString::fromStdString(name)+"_vert.spv");
    VkShaderModule fragShaderModule = m_window->createShader(QCoreApplication::applicationDirPath()+
                                        "/assets/shaders/"+QString::fromStdString(name)+"_frag.spv");

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

    VkVertexInputBindingDescription vertexBindingDesc = {
        0, // binding
        12*sizeof(float),
        VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkVertexInputAttributeDescription vertexAttrDesc[] = {
        { // position
            0, // location
            0, // binding
            VK_FORMAT_R32G32B32_SFLOAT,
            4*sizeof(float)
        },
        { // normal
            1,
            0,
            VK_FORMAT_R32G32B32_SFLOAT,
            7*sizeof(float)
        },
        { // texcoord
            2,
            0,
            VK_FORMAT_R32G32_SFLOAT,
            10*sizeof(float)
        },
        { // tangent
            3,
            0,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            0
        },
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.pNext = nullptr;
    vertexInputInfo.flags = 0;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = 4;
    vertexInputInfo.pVertexAttributeDescriptions = vertexAttrDesc;

    pipelineInfo.pVertexInputState = &vertexInputInfo;

    VkPipelineInputAssemblyStateCreateInfo ia;
    memset(&ia, 0, sizeof(ia));
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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

    VkResult err = m_devFuncs->vkCreateGraphicsPipelines(dev, m_pipelineCache, 1, &pipelineInfo, nullptr, &m_pipeline);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create graphics pipeline");

    if (vertShaderModule)
        m_devFuncs->vkDestroyShaderModule(dev, vertShaderModule, nullptr);
    if (fragShaderModule)
        m_devFuncs->vkDestroyShaderModule(dev, fragShaderModule, nullptr);

    clear();
}

void MonteCarlo::resizeFrameBuffer() {
    const QSize sz = m_window->swapChainImageSize();

    delete m_result;
    m_result = new Texture(sz.width(), sz.height(), m_window, 1, VK_FORMAT_R32G32B32A32_SFLOAT, true, true, true);
    delete m_temp;
    m_temp = new Texture(sz.width(), sz.height(), m_window, 1, VK_FORMAT_R32G32B32A32_SFLOAT, true, true, true);
    delete m_depth;
    m_depth = new Texture(sz.width(), sz.height(), m_window, 1, VK_FORMAT_D32_SFLOAT, false, true, false, true);
    clear();

    if (m_frameBuffer) {
        m_devFuncs->vkDestroyFramebuffer(m_window->device(), m_frameBuffer, nullptr);
    }
    if (m_copyFrameBuffer) {
        m_devFuncs->vkDestroyFramebuffer(m_window->device(), m_copyFrameBuffer, nullptr);
    }

    std::array<VkImageView, 2> attachments = {
        m_temp->getImageView(),
        m_depth->getImageView()
    };

    VkFramebufferCreateInfo framebufferInfo {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_renderPass;
    framebufferInfo.attachmentCount = attachments.size();
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = m_temp->getWidth();
    framebufferInfo.height =  m_temp->getHeight();
    framebufferInfo.layers = 1;

    m_devFuncs->vkCreateFramebuffer(m_window->device(), &framebufferInfo, NULL, &m_frameBuffer);

    std::array<VkImageView, 2> copyAttachments = {
        m_result->getImageView(),
        m_depth->getImageView()
    };

    VkFramebufferCreateInfo copyFramebufferInfo {};
    copyFramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    copyFramebufferInfo.renderPass = m_copyRenderPass;
    copyFramebufferInfo.attachmentCount = copyAttachments.size();
    copyFramebufferInfo.pAttachments = copyAttachments.data();
    copyFramebufferInfo.width = m_temp->getWidth();
    copyFramebufferInfo.height =  m_temp->getHeight();
    copyFramebufferInfo.layers = 1;

    m_devFuncs->vkCreateFramebuffer(m_window->device(), &copyFramebufferInfo, NULL, &m_copyFrameBuffer);

    VkDevice dev = m_window->device();
    for (int i = 0; i < m_window->concurrentFrameCount(); ++i) {
        std::array<VkDescriptorImageInfo, 2> imageInfo{};
        imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo[0].imageView = m_temp->getImageView();
        imageInfo[0].sampler = m_temp->getTextureSampler();
        imageInfo[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo[1].imageView = m_result->getImageView();
        imageInfo[1].sampler = m_result->getTextureSampler();

        std::array<VkWriteDescriptorSet, 2> descWrites{};
        descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrites[0].dstSet = m_resolveDescSet[i];
        descWrites[0].dstBinding = 0;
        descWrites[0].dstArrayElement = 0;
        descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descWrites[0].descriptorCount = 1;
        descWrites[0].pImageInfo = &imageInfo[0];
        descWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrites[1].dstSet = m_resolveDescSet[i];
        descWrites[1].dstBinding = 1;
        descWrites[1].dstArrayElement = 0;
        descWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descWrites[1].descriptorCount = 1;
        descWrites[1].pImageInfo = &imageInfo[1];

        m_devFuncs->vkUpdateDescriptorSets(dev, 2, &descWrites[0], 0, nullptr);
    }
}

void MonteCarlo::clear() {
    if (m_result) m_result->clear(0, 0, 0, 0);
    m_sampleCount = 0;
}

void MonteCarlo::render(Mesh *mesh, VkBuffer *quadBuf, VkDescriptorSet descSet) {
    if (m_sampleCount == 1 << 16) return;

    VkDevice dev = m_window->device();

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

    VkClearColorValue clearColor = {{ 0.45, 0.45, 0.45, 1 }};
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
    rp_begin.renderArea.extent.width = m_temp->getWidth();
    rp_begin.renderArea.extent.height = m_temp->getHeight();
    rp_begin.clearValueCount = 2;
    rp_begin.pClearValues = clearValues;


    m_devFuncs->vkCmdBeginRenderPass(cb, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport;
    viewport.height = m_temp->getHeight();
    viewport.width = m_temp->getWidth();
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    viewport.x = 0;
    viewport.y = 0;
    m_devFuncs->vkCmdSetViewport(cb, 0, 1, &viewport);

    VkRect2D scissor;
    scissor.extent.width = m_temp->getWidth();
    scissor.extent.height = m_temp->getHeight();
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    m_devFuncs->vkCmdSetScissor(cb, 0, 1, &scissor);

    m_devFuncs->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
                                        &descSet, 0, nullptr);

    if (mesh) {
        mesh->draw(cb);
    } else {
        static const VkDeviceSize zero = 0;
        m_devFuncs->vkCmdBindVertexBuffers(cb, 0, 1, quadBuf, &zero);
        m_devFuncs->vkCmdDraw(cb, 6, 1, 0, 0);
    }

    m_devFuncs->vkCmdEndRenderPass(cb);
    m_devFuncs->vkEndCommandBuffer(cb);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cb;

    m_devFuncs->vkQueueSubmit(m_window->graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    m_devFuncs->vkQueueWaitIdle(m_window->graphicsQueue());
    m_devFuncs->vkFreeCommandBuffers(dev, m_window->graphicsCommandPool(), 1, &cb);

    copy();
    m_sampleCount++;
}

void MonteCarlo::copy() {
    VkDevice dev = m_window->device();

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
    VkRenderPassBeginInfo rp_begin {};
    rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.pNext = NULL;
    rp_begin.renderPass = m_copyRenderPass;
    rp_begin.framebuffer = m_copyFrameBuffer;
    rp_begin.renderArea.offset.x = 0;
    rp_begin.renderArea.offset.y = 0;
    rp_begin.renderArea.extent.width = m_temp->getWidth();
    rp_begin.renderArea.extent.height = m_temp->getHeight();

    m_devFuncs->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_copyPipeline);
    m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_resolvePipelineLayout, 0, 1,
                                        m_resolveDescSet, 0, nullptr);
    m_devFuncs->vkCmdBeginRenderPass(cb, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport;
    viewport.height = m_temp->getHeight();
    viewport.width = m_temp->getWidth();
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    viewport.x = 0;
    viewport.y = 0;
    m_devFuncs->vkCmdSetViewport(cb, 0, 1, &viewport);

    VkRect2D scissor;
    scissor.extent.width = m_temp->getWidth();
    scissor.extent.height = m_temp->getHeight();
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    m_devFuncs->vkCmdSetScissor(cb, 0, 1, &scissor);

    static const VkDeviceSize zero = 0;
    m_devFuncs->vkCmdBindVertexBuffers(cb, 0, 1, &m_meshBuf, &zero);
    m_devFuncs->vkCmdDraw(cb, 6, 1, 0, 0);

    m_devFuncs->vkCmdEndRenderPass(cb);
    m_devFuncs->vkEndCommandBuffer(cb);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cb;

    m_devFuncs->vkQueueSubmit(m_window->graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    m_devFuncs->vkQueueWaitIdle(m_window->graphicsQueue());
    m_devFuncs->vkFreeCommandBuffers(dev, m_window->graphicsCommandPool(), 1, &cb);
}

uint32_t MonteCarlo::getSampleCount() {
    return m_sampleCount*16;
}

void MonteCarlo::resolve(VkCommandBuffer cb) {
    m_devFuncs->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_resolvePipeline);
    m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_resolvePipelineLayout, 0, 1,
        &m_resolveDescSet[m_window->currentFrame()], 0, nullptr);

    static const VkDeviceSize zero = 0;
    m_devFuncs->vkCmdBindVertexBuffers(cb, 0, 1, &m_meshBuf, &zero);
    m_devFuncs->vkCmdDraw(cb, 6, 1, 0, 0);
}
