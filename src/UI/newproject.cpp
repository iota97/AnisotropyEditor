#include "newproject.h"
#include "src/Field/optimizer.h"
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>

NewProject::NewProject(VulkanWindow *vulkanWindow) {
    setWindowTitle("New project");
    setMinimumWidth(240);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(QMargins(10,10,10,10));

    QWidget *widgetSize = new QWidget;
    QHBoxLayout *layoutSize = new QHBoxLayout;
    layoutSize->setContentsMargins(QMargins(0,0,0,0));
    widgetSize->setLayout(layoutSize);
    layout->addWidget(widgetSize);
    QSpinBox *width = new QSpinBox(this);
    width->setRange(1, 1<<16);
    width->setValue(1024);
    layoutSize->addWidget(new QLabel("Width: "));
    layoutSize->addWidget(width, 1);
    QSpinBox *height = new QSpinBox(this);
    height->setRange(1, 1<<16);
    height->setValue(1024);
    layoutSize->addWidget(new QLabel("Height: "));
    layoutSize->addWidget(height, 1);

    QWidget *widgetButton = new QWidget;
    QHBoxLayout *layoutButton = new QHBoxLayout;
    layoutButton->setContentsMargins(QMargins(0,0,0,0));
    widgetButton->setLayout(layoutButton);
    layout->addWidget(widgetButton);

    QPushButton *newTexture = new QPushButton("Accept");
    QObject::connect(newTexture, &QPushButton::clicked, [=](){
        vulkanWindow->clearDownKeys();
        vulkanWindow->getRender()->newAnisoDirTexture(width->value(), height->value());
        vulkanWindow->getOptimizer()->clear();
        vulkanWindow->getRender()->resetMeshTransform();
        close();
    });
    layoutButton->addWidget(newTexture);

    QPushButton *cancel = new QPushButton("Cancel");
    QObject::connect(cancel, &QPushButton::clicked, [=](){
        close();
    });
    layoutButton->addWidget(cancel);
    setLayout(layout);
}
