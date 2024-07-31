#pragma once

#include <QWidget>
#include <QMenuBar>
#include <QColorDialog>
#include "src/UI/vulkanwindow.h"
#include "src/UI/menubar.h"
#include "src/UI/toolbar.h"
#include "src/UI/bottombar.h"
#include "src/UI/constraintseditor.h"

class MainApp : public QWidget {
public:
    void init(VulkanWindow *vulkanWindow);
    void closeEvent(QCloseEvent *event) override;
    void dirtyUndoMenu();
    void dirtyToolBar();
    void setSideBarsVisible(bool val);
    BottomBar *getBottomBar();

private:
    MenuBar *m_menuBar;
    ToolBar *m_toolBar;
    BottomBar *m_bottomBar;
    ConstraintsEditor *m_constraintBar;
};
