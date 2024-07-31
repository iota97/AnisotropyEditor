#pragma once
#include "src/UI/vulkanwindow.h"
#include "src/Texture/texture.h"
#include "src/Field/anisotropy.h"
#include <QListWidget>
#include <deque>

class Optimizer {
public:
    Optimizer(VulkanWindow *window, Anisotropy *anisotropy);
    ~Optimizer();

    void addRectangle(float x0, float y0, float x1, float y1);
    void addEllipse(float x0, float y0, float x1, float y1);
    void addLine(const std::vector<std::array<float, 2>> &pointList);

    void removeRectangle(uint32_t id);
    void setAlignPhase(uint32_t id, bool val);
    void rotateRectangle(uint32_t id, float angle, bool opt = true);
    void fillRectangle(uint32_t id, float angle, bool opt = true);
    void moveRectangleX(uint32_t id, float x, bool opt = true);
    void moveRectangleY(uint32_t id, float y, bool opt = true);
    void scaleRectangleX(uint32_t id, float x, bool opt = true);
    void scaleRectangleY(uint32_t id, float y, bool opt = true);
    void skewRectangleX(uint32_t id, float x);
    void skewRectangleY(uint32_t id, float y);
    void duplicateRectangle(uint32_t id);
    void movePoint(uint32_t id, uint32_t offset, float x, float y);
    void addPoint(uint32_t id, uint32_t offset, float x, float y);
    void setAngleOffset(float val);

    enum Method {
        VectorAlternating = 0,
        Vector = 1,
        Tensor = 2,
    };
    void setIteration(uint32_t val);
    void setIterationOnMove(uint32_t val);
    void setMethod(Method val);
    void setNewPhaseMethod(bool val);

    struct Constraint {
        float centerX;
        float centerY;
        float width;
        float height;
        float dirX;
        float dirY;
        float rotAngle;
        float skewH;
        float skewV;
        float lineCenterX;
        float lineCenterY;
        float lineWidth;
        float lineHeight;
        float tesselationLod;
        float alignPhases;
        uint32_t id;
        struct Line { float x0, y0, x1, y1; };
        std::vector<Line> lines;
    };

    const std::vector<Constraint>& getConstraints();

    void optimize();
    bool canUndo();
    bool canRedo();
    void revertChange();
    void redoChange();
    void commitChange();
    void clear();

    void save(const QString &path);
    void load(const QString &path);
    void setDirectionTexture(Texture *tex);

    uint32_t getWidth();
    uint32_t getHeight();

private:
    void createComputeDescriptorSet();
    void createComputePipelineLayout();
    void createPipelineLayout();
    void createLinePipeline();

    void createOptimizeSmoothPipeline();
    void createOptimizeProlongPipeline();
    void createOptimizeRestrictPipeline();
    void createOptimizeFinalizePipeline();
    void createOptimizeInitPipeline();
    void createComputeUniformBuffer();
    void createRenderPass();
    void createMeshData();

    void optimize(uint32_t iteration);
    void optimizeInit();
    void optimizeInitTexture();
    void optimizeFinalize();
    void optimizeSmooth(uint32_t targetLod, uint32_t iterations);
    void optimizeRestrict(uint32_t destLod);
    void optimizeProlong(uint32_t destLod);

    void bakeLine(Constraint &rect);
    void recreateLineAABB(Constraint &rect);

    VulkanWindow *m_window;
    QVulkanDeviceFunctions *m_devFuncs;

    VkCommandPool m_computeCommandPool = VK_NULL_HANDLE;

    VkPipelineLayout m_computePipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_optimizeSmoothPipeline = VK_NULL_HANDLE;
    VkPipeline m_optimizeFinalizePipeline = VK_NULL_HANDLE;
    VkPipeline m_optimizeProlongPipeline = VK_NULL_HANDLE;
    VkPipeline m_optimizeRestrictPipeline = VK_NULL_HANDLE;
    VkPipeline m_optimizeInitPipeline = VK_NULL_HANDLE;

    VkDeviceMemory m_computeUniformBufMem = VK_NULL_HANDLE;
    VkBuffer m_computeUniformBuf = VK_NULL_HANDLE;
    VkDescriptorBufferInfo m_computeUniformBufInfo;

    VkDescriptorPool m_computeDescPool = VK_NULL_HANDLE;
    VkDescriptorSet m_computeDescSet[2]{};
    VkDescriptorSetLayout m_computeDescSetLayout[2]{};

    Texture *m_buffer[2]{};

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkFramebuffer m_frameBuffer = VK_NULL_HANDLE;
    Texture *m_depth = nullptr;
    Texture *m_frameImage = nullptr;

    VkDeviceMemory m_meshBufMem = VK_NULL_HANDLE;
    VkBuffer m_meshBuf = VK_NULL_HANDLE;

    VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_linePipeline = VK_NULL_HANDLE;

    Anisotropy *m_anisotropy;
    Texture *m_directionBackup = nullptr;

    std::vector<Constraint> m_constraints;
    QListWidget *m_listWidget = nullptr;

    uint32_t m_dynamicAlignment;
    static constexpr uint32_t MAX_CONSTRAINTS = 65536;

    std::deque<std::vector<Constraint>> m_changesQueue;
    uint32_t m_currChange = 0;
    uint32_t m_maxUndoCount = 256;

    Method m_optimizationMethod = VectorAlternating;
    bool m_newPhaseMethod = true;
    uint32_t m_iteration = 64;
    uint32_t m_iterationOnMove = 16;
    float m_angleOffset = 0;

    uint32_t m_maxId = 0;
};
