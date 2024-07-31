#include "render.h"
#include "src/UI/vulkanwindow.h"
#include <QVulkanFunctions>
#include <QFile>
#include <QCoreApplication>
#include "src/Field/anisotropy.h"
#include "src/Render/montecarlo.h"
#include "src/Render/constraints.h"
#include "src/Render/mesh.h"
#include "src/Texture/cubemap.h"
#include "src/Field/optimizer.h"

Render::Render(VulkanWindow *w, Optimizer *&o, const std::vector<std::array<float, 2>> &pointList, bool msaa)
    : m_window(w), m_optimizer(o), m_pointList(pointList) {
    if (msaa) {
        const QList<int> counts = w->supportedSampleCounts();
        qDebug() << "Supported sample counts:" << counts;
        for (int s = 16; s >= 4; s /= 2) {
            if (counts.contains(s)) {
                qDebug("Requesting sample count %d", s);
                m_window->setSampleCount(s);
                break;
            }
        }
    }
}

void Render::initResources() {
    m_timer.start();
    VkDevice dev = m_window->device();
    m_devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    m_anisotropy = new Anisotropy(m_window, m_monteCarloRender);
    m_optimizer = new Optimizer(m_window, m_anisotropy);
    m_optimizer->setDirectionTexture(m_anisotropy->getDir());
    m_constraints = new ConstraintsRenderer(m_window);
    m_cubeMap = new CubeMap({
                            QCoreApplication::applicationDirPath()+"/assets/textures/cubemap/px.hdr",
                            QCoreApplication::applicationDirPath()+"/assets/textures/cubemap/nx.hdr",
                            QCoreApplication::applicationDirPath()+"/assets/textures/cubemap/py.hdr",
                            QCoreApplication::applicationDirPath()+"/assets/textures/cubemap/ny.hdr",
                            QCoreApplication::applicationDirPath()+"/assets/textures/cubemap/pz.hdr",
                            QCoreApplication::applicationDirPath()+"/assets/textures/cubemap/nz.hdr"
                            }, m_window);
    m_reference = new Texture(16, 16, m_window);

    createQuadData();
    resetMeshTransform();
    createUniformBuffer();
    createDescriptors();
    createPipelineLayout();
    updateAnisotropyTextureDescriptor();

    m_monteCarloRender = new MonteCarlo(m_window, m_pipelineCache, m_pipelineLayout);
    setPipeline("fast");

    m_window->updateMouseLabel();
}

void Render::initSwapChainResources() {
    // Projection matrix
    m_proj = m_window->clipCorrectionMatrix(); // adjust for Vulkan-OpenGL clip space differences
    const QSize sz = m_window->swapChainImageSize();
    m_proj.perspective(45.0, sz.width() / (float) sz.height(), 0.01f, 100.0f);
    m_proj.translate(0, 0, m_zoom);
    if (m_monteCarloRender) m_monteCarloRender->resizeFrameBuffer();
}

