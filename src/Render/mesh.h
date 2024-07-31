#pragma once
#include <QVulkanWindow>

class VulkanWindow;
class Mesh {
public:
    struct Vec4 {
        float x; float y; float z; float w;
    };

    struct Vec3 {
        float x; float y; float z;
    };

    struct Vec2 {
        float x; float y;
    };

    struct Vertex {
        Vec4 tangent;
        Vec3 position;
        Vec3 normal;
        Vec2 texcoord;
    };

    Mesh(const QString& path, VulkanWindow *w);
    ~Mesh();
    Mesh(const Mesh&) = delete;
    Mesh(Mesh&& other);
    Mesh& operator=(Mesh&& other);
    Mesh& operator=(const Mesh&) = delete;

    bool isValid();
    void draw(VkCommandBuffer cb);

    static VkPipelineVertexInputStateCreateInfo *getVertexInputInfo();

private:
    VulkanWindow *m_window;
    QVulkanDeviceFunctions *m_devFuncs;

    VkDeviceMemory m_meshBufMem = VK_NULL_HANDLE;
    VkBuffer m_meshBuf = VK_NULL_HANDLE;
    VkDeviceSize m_vbOffset = 0;
    VkDeviceSize m_ibOffset = 0;

    bool m_isValid = true;
    size_t m_indexCount;
};
