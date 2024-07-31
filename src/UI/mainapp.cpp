#include "mainapp.h"
#include <QVBoxLayout>
#include "src/UI/constraintseditor.h"

void MainApp::closeEvent(QCloseEvent *event) {
    m_menuBar->close(event);
}

BottomBar *MainApp::getBottomBar() {
    return m_bottomBar;
}

void MainApp::init(VulkanWindow *vulkanWindow) {
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(QMargins(0,0,0,0));
    layout->setSpacing(0);

    m_menuBar = new MenuBar(vulkanWindow);
    layout->addWidget(m_menuBar);

    QWidget *widgetHor = new QWidget;
    QHBoxLayout *layoutHor = new QHBoxLayout;
    layoutHor->setContentsMargins(QMargins(0,0,0,0));
    layoutHor->setSpacing(0);
    widgetHor->setLayout(layoutHor);
    layout->addWidget(widgetHor, 1);

    m_toolBar = new ToolBar(vulkanWindow);
    layoutHor->addWidget(m_toolBar);
    layoutHor->addWidget(QWidget::createWindowContainer(vulkanWindow), 1);
    m_constraintBar = new ConstraintsEditor(vulkanWindow);
    layoutHor->addWidget(m_constraintBar);

    m_bottomBar = new BottomBar;
    layout->addWidget(m_bottomBar);
    setWindowTitle("Tangent Field Editor");
    setLayout(layout);
}

void MainApp::dirtyUndoMenu() {
    m_menuBar->dirtyUndoMenu();
}

void MainApp::setSideBarsVisible(bool val) {
    m_toolBar->setVisible(val);
    m_constraintBar->setVisible(val);
}


void MainApp::dirtyToolBar() {
    m_toolBar->dirty();
}