void Render::startNextFrame() {
    VkDevice dev = m_window->device();
    VkCommandBuffer cb = m_window->currentCommandBuffer();
    const QSize sz = m_window->swapChainImageSize();

    VkClearColorValue clearColor = {{ 0.45, 0.45, 0.45, 1 }};
    VkClearDepthStencilValue clearDS = { 1, 0 };
    VkClearValue clearValues[3];
    memset(clearValues, 0, sizeof(clearValues));
    clearValues[0].color = clearValues[2].color = clearColor;
    clearValues[1].depthStencil = clearDS;

    VkRenderPassBeginInfo rpBeginInfo;
    memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass = m_window->defaultRenderPass();
    rpBeginInfo.framebuffer = m_window->currentFramebuffer();
    rpBeginInfo.renderArea.extent.width = sz.width();
    rpBeginInfo.renderArea.extent.height = sz.height();
    rpBeginInfo.clearValueCount = m_window->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3 : 2;
    rpBeginInfo.pClearValues = clearValues;
    rpBeginInfo.clearValueCount = m_window->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3 : 2;
    rpBeginInfo.pClearValues = clearValues;

    float *p;
    VkResult err = m_devFuncs->vkMapMemory(dev, m_uniformBufMem, m_uniformBufInfo[m_window->currentFrame()].offset,
                                           43*sizeof(float), 0, reinterpret_cast<void **>(&p));
    if (err != VK_SUCCESS)
        m_window->crash("Failed to map memory");

    QMatrix4x4 model;
    model.translate(m_meshPosition);
    model.rotate(m_meshRotation);
    model.scale(m_meshScale);

    memcpy(p, model.constData(), 16 * sizeof(float));       
    memcpy(p+16, m_proj.constData(), 16 * sizeof(float));
    float *projP = p+16;
    p += 32;
    *p++ = m_albedo[0];
    *p++ = m_albedo[1];
    *p++ = m_albedo[2];
    *p++ = m_metallic;
    *p++ = m_matAnisotropy;
    *p++ = m_roughness;
    *p++ = m_mesh ? 1.0 : float(m_anisotropy->getDir()->getWidth())/m_anisotropy->getDir()->getHeight();
    *p++ = m_meshScale;
    *p++ = m_sampleCount;
    *p++ = m_monteCarlo ? rand()/(float)RAND_MAX + 0.52 : 0.0;
    *p++ = m_exposure;

    if (m_monteCarlo) {
        float r1 = rand()/(float)RAND_MAX;
        float r2 = rand()/(float)RAND_MAX;

        float offsetX = 2.0*(r1-0.5)/sz.width();
        float offsetY = 2.0*(r2-0.5)/sz.height();

        QMatrix4x4 monteCarlo;
        monteCarlo.translate(offsetX, offsetY, 0);
        QMatrix4x4 proj = monteCarlo*m_proj;
        memcpy(projP, proj.constData(), 16 * sizeof(float));

        m_monteCarloRender->render(m_mesh, &m_quadBuf, m_descSet[m_window->currentFrame()]);
    }

    m_frameCount++;
    if (m_timer.elapsed() > 500) {
        VkPhysicalDeviceProperties properties{};
        m_window->vulkanInstance()->functions()->vkGetPhysicalDeviceProperties(m_window->physicalDevice(), &properties);
        m_window->setFPSLabel((m_monteCarlo?QString::number(m_monteCarloRender->getSampleCount())+" Samples  |  ":"")+
                              QString::number(1000.f*m_frameCount/m_timer.restart(), 'f', 1)+" FPS");
        m_frameCount = 0;
    }

    m_devFuncs->vkCmdBeginRenderPass(cb, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport;
    viewport.x = viewport.y = 0;
    viewport.width = sz.width();
    viewport.height = sz.height();
    viewport.minDepth = 0;
    viewport.maxDepth = 1;
    m_devFuncs->vkCmdSetViewport(cb, 0, 1, &viewport);

    VkRect2D scissor;
    scissor.offset.x = scissor.offset.y = 0;
    scissor.extent.width = viewport.width;
    scissor.extent.height = viewport.height;
    m_devFuncs->vkCmdSetScissor(cb, 0, 1, &scissor);

    m_devFuncs->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
                                        &m_descSet[m_window->currentFrame()], 0, nullptr);

    if (m_mesh) {
        m_mesh->draw(cb);
    } else {
        static const VkDeviceSize zero = 0;
        m_devFuncs->vkCmdBindVertexBuffers(cb, 0, 1, &m_quadBuf, &zero);
        m_devFuncs->vkCmdDraw(cb, 6, 1, 0, 0);
    }

    if (m_monteCarlo) {
        m_monteCarloRender->resolve(cb);
    }
    m_devFuncs->vkUnmapMemory(dev, m_uniformBufMem);

    if (m_showConstraints) {
        m_constraints->draw(cb, m_optimizer, m_proj*model, m_meshScale, m_lastDown, m_currentDown, m_pointList);
    }

    m_devFuncs->vkCmdEndRenderPass(cb);

    m_window->frameReady();
    m_window->requestUpdate();
}

