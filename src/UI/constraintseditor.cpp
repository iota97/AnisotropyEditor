#include "constraintseditor.h"
#include "src/Field/optimizer.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QLayout>
#include <QListWidget>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QCheckBox>

ConstraintsEditor::ConstraintsEditor(VulkanWindow *vulkanWindow) {
    QVBoxLayout *layoutRightBar = new QVBoxLayout;
    layoutRightBar->setContentsMargins(QMargins(3,3,3,3));
    setLayout(layoutRightBar);
    setMaximumWidth(200);

    vulkanWindow->getConstraintWidgetList() = new QListWidget(this);
    layoutRightBar->addWidget(vulkanWindow->getConstraintWidgetList());

    QLabel *posLabel = new QLabel("Position:");
    posLabel->setEnabled(false);
    layoutRightBar->addWidget(posLabel);
    QWidget *widgetPosition = new QWidget;
    QHBoxLayout *layoutPosition = new QHBoxLayout;
    layoutPosition->setContentsMargins(QMargins(0,0,0,0));
    widgetPosition->setLayout(layoutPosition);
    layoutRightBar->addWidget(widgetPosition);

    QLabel *posXLabel = new QLabel("X");
    posXLabel->setEnabled(false);
    layoutPosition->addWidget(posXLabel);
    QDoubleSpinBox *posX = new QDoubleSpinBox(this);
    posX->setEnabled(false);
    posX->setSingleStep(1);
    posX->setRange(-std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    posX->setDecimals(0);
    QObject::connect(posX, &QDoubleSpinBox::valueChanged, [=](double value){
        int row = vulkanWindow->getConstraintWidgetList()->currentRow();
        if (row >= 0) {
            float w = vulkanWindow->getOptimizer()->getWidth();
            float h = vulkanWindow->getOptimizer()->getHeight();
            vulkanWindow->getOptimizer()->moveRectangleX(row, (value-(w-h)*0.5)/h);
        }
    });
    layoutPosition->addWidget(posX, 1);

    QLabel *posYLabel = new QLabel("Y");
    posYLabel->setEnabled(false);
    layoutPosition->addWidget(posYLabel);
    QDoubleSpinBox *posY = new QDoubleSpinBox(this);
    posY->setEnabled(false);
    posY->setSingleStep(1);
    posY->setRange(-std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    posY->setDecimals(0);
    QObject::connect(posY, &QDoubleSpinBox::valueChanged, [=](double value){
        int row = vulkanWindow->getConstraintWidgetList()->currentRow();
        if (row >= 0) {
            float h = vulkanWindow->getOptimizer()->getHeight();
            vulkanWindow->getOptimizer()->moveRectangleY(row, value/h);
        }
    });
    layoutPosition->addWidget(posY, 1);

    QDoubleSpinBox *angleSpin = new QDoubleSpinBox(this);
    angleSpin->setEnabled(false);
    angleSpin->setWrapping(true);
    angleSpin->setRange(-180, 180);
    angleSpin->setSingleStep(15.0);
    angleSpin->setDecimals(3);
    QLabel *rotLabel = new QLabel("Rotation:");
    rotLabel->setEnabled(false);
    layoutRightBar->addWidget(rotLabel);
    QObject::connect(angleSpin, &QDoubleSpinBox::valueChanged, [=](double value){
        int row = vulkanWindow->getConstraintWidgetList()->currentRow();
        if (row >= 0) {
            vulkanWindow->getOptimizer()->rotateRectangle(row, value/180*M_PI);
        }
    });
    layoutRightBar->addWidget(angleSpin);

    QLabel *scaleLabel = new QLabel("Scale:");
    scaleLabel->setEnabled(false);
    layoutRightBar->addWidget(scaleLabel);
    QWidget *widgetScale = new QWidget;
    QHBoxLayout *layoutScale = new QHBoxLayout;
    layoutScale->setContentsMargins(QMargins(0,0,0,0));
    widgetScale->setLayout(layoutScale);
    layoutRightBar->addWidget(widgetScale);

    QLabel *scaleXLabel = new QLabel("X");
    scaleXLabel->setEnabled(false);
    layoutScale->addWidget(scaleXLabel);
    QDoubleSpinBox *scaleX = new QDoubleSpinBox(this);
    scaleX->setEnabled(false);
    scaleX->setSingleStep(0.05);
    scaleX->setRange(-std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    scaleX->setDecimals(3);
    QObject::connect(scaleX, &QDoubleSpinBox::valueChanged, [=](double value){
        int row = vulkanWindow->getConstraintWidgetList()->currentRow();
        if (row >= 0) {
            vulkanWindow->getOptimizer()->scaleRectangleX(row, value);
        }
    });
    layoutScale->addWidget(scaleX, 1);

    QLabel *scaleYLabel = new QLabel("Y");
    scaleYLabel->setEnabled(false);
    layoutScale->addWidget(scaleYLabel);
    QDoubleSpinBox *scaleY = new QDoubleSpinBox(this);
    scaleY->setEnabled(false);
    scaleY->setSingleStep(0.05);
    scaleY->setRange(-std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    scaleY->setDecimals(3);
    QObject::connect(scaleY, &QDoubleSpinBox::valueChanged, [=](double value){
        int row = vulkanWindow->getConstraintWidgetList()->currentRow();
        if (row >= 0) {
            vulkanWindow->getOptimizer()->scaleRectangleY(row, value);
        }
    });
    layoutScale->addWidget(scaleY, 1);

    QLabel *skewLabel = new QLabel("Skew:");
    skewLabel->setEnabled(false);
    layoutRightBar->addWidget(skewLabel);
    QWidget *widgetSkew = new QWidget;
    QHBoxLayout *layoutSkew = new QHBoxLayout;
    layoutSkew->setContentsMargins(QMargins(0,0,0,0));
    widgetSkew->setLayout(layoutSkew);
    layoutRightBar->addWidget(widgetSkew);

    QLabel *skewXLabel = new QLabel("X");
    skewXLabel->setEnabled(false);
    layoutSkew->addWidget(skewXLabel);
    QDoubleSpinBox *skewX = new QDoubleSpinBox(this);
    skewX->setEnabled(false);
    skewX->setSingleStep(0.05);
    skewX->setRange(-std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    skewX->setDecimals(3);
    QObject::connect(skewX, &QDoubleSpinBox::valueChanged, [=](double value){
        int row = vulkanWindow->getConstraintWidgetList()->currentRow();
        if (row >= 0) {
            vulkanWindow->getOptimizer()->skewRectangleX(row, value);
        }
    });
    layoutSkew->addWidget(skewX, 1);

    QLabel *skewYLabel = new QLabel("Y");
    skewYLabel->setEnabled(false);
    layoutSkew->addWidget(skewYLabel);
    QDoubleSpinBox *skewY = new QDoubleSpinBox(this);
    skewY->setEnabled(false);
    skewY->setSingleStep(0.05);
    skewY->setRange(-std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    skewY->setDecimals(3);
    QObject::connect(skewY, &QDoubleSpinBox::valueChanged, [=](double value){
        int row = vulkanWindow->getConstraintWidgetList()->currentRow();
        if (row >= 0) {
            vulkanWindow->getOptimizer()->skewRectangleY(row, value);
        }
    });
    layoutSkew->addWidget(skewY, 1);

    QDoubleSpinBox *directionSpin = new QDoubleSpinBox(this);
    directionSpin->setEnabled(false);
    directionSpin->setWrapping(true);
    directionSpin->setRange(0, 180);
    directionSpin->setSingleStep(15.0);
    directionSpin->setDecimals(3);
    QLabel *dirLabel = new QLabel("Direction:");
    dirLabel->setEnabled(false);
    layoutRightBar->addWidget(dirLabel);
    QObject::connect(directionSpin, &QDoubleSpinBox::valueChanged, [=](double value){
        int row = vulkanWindow->getConstraintWidgetList()->currentRow();
        if (row >= 0) {
            vulkanWindow->getOptimizer()->fillRectangle(row, value/180*M_PI);
        }
    });
    layoutRightBar->addWidget(directionSpin);

    QWidget *widgetAlign = new QWidget;
    QHBoxLayout *layoutAlign = new QHBoxLayout;
    layoutAlign->setContentsMargins(QMargins(0,0,0,0));
    widgetAlign->setLayout(layoutAlign);
    layoutRightBar->addWidget(widgetAlign);
    QCheckBox *alignPhase = new QCheckBox(this);
    alignPhase->setCheckState(Qt::Checked);
    alignPhase->setEnabled(false);
    QObject::connect(alignPhase, &QCheckBox::stateChanged, [=](bool value){
        int row = vulkanWindow->getConstraintWidgetList()->currentRow();
        if (row >= 0) {
            vulkanWindow->getOptimizer()->setAlignPhase(row, value);
        }
    });
    layoutAlign->addWidget(alignPhase);
    QLabel *alignLabel = new QLabel("Align field to border");
    alignLabel->setEnabled(false);
    layoutAlign->addWidget(alignLabel, 1);

    QPushButton *duplicateSelected = new QPushButton("Duplicate");
    duplicateSelected->setShortcut(Qt::CTRL | Qt::Key_D);
    duplicateSelected->setToolTip("Duplicate (CTRL+D)");
    duplicateSelected->setEnabled(false);
    layoutRightBar->addWidget(duplicateSelected);
    QObject::connect(duplicateSelected, &QPushButton::pressed, [=](){
        int row = vulkanWindow->getConstraintWidgetList()->currentRow();
        if (row >= 0) {
            vulkanWindow->getOptimizer()->duplicateRectangle(row);
            vulkanWindow->getConstraintWidgetList()->setCurrentRow(vulkanWindow->getOptimizer()->getConstraints().size()-1);
        }
    });

    QPushButton *removeSelected = new QPushButton("Remove");
    removeSelected->setShortcut(Qt::Key_Delete);
    removeSelected->setToolTip("Remove (Delete)");
    removeSelected->setEnabled(false);
    layoutRightBar->addWidget(removeSelected);
    QObject::connect(removeSelected, &QPushButton::pressed, [=](){
        int row = vulkanWindow->getConstraintWidgetList()->currentRow();
        if (row >= 0) {
            vulkanWindow->getOptimizer()->removeRectangle(row);
        }
    });

    QObject::connect(vulkanWindow->getConstraintWidgetList(), &QListWidget::currentItemChanged, [=](){
        int row = vulkanWindow->getConstraintWidgetList()->currentRow();
        removeSelected->setEnabled(row >= 0);
        duplicateSelected->setEnabled(row >= 0);

        angleSpin->setEnabled(row >= 0);
        rotLabel->setEnabled(row >= 0);
        if (row >= 0) angleSpin->setValue(vulkanWindow->getOptimizer()->getConstraints()[row].rotAngle/M_PI*180);

        auto &tmp = vulkanWindow->getOptimizer()->getConstraints()[row];
        directionSpin->setEnabled(row >= 0);
        dirLabel->setEnabled(row >= 0);
        if (row >= 0) directionSpin->setValue(atan2(tmp.dirY, tmp.dirX)/M_PI*180);

        float w = vulkanWindow->getOptimizer()->getWidth();
        float h = vulkanWindow->getOptimizer()->getHeight();
        posX->setEnabled(row >= 0);
        posXLabel->setEnabled(row >= 0);
        if (row >= 0) posX->setValue(vulkanWindow->getOptimizer()->getConstraints()[row].centerX*h + (w-h)*0.5);
        posY->setEnabled(row >= 0);
        posYLabel->setEnabled(row >= 0);
        posLabel->setEnabled(row >= 0);
        if (row >= 0) posY->setValue(vulkanWindow->getOptimizer()->getConstraints()[row].centerY*h);

        scaleX->setEnabled(row >= 0);
        scaleXLabel->setEnabled(row >= 0);
        if (row >= 0) scaleX->setValue(vulkanWindow->getOptimizer()->getConstraints()[row].width);
        scaleY->setEnabled(row >= 0);
        scaleYLabel->setEnabled(row >= 0);
        scaleLabel->setEnabled(row >= 0);
        if (row >= 0) scaleY->setValue(vulkanWindow->getOptimizer()->getConstraints()[row].height);

        skewX->setEnabled(row >= 0);
        skewXLabel->setEnabled(row >= 0);
        if (row >= 0) skewX->setValue(vulkanWindow->getOptimizer()->getConstraints()[row].skewH);
        skewY->setEnabled(row >= 0);
        skewYLabel->setEnabled(row >= 0);
        skewLabel->setEnabled(row >= 0);
        if (row >= 0) skewY->setValue(vulkanWindow->getOptimizer()->getConstraints()[row].skewV);

        alignPhase->setEnabled(row >= 0);
        alignLabel->setEnabled(row >= 0);
        if (row >= 0) alignPhase->setCheckState(vulkanWindow->getOptimizer()->getConstraints()[row].alignPhases ? Qt::Checked : Qt::Unchecked);
    });
}
