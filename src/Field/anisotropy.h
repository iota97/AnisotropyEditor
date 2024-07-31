#pragma once
#include "src/UI/vulkanwindow.h"
#include "src/Texture/texture.h"
class MonteCarloReference;

class Anisotropy {
public:
    Anisotropy(VulkanWindow *window, MonteCarlo*& monteCarlo);
    ~Anisotropy();

    Anisotropy(const Anisotropy&) = delete;
    Anisotropy(const Anisotropy&&) = delete;
    Anisotropy& operator=(const Anisotropy&) = delete;
    Anisotropy& operator=(const Anisotropy&&) = delete;

    Texture* getDir();
    Texture* getMap();

    void newAnisoDirTexture(uint32_t width, uint32_t heigth);
    void setAnisoDirTexture(const QString &path);
    void setAnisoAngleTexture(const QString &path, bool half);
    void updateAnisotropyTextureMap();
    void setAnisoValues(float roughness, float anisotropy);
    void saveZebra(const QString &path);
    void saveAnisoDir(const QString &path);
    void saveAnisoAngle(const QString &path);

private:
    void createComputePipelineLayout();
    void createComputeDescriptorSet();
    void createComputeUniformBuffer();
    void createDir2CovPipeline();
    void createDir2DirPipeline();
    void createDir2ZebraPipeline();
    void createExportPipeline();
    void createAngle2DirPipeline();
    void createHalfAngle2DirPipeline();

    void updateAnisotropyTextureDescriptor();
    void convertAnisoAngleTexture(bool half);

    VulkanWindow *m_window;
    QVulkanDeviceFunctions *m_devFuncs;

    VkCommandPool m_computeCommandPool = VK_NULL_HANDLE;

    VkPipelineLayout m_computePipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_dir2CovPipeline = VK_NULL_HANDLE;
    VkPipeline m_dir2ZebraPipeline = VK_NULL_HANDLE;
    VkPipeline m_exportPipeline = VK_NULL_HANDLE;
    VkPipeline m_dir2DirPipeline = VK_NULL_HANDLE;
    VkPipeline m_angle2DirPipeline = VK_NULL_HANDLE;
    VkPipeline m_halfAngle2DirPipeline = VK_NULL_HANDLE;

    VkDescriptorSet m_computeDescSet = VK_NULL_HANDLE;
    VkDescriptorPool m_computeDescPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_computeDescSetLayout = VK_NULL_HANDLE;

    VkDeviceMemory m_computeUniformBufMem = VK_NULL_HANDLE;
    VkBuffer m_computeUniformBuf = VK_NULL_HANDLE;
    VkDescriptorBufferInfo m_computeUniformBufInfo;

    Texture* m_anisoMap = nullptr;
    Texture* m_anisoDir = nullptr;
    MonteCarlo*& m_monteCarlo;
};

