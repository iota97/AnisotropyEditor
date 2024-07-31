#include "toolbar.h"
#include <QVBoxLayout>
#include <QCoreApplication>

ToolBar::ToolBar(VulkanWindow *vulkanWindow) {
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(QMargins(2,2,2,2));
    layout->setSpacing(1);
    setLayout(layout);
    setMaximumWidth(44);

    m_select = new QPushButton("");
    layout->addWidget(m_select);
    m_select->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/select-selected.png"));
    m_select->setToolTip("Edit - Q");
    m_select->setShortcut(Qt::Key_Q);
    m_select->setFixedSize(QSize(40, 40));
    m_select->setIconSize(QSize(27, 27));

    QPushButton *line = new QPushButton("");
    layout->addWidget(line);
    line->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/line.png"));
    line->setToolTip("Line - W");
    line->setShortcut(Qt::Key_W);
    line->setFixedSize(QSize(40, 40));
    line->setIconSize(QSize(32, 32));

    QPushButton *ellipse = new QPushButton("");
    layout->addWidget(ellipse);
    ellipse->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/ellipse.png"));
    ellipse->setToolTip("Ellipse - E");
    ellipse->setShortcut(Qt::Key_E);
    ellipse->setFixedSize(QSize(40, 40));
    ellipse->setIconSize(QSize(32, 32));

    QPushButton *rectangle = new QPushButton("");
    layout->addWidget(rectangle);
    rectangle->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/rectangle.png"));
    rectangle->setToolTip("Rectangle - R");
    rectangle->setShortcut(Qt::Key_R);
    rectangle->setFixedSize(QSize(40, 40));
    rectangle->setIconSize(QSize(32, 32));

    QObject::connect(m_select, &QPushButton::pressed, [=](){
        vulkanWindow->switchTool(VulkanWindow::Tools::Select);
        m_select->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/select-selected.png"));
        rectangle->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/rectangle.png"));
        line->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/line.png"));
        ellipse->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/ellipse.png"));
    });
    QObject::connect(rectangle, &QPushButton::pressed, [=](){
        vulkanWindow->switchTool(VulkanWindow::Tools::Rectangle);
        m_select->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/select.png"));
        rectangle->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/rectangle-selected.png"));
        line->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/line.png"));
        ellipse->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/ellipse.png"));
    });
    QObject::connect(line, &QPushButton::pressed, [=](){
        vulkanWindow->switchTool(VulkanWindow::Tools::Line);
        m_select->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/select.png"));
        rectangle->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/rectangle.png"));
        line->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/line-selected.png"));
        ellipse->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/ellipse.png"));
    });
    QObject::connect(ellipse, &QPushButton::pressed, [=](){
        vulkanWindow->switchTool(VulkanWindow::Tools::Ellipse);
        m_select->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/select.png"));
        rectangle->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/rectangle.png"));
        line->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/line.png"));
        ellipse->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/assets/icons/ellipse-selected.png"));
    });

    layout->addStretch();
}

void ToolBar::dirty() {
    m_select->click();
}
