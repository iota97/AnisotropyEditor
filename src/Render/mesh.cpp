#include "mesh.h"
#include "objloader.h"
#include <QVulkanFunctions>
#include "src/UI/vulkanwindow.h"

Mesh::Mesh(const QString& path, VulkanWindow *w) : m_window(w) {
    ObjLoader loader;
#ifdef _WIN32
    loader.load(path.toStdWString().c_str());
#else
    loader.load(path.toStdString().c_str());
#endif

    if (!loader.hasNormals() || !loader.hasTangents()) {
        m_isValid = false;
        return;
    }

    const float* vertexData = loader.getPositions();
    const float* normalData = loader.getNormals();
    const float* texcoordData = loader.getTexCoords();
    const float* tangentData = loader.getTangents();

    std::vector<Vertex> vert;
    for (size_t i = 0; i < loader.getVertCount(); i++) {
        Vertex v;
        v.position.x = vertexData[i*3+0];
        v.position.y = vertexData[i*3+1];
        v.position.z = vertexData[i*3+2];

        v.normal.x = normalData[i*3+0];
        v.normal.y = normalData[i*3+1];
        v.normal.z = normalData[i*3+2];

        v.tangent.x = tangentData[i*4+0];
        v.tangent.y = tangentData[i*4+1];
        v.tangent.z = tangentData[i*4+2];
        v.tangent.w = tangentData[i*4+3];

        v.texcoord.x = texcoordData[i*2+0];
        v.texcoord.y = texcoordData[i*2+1];
        vert.push_back(v);
    }
    vertexData = (float*)&vert[0];
    const uint32_t* indexData = loader.getFaces();
    m_indexCount = loader.getIndexCount();
    size_t vertexDataSize = loader.getVertCount()*sizeof(Vertex);
    size_t indexDataSize = m_indexCount*sizeof(uint32_t);

    VkDevice dev = m_window->device();
    m_devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);

    VkBufferCreateInfo meshBufInfo;
    memset(&meshBufInfo, 0, sizeof(meshBufInfo));
    meshBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    m_ibOffset = vertexDataSize;
    meshBufInfo.size = vertexDataSize + indexDataSize;
    meshBufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

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

    quint8 *p;
    err = m_devFuncs->vkMapMemory(dev, m_meshBufMem, 0, meshMemReq.size, 0, reinterpret_cast<void **>(&p));
    if (err != VK_SUCCESS)
        m_window->crash("Failed to map memory");
    memcpy(p, vertexData, vertexDataSize);
    memcpy(p+vertexDataSize, indexData, indexDataSize);
    m_devFuncs->vkUnmapMemory(dev, m_meshBufMem);
}

Mesh::~Mesh() {
    VkDevice dev = m_window->device();
    if (!m_isValid) {
        return;
    }

    m_devFuncs->vkDeviceWaitIdle(dev);
    if (m_meshBuf) {
        m_devFuncs->vkDestroyBuffer(dev, m_meshBuf, nullptr);
        m_meshBuf = VK_NULL_HANDLE;
    }

    if (m_meshBufMem) {
        m_devFuncs->vkFreeMemory(dev, m_meshBufMem, nullptr);
        m_meshBufMem = VK_NULL_HANDLE;
    }
}

Mesh::Mesh(Mesh&& other) {
    m_window = other.m_window;
    m_devFuncs = other.m_devFuncs;
    m_meshBufMem = other.m_meshBufMem;
    m_vbOffset = other.m_vbOffset;
    m_ibOffset = other.m_ibOffset;
    m_indexCount = other.m_indexCount;
    other.m_meshBufMem = VK_NULL_HANDLE;
    other.m_meshBuf = VK_NULL_HANDLE;
}

Mesh& Mesh::operator=(Mesh&& other) {
    VkDevice dev = m_window->device();
    m_devFuncs->vkDeviceWaitIdle(dev);
    if (m_meshBuf) {
        m_devFuncs->vkDestroyBuffer(dev, m_meshBuf, nullptr);
    }

    if (m_meshBufMem) {
        m_devFuncs->vkFreeMemory(dev, m_meshBufMem, nullptr);
    }
    m_window = other.m_window;
    m_devFuncs = other.m_devFuncs;
    m_meshBufMem = other.m_meshBufMem;
    m_vbOffset = other.m_vbOffset;
    m_ibOffset = other.m_ibOffset;
    m_indexCount = other.m_indexCount;
    other.m_meshBufMem = VK_NULL_HANDLE;
    other.m_meshBuf = VK_NULL_HANDLE;

    return *this;
}

void Mesh::draw(VkCommandBuffer cb) {
    m_devFuncs->vkCmdBindVertexBuffers(cb, 0, 1, &m_meshBuf, &m_vbOffset);
    m_devFuncs->vkCmdBindIndexBuffer(cb, m_meshBuf, m_ibOffset, VK_INDEX_TYPE_UINT32);
    m_devFuncs->vkCmdDrawIndexed(cb, m_indexCount, 1, 0, 0, 0);
}

bool Mesh::isValid() {
    return m_isValid;
}
