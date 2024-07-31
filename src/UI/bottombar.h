#pragma once
#include <QWidget>
#include <QLabel>

class BottomBar : public QWidget {
public:
    BottomBar();
    void setFPSLabel(const QString& str);
    void setMouseLabel(const QString& str);

private:
    QLabel *m_fps;
    QLabel *m_mouse;
};