void Render::releaseResources() {
    VkDevice dev = m_window->device();

    delete m_reference;
    m_reference = nullptr;

    delete m_mesh;
    m_mesh = nullptr;

    delete m_anisotropy;
    m_anisotropy = nullptr;

    delete m_constraints;
    m_constraints = nullptr;

    delete m_optimizer;
    m_optimizer = nullptr;

    delete m_monteCarloRender;
    m_monteCarloRender = nullptr;

    delete m_cubeMap;
    m_cubeMap = nullptr;

    if (m_quadBuf) {
        m_devFuncs->vkDestroyBuffer(dev, m_quadBuf, nullptr);
        m_quadBuf = VK_NULL_HANDLE;
    }

    if (m_quadBufMem) {
        m_devFuncs->vkFreeMemory(dev, m_quadBufMem, nullptr);
        m_quadBufMem = VK_NULL_HANDLE;
    }

    if (m_pipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
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

void Render::createPipelineLayout() {
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

void Render::createUniformBuffer() {
    VkDevice dev = m_window->device();

    const int concurrentFrameCount = m_window->concurrentFrameCount();
    const VkPhysicalDeviceLimits *pdevLimits = &m_window->physicalDeviceProperties()->limits;
    const VkDeviceSize uniAlign = pdevLimits->minUniformBufferOffsetAlignment;

    VkBufferCreateInfo uniformBufInfo;
    memset(&uniformBufInfo, 0, sizeof(uniformBufInfo));
    uniformBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    const VkDeviceSize uniformAllocSize = m_window->aligned(43*sizeof(float), uniAlign);
    uniformBufInfo.size = concurrentFrameCount * uniformAllocSize;
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
        m_window->crash("Failed to bind buffer");

    quint8 *p;
    err = m_devFuncs->vkMapMemory(dev, m_uniformBufMem, 0, uniformMemReq.size, 0, reinterpret_cast<void **>(&p));
    if (err != VK_SUCCESS)
        m_window->crash("Failed to map memory");

    QMatrix4x4 ident;
    memset(m_uniformBufInfo, 0, sizeof(m_uniformBufInfo));
    for (int i = 0; i < concurrentFrameCount; ++i) {
        const VkDeviceSize offset = i * uniformAllocSize;
        memcpy(p + offset, ident.constData(), 16 * sizeof(float));
        m_uniformBufInfo[i].buffer = m_uniformBuf;
        m_uniformBufInfo[i].offset = offset;
        m_uniformBufInfo[i].range = uniformAllocSize;
    }
    m_devFuncs->vkUnmapMemory(dev, m_uniformBufMem);
}

void Render::createDescriptors() {
    VkDevice dev = m_window->device();
    const int concurrentFrameCount = m_window->concurrentFrameCount();

    // Set up descriptor set and its layout.
    std::array<VkDescriptorPoolSize, 6> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(concurrentFrameCount);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(concurrentFrameCount);
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[2].descriptorCount = static_cast<uint32_t>(concurrentFrameCount);
    poolSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[3].descriptorCount = static_cast<uint32_t>(concurrentFrameCount);
    poolSizes[4].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[4].descriptorCount = static_cast<uint32_t>(concurrentFrameCount);
    poolSizes[5].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[5].descriptorCount = static_cast<uint32_t>(concurrentFrameCount);

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
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        1,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        nullptr
    };
    VkDescriptorSetLayoutBinding anisoLayoutBinding = {
        1, // binding
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        1,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        nullptr
    };
    VkDescriptorSetLayoutBinding anisoLayoutDirBinding = {
        2, // binding
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        1,
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        nullptr
    };
    VkDescriptorSetLayoutBinding cubemapBinding = {
        3, // binding
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        1,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        nullptr
    };
    VkDescriptorSetLayoutBinding referenceBinding = {
        4, // binding
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        1,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        nullptr
    };
    VkDescriptorSetLayoutBinding ggxCubemapBinding = {
        5, // binding
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        1,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        nullptr
    };

    std::array<VkDescriptorBindingFlags, 6> flags{0,
                                                  VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
                                                  VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
                                                  VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
                                                  VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
                                                  VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT};

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
    bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlags.pNext = nullptr;
    bindingFlags.pBindingFlags = flags.data();
    bindingFlags.bindingCount = static_cast<uint32_t>(flags.size());;

    std::array<VkDescriptorSetLayoutBinding, 6> bindings = {uboLayoutBinding, anisoLayoutBinding,
                                                            anisoLayoutDirBinding, cubemapBinding,
                                                            referenceBinding, ggxCubemapBinding};
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

        std::array<VkDescriptorImageInfo, 5> imageInfo{};
        imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo[0].imageView = m_anisotropy->getMap()->getImageView();
        imageInfo[0].sampler = m_anisotropy->getMap()->getTextureSampler();
        imageInfo[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo[1].imageView = m_anisotropy->getDir()->getImageView();
        imageInfo[1].sampler = m_anisotropy->getDir()->getTextureSampler();
        imageInfo[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo[2].imageView = m_cubeMap->getImageView();
        imageInfo[2].sampler = m_cubeMap->getTextureSampler();
        imageInfo[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo[3].imageView = m_reference->getImageView();
        imageInfo[3].sampler = m_reference->getTextureSampler();
        imageInfo[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo[4].imageView = m_cubeMap->getImageView(1);
        imageInfo[4].sampler = m_cubeMap->getTextureSampler();

        std::array<VkWriteDescriptorSet, 6> descWrites{};
        descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrites[0].dstSet = m_descSet[i];
        descWrites[0].dstBinding = 0;
        descWrites[0].dstArrayElement = 0;
        descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descWrites[0].descriptorCount = 1;
        descWrites[0].pBufferInfo = &m_uniformBufInfo[i];

        descWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrites[1].dstSet = m_descSet[i];
        descWrites[1].dstBinding = 1;
        descWrites[1].dstArrayElement = 0;
        descWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descWrites[1].descriptorCount = 1;
        descWrites[1].pImageInfo = &imageInfo[0];

        descWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrites[2].dstSet = m_descSet[i];
        descWrites[2].dstBinding = 2;
        descWrites[2].dstArrayElement = 0;
        descWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descWrites[2].descriptorCount = 1;
        descWrites[2].pImageInfo = &imageInfo[1];

        descWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrites[3].dstSet = m_descSet[i];
        descWrites[3].dstBinding = 3;
        descWrites[3].dstArrayElement = 0;
        descWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descWrites[3].descriptorCount = 1;
        descWrites[3].pImageInfo = &imageInfo[2];

        descWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrites[4].dstSet = m_descSet[i];
        descWrites[4].dstBinding = 4;
        descWrites[4].dstArrayElement = 0;
        descWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descWrites[4].descriptorCount = 1;
        descWrites[4].pImageInfo = &imageInfo[3];

        descWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrites[5].dstSet = m_descSet[i];
        descWrites[5].dstBinding = 5;
        descWrites[5].dstArrayElement = 0;
        descWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descWrites[5].descriptorCount = 1;
        descWrites[5].pImageInfo = &imageInfo[4];

        m_devFuncs->vkUpdateDescriptorSets(dev, static_cast<uint32_t>(descWrites.size()), descWrites.data(), 0, nullptr);
    }
}

void Render::updateAnisotropyTextureDescriptor() {
    for (int i = 0; i < m_window->concurrentFrameCount(); ++i) {
        std::array<VkDescriptorImageInfo, 2> imageInfo{};
        imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo[0].imageView = m_anisotropy->getMap()->getImageView();
        imageInfo[0].sampler = m_anisotropy->getMap()->getTextureSampler();
        imageInfo[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo[1].imageView = m_anisotropy->getDir()->getImageView();
        imageInfo[1].sampler = m_anisotropy->getDir()->getTextureSampler();

        std::array<VkWriteDescriptorSet, 2> descWrites{};
        descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrites[0].dstSet = m_descSet[i];
        descWrites[0].dstBinding = 1;
        descWrites[0].dstArrayElement = 0;
        descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descWrites[0].descriptorCount = 1;
        descWrites[0].pImageInfo = &imageInfo[0];
        descWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrites[1].dstSet = m_descSet[i];
        descWrites[1].dstBinding = 2;
        descWrites[1].dstArrayElement = 0;
        descWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descWrites[1].descriptorCount = 1;
        descWrites[1].pImageInfo = &imageInfo[1];

        m_devFuncs->vkUpdateDescriptorSets(m_window->device(), 2, descWrites.data(), 0, nullptr);
    }
}

void Render::setPipeline(const std::string &name, bool montecarlo) {
    m_monteCarlo = montecarlo;
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
    ms.rasterizationSamples = m_window->sampleCountFlagBits();
    pipelineInfo.pMultisampleState = &ms;

    VkPipelineDepthStencilStateCreateInfo ds;
    memset(&ds, 0, sizeof(ds));
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    ds.stencilTestEnable = VK_TRUE;
    ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
    ds.back.failOp = VK_STENCIL_OP_REPLACE;
    ds.back.depthFailOp = VK_STENCIL_OP_REPLACE;
    ds.back.passOp = VK_STENCIL_OP_REPLACE;
    ds.back.compareMask = 0xff;
    ds.back.writeMask = 0xff;
    ds.back.reference = 1;
    ds.front = ds.back;
    pipelineInfo.pDepthStencilState = &ds;

    VkPipelineColorBlendStateCreateInfo cb;
    memset(&cb, 0, sizeof(cb));
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    // no blend, write out all of rgba
    VkPipelineColorBlendAttachmentState att;
    memset(&att, 0, sizeof(att));
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
        m_window->crash("Failed to create graphis pipeline");

    if (vertShaderModule)
        m_devFuncs->vkDestroyShaderModule(dev, vertShaderModule, nullptr);
    if (fragShaderModule)
        m_devFuncs->vkDestroyShaderModule(dev, fragShaderModule, nullptr);

    if (m_monteCarloRender) m_monteCarloRender->setPipeline(name);
}

void Render::resetMeshTransform() {
    m_meshRotation = QQuaternion(1.0, QVector3D(0.0, 0.0, 0.0));
    m_meshPosition = QVector3D(0.0, 0.0, 0.0);
    m_meshScale = 1.3f;

    m_proj = m_window->clipCorrectionMatrix(); // adjust for Vulkan-OpenGL clip space differences
    const QSize sz = m_window->swapChainImageSize();
    m_proj.perspective(45.0, sz.width() / (float) sz.height(), 0.01f, 100.0f);
    m_proj.translate(0, 0, m_zoom);

    if (m_monteCarloRender) m_monteCarloRender->clear();
    m_window->updateMouseLabel();
}

void Render::setReferenceImage(const QString &path, bool resetAnisotropySize) {
    delete m_reference;
    m_reference = new Texture(path, m_window);
    for (int i = 0; i < m_window->concurrentFrameCount(); ++i) {
        std::array<VkDescriptorImageInfo, 1> imageInfo{};
        imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo[0].imageView = m_reference->getImageView();
        imageInfo[0].sampler = m_reference->getTextureSampler();

        std::array<VkWriteDescriptorSet, 1> descWrites{};
        descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrites[0].dstSet = m_descSet[i];
        descWrites[0].dstBinding = 4;
        descWrites[0].dstArrayElement = 0;
        descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descWrites[0].descriptorCount = 1;
        descWrites[0].pImageInfo = &imageInfo[0];

        m_devFuncs->vkUpdateDescriptorSets(m_window->device(), 1, descWrites.data(), 0, nullptr);
    }

    if (resetAnisotropySize) {
        newAnisoDirTexture(m_reference->getWidth(), m_reference->getHeight());
    }
}

void Render::setAnisoValues(float roughness, float anisotropy) {
    m_matAnisotropy = anisotropy;
    m_roughness = roughness;
    m_anisotropy->setAnisoValues(roughness, anisotropy);
    if (m_monteCarloRender) m_monteCarloRender->clear();
}

void Render::setAnisoDirTexture(const QString &path) {
    m_anisotropy->setAnisoDirTexture(path);
    updateAnisotropyTextureDescriptor();
    m_optimizer->setDirectionTexture(m_anisotropy->getDir());
    if (m_monteCarloRender) m_monteCarloRender->clear();
    m_window->updateMouseLabel();
}

void Render::newAnisoDirTexture(uint32_t width, uint32_t heigth) {
    m_anisotropy->newAnisoDirTexture(width, heigth);
    updateAnisotropyTextureDescriptor();
    m_optimizer->setDirectionTexture(m_anisotropy->getDir());
    if (m_monteCarloRender) m_monteCarloRender->clear();
    m_window->updateMouseLabel();
}

bool Render::setMesh(const QString &path) {
    delete m_mesh;
    m_mesh = new Mesh(path, m_window);
    if (!m_mesh->isValid()) {
        delete m_mesh;
        m_mesh = nullptr;
    }
    resetMeshTransform();
    if (m_monteCarloRender) m_monteCarloRender->clear();
    return m_mesh;
}

void Render::unloadMesh() {
    delete m_mesh;
    m_mesh = nullptr;
    resetMeshTransform();
    if (m_monteCarloRender) m_monteCarloRender->clear();
}

void Render::setAnisoAngleTexture(const QString &path, bool half) {
    m_anisotropy->setAnisoAngleTexture(path, half);
    updateAnisotropyTextureDescriptor();
    m_optimizer->setDirectionTexture(m_anisotropy->getDir());
    if (m_monteCarloRender) m_monteCarloRender->clear();
    m_window->updateMouseLabel();
}

void Render::setAlbedo(float r, float g, float b) {
    m_albedo[0] = r;
    m_albedo[1] = g;
    m_albedo[2] = b;
    if (m_monteCarloRender) m_monteCarloRender->clear();
}

void Render::setExposure(float val) {
    m_exposure = val;
    if (m_monteCarloRender) m_monteCarloRender->clear();
}

void Render::showConstraints(bool val) {
    m_showConstraints = val;
}

bool Render::getShowConstraints() {
    return m_showConstraints;
}

void Render::createQuadData() {
    VkDevice dev = m_window->device();

    float data[] = {1.0, 0.0, 0.0, -1.0, 1.0, 1.0, 0.0, 0.0, 0.0, -1.0, 1.0, 0.0,
                    1.0, 0.0, 0.0, -1.0, 1.0, -1.0, 0.0, 0.0, 0.0, -1.0, 1.0, 1.0,
                    1.0, 0.0, 0.0, -1.0, -1.0, -1.0, 0.0, 0.0, 0.0, -1.0, 0.0, 1.0,
                    1.0, 0.0, 0.0, -1.0, 1.0, 1.0, 0.0, 0.0, 0.0, -1.0, 1.0, 0.0,
                    1.0, 0.0, 0.0, -1.0, -1.0, -1.0, 0.0, 0.0, 0.0, -1.0, 0.0, 1.0,
                    1.0, 0.0, 0.0, -1.0, -1.0, 1.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0};

    VkBufferCreateInfo meshBufInfo;
    memset(&meshBufInfo, 0, sizeof(meshBufInfo));
    meshBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    meshBufInfo.size = sizeof(data);
    meshBufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VkResult err = m_devFuncs->vkCreateBuffer(dev, &meshBufInfo, nullptr, &m_quadBuf);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to create buffer");

    VkMemoryRequirements meshMemReq;
    m_devFuncs->vkGetBufferMemoryRequirements(dev, m_quadBuf, &meshMemReq);

    VkMemoryAllocateInfo meshMemAllocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        meshMemReq.size,
        m_window->findMemoryType(meshMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    };

    err = m_devFuncs->vkAllocateMemory(dev, &meshMemAllocInfo, nullptr, &m_quadBufMem);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to allocate memory");

    err = m_devFuncs->vkBindBufferMemory(dev, m_quadBuf, m_quadBufMem, 0);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to bind buffer memory");

    void *p;
    err = m_devFuncs->vkMapMemory(dev, m_quadBufMem, 0, meshMemReq.size, 0, &p);
    if (err != VK_SUCCESS)
        m_window->crash("Failed to map memory");

    memcpy(p, data, sizeof(data));
    m_devFuncs->vkUnmapMemory(dev, m_quadBufMem);
}

void Render::updateRotation(float x, float y) {
    m_meshRotation = QQuaternion::fromEulerAngles(y, x, 0)*m_meshRotation;
    if (m_monteCarloRender) m_monteCarloRender->clear();
}

void Render::updatePosition(float x, float y) {
    m_meshPosition += QVector3D(x, y, 0);
    if (m_monteCarloRender) m_monteCarloRender->clear();
}

void Render::updateScale(float w) {
    m_meshScale *= w;
    m_meshPosition *= w;
    if (m_monteCarloRender) m_monteCarloRender->clear();
    m_window->updateMouseLabel();
}

void Render::setMetallic(float val) {
    m_metallic = val;
    if (m_monteCarloRender) m_monteCarloRender->clear();
}

float Render::getScale() {
    return m_meshScale;
}

void Render::saveAnisoDir(const QString &path) {
    m_anisotropy->saveAnisoDir(path);
}

void Render::saveAnisoAngle(const QString &path) {
    m_anisotropy->saveAnisoAngle(path);
}

void Render::saveZebra(const QString &path) {
    m_anisotropy->saveZebra(path);
}

void Render::setSampleCount(uint32_t val) {
    m_sampleCount = val;
}

bool Render::mouseToUV(QPointF mousePosition, QPointF &hitUV) {
    QMatrix4x4 view;
    view.translate(0, 0, m_zoom);
    QMatrix4x4 projection = m_proj;
    projection.translate(0, 0, -m_zoom);

    QVector4D rayClip = QVector4D(2*mousePosition.x()/m_window->width()-1, 2*mousePosition.y()/m_window->height()-1, -1, 1);
    QVector4D rayEye = projection.inverted() * rayClip;
    rayEye = QVector4D(rayEye.x(), rayEye.y(), -1.0f, 0.0f);
    QVector4D rayWS = (view.inverted() * rayEye);
    QVector3D direction = QVector3D(rayWS.x(), rayWS.y(), rayWS.z()).normalized();
    QVector3D normal = m_meshRotation*QVector3D(0, 0, 1);

    QVector3D origin = QVector3D(0, 0, -m_zoom);
    float t = QVector3D::dotProduct(m_meshPosition-origin, normal)/QVector3D::dotProduct(direction, normal);
    QVector3D hit = origin + t * direction;

    QVector3D res = m_meshRotation.inverted() * (hit-m_meshPosition) / m_meshScale;
    hitUV.setX(res.x() * 0.5f + 0.5f);
    hitUV.setY(res.y() * 0.5f + 0.5f);

    return true;
}

void Render::updateConstraintPreview(QPointF last, QPointF current) {
    m_lastDown = last;
    m_currentDown = current;
}


