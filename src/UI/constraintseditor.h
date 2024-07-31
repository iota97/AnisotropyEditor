#pragma once
#include "vulkanwindow.h"
#include <QWidget>

class ConstraintsEditor : public QWidget {
public:
    ConstraintsEditor(VulkanWindow *vulkanWindow);
};
