#pragma once
#include <QMenuBar>
#include <QTextEdit>
#include <QFileSystemWatcher>
#include <QTimer>
#include "vulkanwindow.h"
#include "src/UI/renderingsettings.h"
#include "src/UI/optimizersettings.h"
#include "src/UI/newproject.h"

class MenuBar : public QMenuBar {
public:
    MenuBar(VulkanWindow *vulkanWindow);
    ~MenuBar();
    void close(QCloseEvent *event);
    void dirtyUndoMenu();

private:
    QString getFileDir();

    QTextEdit *m_helpTextEdit;
    RenderingSettings *m_renderingSettings;
    OptimizerSettings *m_optimizerSettings;

    VulkanWindow *m_window = nullptr;
    QAction *m_undo = nullptr;
    QAction *m_redo = nullptr;
    NewProject *m_newProject = nullptr;
    QFileSystemWatcher m_inputImageWatcher;
    QTimer m_imageLoadTimer;
    QString m_imageFileName;
    QString m_fileDir = "";
    bool m_inputImageIsDirection = false;
    bool m_inputImageIsHalf = false;
    bool m_hotReload = false;
};
