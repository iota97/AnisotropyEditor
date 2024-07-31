#pragma once
#include <QWidget>
#include "vulkanwindow.h"
#include <QPushButton>

class ToolBar : public QWidget {
public:
    ToolBar(VulkanWindow *vulkanWindow);
    void dirty();

private:
    QPushButton *m_select;
};
