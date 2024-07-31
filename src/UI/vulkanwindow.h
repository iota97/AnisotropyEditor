#pragma once
#include <QVulkanWindow>
#include <QElapsedTimer>
#include "src/Render/render.h"

class MainApp;
class Optimizer;
class QListWidget;
class VulkanWindow : public QVulkanWindow {
public:
    VulkanWindow(MainApp *mainApp);

    QVulkanWindowRenderer *createRenderer() override;
    Render *getRender();
    Optimizer *getOptimizer();
    MainApp *getMainApp();

    enum Tools {
        Select,
        Rectangle,
        Line,
        Ellipse,
        None,
    };

    void setMainAppTitle(const QString &title);
    void switchTool(Tools tool, bool force = false);
    Tools getTool();
    bool getOptimizeOnMove();
    void setOptimizeOnMove(bool val);
    int32_t getSelectedConstraint();
    int32_t getSelectedLine();

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

    void setComputeQueueFamilyIndex(uint32_t ind);
    uint32_t getComputeQueueFamilyIndex();
    void clearDownKeys();

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkShaderModule createShader(const QString &name);
    VkDeviceSize aligned(VkDeviceSize v, VkDeviceSize byteAlign);
    QListWidget *&getConstraintWidgetList();
    void crash(const QString& msg);

    void dirtyUndoMenu();
    void setFPSLabel(const QString& str);
    void updateMouseLabel();

private:
    int32_t getConstraint(QPointF uv, bool doubleClick);
    bool tryPick(uint32_t id, QPointF uv);
    float uvToAngle(QPointF uv, int32_t id);

    QPointF m_lastDragPosition;
    QPointF m_lastPressPosition;
    QPointF m_lastDown;

    Render *m_renderer = nullptr;
    Optimizer *m_optimizer = nullptr;
    uint32_t m_downKeys = 0;
    uint32_t m_computeQueueFamilyIndex;

    enum {
        LeftMouseButton = 1 << 0,
        RightMouseButton = 1 << 1,
        MiddleMouseButton = 1 << 2,
        Ctrl = 1 << 3,
        Shift = 1 << 4,
    };

    Tools m_tool = Select;
    MainApp *m_mainApp;

    enum {
        Move,
        Rotate,
        DragPX,
        DragNX,
        DragPY,
        DragNY,
        MovePoint,
    };
    int32_t m_selectedConstraint = -1;
    int32_t m_selectedLine = -1;
    uint32_t m_editAction = Move;
    bool m_optimizeOnMove = true;
    float m_actionStartData[7];
    QListWidget *m_constraintWidgetList;
    std::vector<std::array<float, 2>> m_pointList;
    QElapsedTimer m_doubleClickTimer;
    uint64_t m_lastPressTime = 0;
};
