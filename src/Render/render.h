#pragma once

class Anisotropy;
class Optimizer;
class MonteCarlo;
class BrdfIntegrator;

#include <QVulkanWindow>
#include <QVulkanFunctions>
#include "src/Texture/texture.h"
#include <QElapsedTimer>

class CubeMap;
class Mesh;
class VulkanWindow;
class Skybox;
class ConstraintsRenderer;
class Render : public QVulkanWindowRenderer {
public:
    Render(VulkanWindow *w, Optimizer *&o, const std::vector<std::array<float, 2>> &pointList, bool msaa = false);

    void initResources() override;
    void initSwapChainResources() override;
    void releaseResources() override;
    void startNextFrame() override;

    void setReferenceImage(const QString &path, bool resetAnisotropySize = false);
    void updateRotation(float x, float y);
    void updateScale(float w);
    float getScale();
    void updatePosition(float x, float y);
    void resetMeshTransform();

    void setPipeline(const std::string &name, bool montecarlo = false);
    void setAnisoDirTexture(const QString &path);
    void newAnisoDirTexture(uint32_t width, uint32_t heigth);
    void setAnisoAngleTexture(const QString &path, bool half = false);
    void setAnisoValues(float roughness, float anisotropy);
    void setMetallic(float val);
    void setAlbedo(float r, float g, float b);
    void setExposure(float val);
    void showConstraints(bool val);
    bool getShowConstraints();
    void setSampleCount(uint32_t val);
    bool setMesh(const QString &path);
    void unloadMesh();

    void saveAnisoDir(const QString &path);
    void saveZebra(const QString &path);
    void saveAnisoAngle(const QString &path);
    void updateAnisotropyTextureDescriptor();

    bool mouseToUV(QPointF mousePosition, QPointF &hitUV);
    void updateConstraintPreview(QPointF last, QPointF current);

protected:
    void createPipelineLayout();
    void createUniformBuffer();
    void createDescriptors();
    void createQuadData();

    VulkanWindow *m_window;
    QVulkanDeviceFunctions *m_devFuncs;

    VkDeviceMemory m_uniformBufMem = VK_NULL_HANDLE;
    VkBuffer m_uniformBuf = VK_NULL_HANDLE;
    VkDescriptorBufferInfo m_uniformBufInfo[QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT];

    VkDescriptorPool m_descPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_descSet[QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT];

    VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;

    QQuaternion m_meshRotation;
    QVector3D m_meshPosition;
    float m_meshScale;
    Mesh *m_mesh = nullptr;

    QMatrix4x4 m_proj;
    const float m_zoom = -4;
    float m_metallic = 1.0f;
    float m_matAnisotropy = 0.70;
    float m_roughness = 0.5;
    float m_albedo[3]{1.0, 0.6, 0.3};
    float m_monteCarlo = 0.0;
    float m_exposure = 1.5;
    uint32_t m_sampleCount = 32;

    Anisotropy *m_anisotropy = nullptr;
    Texture *m_reference = nullptr;
    ConstraintsRenderer *m_constraints = nullptr;
    bool m_showConstraints = true;
    Optimizer *&m_optimizer;
    const std::vector<std::array<float, 2>> &m_pointList;
    MonteCarlo *m_monteCarloRender = nullptr;
    CubeMap *m_cubeMap = nullptr;
    VkDeviceMemory m_quadBufMem = VK_NULL_HANDLE;
    VkBuffer m_quadBuf = VK_NULL_HANDLE;

    QPointF m_currentDown;
    QPointF m_lastDown;

    QElapsedTimer m_timer;
    uint32_t m_frameCount = 0;
};
