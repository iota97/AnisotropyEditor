#pragma once
#include <QWidget>
#include <QColorDialog>
#include <QLabel>
#include "src/UI/vulkanwindow.h"

class RenderingSettings: public QWidget {
public:
    RenderingSettings(VulkanWindow *window);
    ~RenderingSettings();

private:
    QColorDialog *m_colorDialog;
    VulkanWindow *m_window;

    float m_anisotropy = 0.7;
    QLabel *m_anisoLabel;
    float m_roughness = 0.2;
    QLabel *m_roughLabel;
};
