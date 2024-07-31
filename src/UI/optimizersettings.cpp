#include "optimizersettings.h"
#include "src/UI/vulkanwindow.h"
#include "src/Field/optimizer.h"

#include <QWidget>
#include <QSpinBox>
#include <QComboBox>
#include <QVBoxLayout>
#include <QCheckBox>

OptimizerSettings::OptimizerSettings(VulkanWindow *window): m_window(window) {
    setWindowTitle("Optimizer settings");
    setMinimumWidth(320);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(QMargins(10,10,10,10));

    QComboBox *method = new QComboBox();
    layout->addWidget(new QLabel("Method: "));
    layout->addWidget(method);
    method->addItem("Tensor field - vector fallback");
    method->addItem("Tensor field - no fallback");
    method->addItem("Vector field - alternating neighborhood");
    method->addItem("Vector field - linear neighborhood");
    QObject::connect(method, &QComboBox::currentIndexChanged, [&](int val){
        m_window->getOptimizer()->setMethod(static_cast<Optimizer::Method>(val));
    });

    QCheckBox *newPhase = new QCheckBox("Knöppel et al. 2015 phase alignment method");
    layout->addWidget(newPhase);
    newPhase->setChecked(true);
    QObject::connect(newPhase, &QCheckBox::stateChanged, [&](bool val){
        m_window->getOptimizer()->setNewPhaseMethod(val);
    });

    QCheckBox *onMove = new QCheckBox("Optimize on move");
    layout->addWidget(onMove);
    onMove->setChecked(true);
    QObject::connect(onMove, &QCheckBox::stateChanged, [&](bool val){
        m_window->setOptimizeOnMove(val);
    });

    QWidget *iterationWidget = new QWidget();
    layout->addWidget(iterationWidget);
    QHBoxLayout *layoutIteration = new QHBoxLayout;
    layoutIteration->setContentsMargins(QMargins(0,0,0,0));
    iterationWidget->setLayout(layoutIteration);
    QSpinBox *iteration = new QSpinBox(this);
    iteration->setRange(2, 65536);
    iteration->setValue(64);
    layoutIteration->addWidget(new QLabel("Iteration: "));
    layoutIteration->addWidget(iteration);
    QObject::connect(iteration, &QSpinBox::valueChanged, [&](int val){
        m_window->getOptimizer()->setIteration(val);
    });

    QWidget *iterationOnMoveWidget = new QWidget();
    layout->addWidget(iterationOnMoveWidget);
    QHBoxLayout *layoutOnMoveIteration = new QHBoxLayout;
    layoutOnMoveIteration->setContentsMargins(QMargins(0,0,0,0));
    iterationOnMoveWidget->setLayout(layoutOnMoveIteration);
    QSpinBox *iterationOnMove = new QSpinBox(this);
    iterationOnMove->setRange(2, 4096);
    iterationOnMove->setValue(16);
    layoutOnMoveIteration->addWidget(new QLabel("Iteration on move: "));
    layoutOnMoveIteration->addWidget(iterationOnMove);
    QObject::connect(iterationOnMove, &QSpinBox::valueChanged, [&](int val){
        m_window->getOptimizer()->setIterationOnMove(val);
    });

    QWidget *angleWidget = new QWidget();
    layout->addWidget(angleWidget);
    QHBoxLayout *layoutAngle = new QHBoxLayout;
    layoutAngle->setContentsMargins(QMargins(0,0,0,0));
    angleWidget->setLayout(layoutAngle);
    QDoubleSpinBox *angle = new QDoubleSpinBox(this);
    angle->setRange(0, 180);
    angle->setValue(0);
    angle->setWrapping(true);
    angle->setSingleStep(5);
    layoutAngle->addWidget(new QLabel("Angle offset: "));
    layoutAngle->addWidget(angle);
    QObject::connect(angle, &QDoubleSpinBox::valueChanged, [&](float val){
        m_window->getOptimizer()->setAngleOffset(val/180*M_PI);
    });

    setLayout(layout);
}
