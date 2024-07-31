#include "bottombar.h"
#include <QHBoxLayout>
#include <QCoreApplication>

BottomBar::BottomBar() {
    QHBoxLayout *layout = new QHBoxLayout;
    layout->setContentsMargins(QMargins(46,0,203,0));
    layout->setSpacing(0);
    setMaximumHeight(20);
    setLayout(layout);

    m_mouse = new QLabel("");
    layout->addWidget(m_mouse);
    layout->addStretch();

    m_fps = new QLabel("");
    m_fps->setAlignment(Qt::AlignRight);
    layout->addWidget(m_fps);
}

void BottomBar::setFPSLabel(const QString& str) {
    m_fps->setText(str);
}

void BottomBar::setMouseLabel(const QString& str) {
    m_mouse->setText(str);
}
