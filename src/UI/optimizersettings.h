#pragma once
#include <QWidget>
#include <QColorDialog>
#include <QLabel>
#include "src/UI/vulkanwindow.h"

class OptimizerSettings: public QWidget {
public:
    OptimizerSettings(VulkanWindow *window);

private:
    VulkanWindow *m_window;
};
