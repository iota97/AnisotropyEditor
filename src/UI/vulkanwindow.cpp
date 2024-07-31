#include "vulkanwindow.h"
#include <QMouseEvent>
#include <QFile>
#include "src/UI/mainapp.h"
#include <QMessageBox>
#include <QListWidget>
#include "src/Field/optimizer.h"

VulkanWindow::VulkanWindow(MainApp *mainApp) : m_mainApp(mainApp) {
    m_doubleClickTimer.start();
}

float VulkanWindow::uvToAngle(QPointF uv, int32_t id) {
    return atan2((1.0 - uv.y()) - m_optimizer->getConstraints()[id].centerY,
                 uv.x() - m_optimizer->getConstraints()[id].centerX);
}

QListWidget *&VulkanWindow::getConstraintWidgetList() {
    return m_constraintWidgetList;
}

void VulkanWindow::crash(const QString& msg) {
    QMessageBox *errorBox = new QMessageBox();
    errorBox->critical(0, "Fatal error", msg);
    errorBox->setFixedSize(500, 200);
    qFatal(qUtf8Printable(msg));
}

void VulkanWindow::setFPSLabel(const QString& str) {
    m_mainApp->getBottomBar()->setFPSLabel(str);
}

MainApp *VulkanWindow::getMainApp() {
    return m_mainApp;
}

bool VulkanWindow::tryPick(uint32_t id, QPointF uv) {
    auto &rect = m_optimizer->getConstraints()[id];

    const float scale = m_renderer->getScale();
    float x = uv.x(); float y = 1.0f-uv.y();
    x -= rect.centerX; y -= rect.centerY;
    float tmp = x;
    x = cosf(rect.rotAngle)*x - sinf(rect.rotAngle)*y;
    y = sinf(rect.rotAngle)*tmp + cosf(rect.rotAngle)*y;
    tmp = x;
    x = x + rect.skewH*y;
    y = (1.0f+rect.skewH*rect.skewV)*y+rect.skewV*tmp;
    x /= rect.width*rect.lineWidth*0.5f; y /= rect.height*rect.lineHeight*0.5f;

    if ((fabs(x)-1.0f)*fabs(rect.width*rect.lineWidth)*scale < 0.08f && (fabs(y)-1.0f)*fabs(rect.height*rect.lineHeight)*scale < 0.08f) {
        if (fabs(x) > 0.8f && fabs(y) > 0.8f) {
            m_editAction = Rotate;
            m_actionStartData[0] = rect.rotAngle;
            m_actionStartData[1] = uvToAngle(uv, id);
        } else if (fabs(fabs(x)-1.0f)*fabs(rect.width*rect.lineWidth)*scale > 0.08f && fabs(fabs(y)-1.0f)*fabs(rect.height*rect.lineHeight)*scale > 0.08f) {
            m_editAction = Move;
            m_actionStartData[0] = rect.centerX;
            m_actionStartData[1] = rect.centerY;
            m_actionStartData[2] = uv.x();
            m_actionStartData[3] = uv.y();
        } else {
            m_actionStartData[0] = rect.centerX;
            m_actionStartData[1] = rect.centerY;
            m_actionStartData[2] = rect.width;
            m_actionStartData[3] = rect.height;
            m_actionStartData[4] = cosf(rect.rotAngle)*uv.x() - sinf(rect.rotAngle)*(1.0f-uv.y());
            m_actionStartData[5] = cosf(rect.rotAngle)*(1.0f-uv.y()) + sinf(rect.rotAngle)*uv.x();
            m_actionStartData[6] = rect.rotAngle;
            if (x > 0.8f) {
                m_editAction = DragPX;
            } else if (x < -0.8f) {
                m_editAction = DragNX;
            } else if (y > 0.8f) {
                m_editAction = DragPY;
            } else {
                m_editAction = DragNY;
            }
        }
        return true;
    }
    return false;
}

int32_t VulkanWindow::getConstraint(QPointF uv, bool doubleClick) {
    if (!m_renderer->getShowConstraints()) {
        return -1;
    }

    int32_t minIndex = -1;
    float minDist = std::numeric_limits<float>::max();
    for (uint32_t index = 0; index < m_optimizer->getConstraints().size(); ++index) {
        auto &rect = m_optimizer->getConstraints()[index];

        const float scale = m_renderer->getScale();
        float x = uv.x(); float y = 1.0f-uv.y();
        x -= rect.centerX; y -= rect.centerY;
        float tmp = x;
        x = cosf(rect.rotAngle)*x - sinf(rect.rotAngle)*y;
        y = sinf(rect.rotAngle)*tmp + cosf(rect.rotAngle)*y;
        tmp = x;
        x = x + rect.skewH*y;
        y = (1.0f+rect.skewH*rect.skewV)*y+rect.skewV*tmp;
        x /= rect.width*rect.lineWidth*0.5f; y /= rect.height*rect.lineHeight*0.5f;

        if ((fabs(x)-1.0f)*fabs(rect.width*rect.lineWidth)*scale < 0.08f && (fabs(y)-1.0f)*fabs(rect.height*rect.lineHeight)*scale < 0.08f) {
            x = x*rect.lineWidth*0.5f+rect.lineCenterX;
            y = 1.0-y*rect.lineHeight*0.5f-rect.lineCenterY;
            for (uint32_t i = 0; i < rect.lines.size(); ++i) {
                auto &line = rect.lines[i];
                if (doubleClick) {
                    QVector2D lineDir0(line.x1-line.x0, line.y1-line.y0);
                    QVector2D toPoint0(x-line.x0, y-line.y0);
                    QVector2D lineDir1(line.x0-line.x1, line.y0-line.y1);
                    QVector2D toPoint1(x-line.x1, y-line.y1);
                    if (QVector2D::dotProduct(lineDir0, toPoint0) > 0.0 && QVector2D::dotProduct(lineDir1, toPoint1) > 0.0) {
                        lineDir0.normalize();
                        if (fabs(QVector2D::dotProduct(toPoint0, QVector2D(-lineDir0.y(), lineDir0.x())))*scale*rect.width < 0.01) {
                            m_optimizer->addPoint(index, i, uv.x(), uv.y());
                            m_editAction = MovePoint;
                            m_selectedLine = i+1;
                            return index;
                        }
                    }
                }
                if (fabs(line.x0 - x)*scale*rect.width < 0.016 && fabs(line.y0 - y)*scale*rect.height < 0.016) {
                    m_editAction = MovePoint;
                    m_selectedLine = i;
                    return index;
                } else if (i == rect.lines.size()-1 && fabs(line.x1 - x)*scale*rect.width < 0.016 && fabs(line.y1 - y)*scale*rect.height < 0.016) {
                    m_editAction = MovePoint;
                    m_selectedLine = rect.lines.size();
                    return index;
                }
            }

            float dx = uv.x()-rect.centerX;
            float dy = 1.0f-uv.y()-rect.centerY;

            float dist = sqrt(dx*dx+dy*dy);
            if (dist < minDist) {
                minDist = dist;
                minIndex = index;
            }
        }
    }

    if (minIndex >= 0) {
        tryPick(minIndex, uv);
    }
    return minIndex;
}

void VulkanWindow::switchTool(Tools tool, bool force) {
    if (m_tool == Tools::None && !force)
        return;
    m_downKeys &= ~LeftMouseButton;
    m_selectedConstraint = -1;
    m_selectedLine = -1;
    m_pointList.clear();
    m_renderer->updateConstraintPreview(QPointF(0,0), QPointF(0,0));
    m_tool = tool;
    setCursor(QCursor((m_tool == Select || m_tool == None) ? Qt::ArrowCursor : Qt::CrossCursor));
    m_mainApp->setSideBarsVisible(m_tool != None);
}

VulkanWindow::Tools VulkanWindow::getTool() {
    return m_tool;
}

void VulkanWindow::setMainAppTitle(const QString &title) {
    m_mainApp->setWindowTitle(title);
}

void VulkanWindow::keyReleaseEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Control) {
        m_downKeys &= ~Ctrl;
    } else if (event->key() == Qt::Key_Shift) {
        m_downKeys &= ~Shift;
    }
    event->accept();
}

void VulkanWindow::clearDownKeys() {
    m_downKeys = 0;
    m_mainApp->dirtyToolBar();
}

void VulkanWindow::setComputeQueueFamilyIndex(uint32_t ind) {
    m_computeQueueFamilyIndex = ind;
}

uint32_t VulkanWindow::getComputeQueueFamilyIndex() {
    return m_computeQueueFamilyIndex;
}

void VulkanWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        uint64_t time = m_doubleClickTimer.elapsed();
        m_lastPressPosition = event->position();
        m_downKeys |= LeftMouseButton;
        if (!(m_downKeys & MiddleMouseButton)) {
            m_renderer->mouseToUV(event->position(), m_lastDown);
            if (m_tool == Select) {
                grab();
                m_selectedConstraint = getConstraint(m_lastDown, time-m_lastPressTime < 500);
                m_constraintWidgetList->setCurrentRow(m_selectedConstraint);
            } else if (m_tool == Line) {
                std::array<float, 2> point = {(float)m_lastDown.x(), (float)m_lastDown.y()};
                if (m_pointList.empty() ||
                    fabs(point[0]-m_pointList[m_pointList.size()-1][0]) > 0.0005 ||
                    fabs(point[1]-m_pointList[m_pointList.size()-1][1]) > 0.0005) {
                    m_pointList.push_back(point);
                }
            }
        }
        m_lastPressTime = time;
    } else if (event->button() == Qt::RightButton) {
        m_downKeys |= RightMouseButton;
    } else if (event->button() == Qt::MiddleButton || event->button() == Qt::ExtraButton1) {
        m_downKeys |= MiddleMouseButton;
        setCursor(QCursor(Qt::ClosedHandCursor));
    }
    event->accept();
}

void VulkanWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_downKeys &= ~LeftMouseButton;

        QPointF uv;
        m_renderer->mouseToUV(event->position(), uv);
        if (m_tool == Rectangle) {
            m_optimizer->addRectangle(uv.x(), uv.y(), m_lastDown.x(), m_lastDown.y());
            m_renderer->updateConstraintPreview(uv, uv);
            m_constraintWidgetList->setCurrentRow(m_optimizer->getConstraints().size()-1);
        } else if (m_tool == Ellipse) {
                m_optimizer->addEllipse(uv.x(), uv.y(), m_lastDown.x(), m_lastDown.y());
                m_renderer->updateConstraintPreview(uv, uv);
                m_constraintWidgetList->setCurrentRow(m_optimizer->getConstraints().size()-1);
        } else if (m_tool == Select && m_selectedConstraint != -1) {
            grab();
            if ((event->position()-m_lastPressPosition).manhattanLength() > 1) {
                m_optimizer->optimize();
                m_optimizer->commitChange();
            }
            m_constraintWidgetList->setCurrentRow(-1);
            m_constraintWidgetList->setCurrentRow(m_selectedConstraint);
            m_selectedConstraint = -1;
            m_selectedLine = -1;
        }
    } else if (event->button() == Qt::RightButton) {
        m_downKeys &= ~RightMouseButton;
        if (m_tool == Line && m_pointList.size() > 1) {
            m_optimizer->addLine(m_pointList);
            m_pointList.clear();
            m_renderer->updateConstraintPreview(m_lastDown, m_lastDown);
        }
    } else if (event->button() == Qt::MiddleButton || event->button() == Qt::ExtraButton1) {
        m_downKeys &= ~MiddleMouseButton;
    }

    if (!(m_downKeys & MiddleMouseButton)) {
        setCursor(QCursor((m_tool == Select || m_tool == None) ? Qt::ArrowCursor : Qt::CrossCursor));
    }
    event->accept();
}

void VulkanWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Control) {
        m_downKeys |= Ctrl;
    } else if (event->key() == Qt::Key_Shift) {
        m_downKeys |= Shift;
    } else if ((event->key() == Qt::Key_Escape || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && m_tool == Line && m_pointList.size() > 1) {
        m_optimizer->addLine(m_pointList);
        m_pointList.clear();
        m_renderer->updateConstraintPreview(m_lastDown, m_lastDown);
    } else if (event->key() == Qt::Key_F12) {
        //regrab().save("screenshot.png");
    }
    event->accept();
}

void VulkanWindow::mouseMoveEvent(QMouseEvent* event) {
    if (!isActive() && m_mainApp->hasFocus()) {
        requestActivate();
    }
    const int deltaX = event->position().x() - m_lastDragPosition.x();
    const int deltaY = event->position().y() - m_lastDragPosition.y();

    if (m_downKeys & MiddleMouseButton) {
        if (m_downKeys & Ctrl) {
            const float speed = 0.2;
            m_renderer->updateRotation(deltaX * speed, deltaY * speed);
        } else {
            const float speed = 0.0037f;
            m_renderer->updatePosition(deltaX * speed, -deltaY * speed);
        }
    }

    if ((m_tool == Select || m_tool == None) && m_downKeys & RightMouseButton) {
        const float speed = 0.2;
        m_renderer->updateRotation(deltaX * speed, deltaY * speed);
    }

    QPointF uv;
    m_renderer->mouseToUV(event->position(), uv);
    int32_t w = m_optimizer->getWidth();
    int32_t h = m_optimizer->getHeight();
    m_mainApp->getBottomBar()->setMouseLabel(QString::number(w)+"X"+QString::number(h)+"  |  "+
                                             QString::number(int(uv.x()*h) + (w-h)/2)+", "+QString::number(int((1-uv.y())*h)));

    if (m_tool == Line && !m_pointList.empty()) {
        m_renderer->updateConstraintPreview(m_lastDown, uv);
    } else if (m_downKeys & LeftMouseButton && !(m_downKeys & MiddleMouseButton)) {
        if (m_tool == Rectangle || m_tool == Ellipse) {
            m_renderer->updateConstraintPreview(m_lastDown, uv);
        } else if (m_tool == Select && m_selectedConstraint != -1) {
            auto &rect = m_optimizer->getConstraints()[m_selectedConstraint];
            switch(m_editAction) {
            case Move:
                m_optimizer->moveRectangleX(m_selectedConstraint, m_actionStartData[0]+uv.x()-m_actionStartData[2], false);
                m_optimizer->moveRectangleY(m_selectedConstraint, m_actionStartData[1]-uv.y()+m_actionStartData[3], false);
                break;
            case Rotate:
                m_optimizer->rotateRectangle(m_selectedConstraint, m_actionStartData[0]+m_actionStartData[1]-uvToAngle(uv, m_selectedConstraint), false);
                break;
            case DragPX:
            {
                float x = cosf(m_actionStartData[6])*uv.x() - sinf(m_actionStartData[6])*(1.0-uv.y());
                m_optimizer->moveRectangleX(m_selectedConstraint, m_actionStartData[0] + cosf(m_actionStartData[6])*(x-m_actionStartData[4])/2, false);
                m_optimizer->moveRectangleY(m_selectedConstraint, m_actionStartData[1] - sinf(m_actionStartData[6])*(x-m_actionStartData[4])/2, false);
                m_optimizer->scaleRectangleX(m_selectedConstraint, m_actionStartData[2] + (x-m_actionStartData[4])/rect.lineWidth, false);
            }   break;
            case DragNX:
            {
                float x = cosf(m_actionStartData[6])*uv.x() - sinf(m_actionStartData[6])*(1.0-uv.y());
                m_optimizer->moveRectangleX(m_selectedConstraint, m_actionStartData[0] + cosf(m_actionStartData[6])*(x-m_actionStartData[4])/2, false);
                m_optimizer->moveRectangleY(m_selectedConstraint, m_actionStartData[1] - sinf(m_actionStartData[6])*(x-m_actionStartData[4])/2, false);
                m_optimizer->scaleRectangleX(m_selectedConstraint, m_actionStartData[2] - (x-m_actionStartData[4])/rect.lineWidth, false);
            }   break;
            case DragPY:
            {
                float y = cosf(m_actionStartData[6])*(1.0f-uv.y()) + sinf(m_actionStartData[6])*uv.x();
                m_optimizer->moveRectangleX(m_selectedConstraint, m_actionStartData[0] + sinf(m_actionStartData[6])*(y-m_actionStartData[5])/2, false);
                m_optimizer->moveRectangleY(m_selectedConstraint, m_actionStartData[1] + cosf(m_actionStartData[6])*(y-m_actionStartData[5])/2, false);
                m_optimizer->scaleRectangleY(m_selectedConstraint, m_actionStartData[3] + (y-m_actionStartData[5])/rect.lineHeight, false);
            }   break;
            case DragNY:
            {
                float y = cosf(m_actionStartData[6])*(1.0f-uv.y()) + sinf(m_actionStartData[6])*uv.x();
                m_optimizer->moveRectangleX(m_selectedConstraint, m_actionStartData[0] + sinf(m_actionStartData[6])*(y-m_actionStartData[5])/2, false);
                m_optimizer->moveRectangleY(m_selectedConstraint, m_actionStartData[1] + cosf(m_actionStartData[6])*(y-m_actionStartData[5])/2, false);
                m_optimizer->scaleRectangleY(m_selectedConstraint, m_actionStartData[3] - (y-m_actionStartData[5])/rect.lineHeight, false);
            }   break;
            case MovePoint:
                m_optimizer->movePoint(m_selectedConstraint, m_selectedLine, uv.x(), uv.y());
                break;
            default:
                break;
            }
        }
    }

    m_lastDragPosition = event->pos();
    event->accept();
}

void VulkanWindow::wheelEvent(QWheelEvent* event) {
    float val = event->angleDelta().y()/1200.0f;
    m_renderer->updateScale(1.0+val);
    event->accept();
}

Render *VulkanWindow::getRender() {
    return m_renderer;
}

bool VulkanWindow::getOptimizeOnMove() {
    return m_optimizeOnMove;
}

void VulkanWindow::setOptimizeOnMove(bool val) {
    m_optimizeOnMove = val;
}

Optimizer *VulkanWindow::getOptimizer() {
    return m_optimizer;
}

int32_t VulkanWindow::getSelectedLine() {
    return m_selectedLine;
}

int32_t VulkanWindow::getSelectedConstraint() {
    return m_selectedConstraint;
}

void VulkanWindow::updateMouseLabel() {
    QPointF uv;
    m_renderer->mouseToUV(QCursor::pos(), uv);
    int32_t w = m_optimizer->getWidth();
    int32_t h = m_optimizer->getHeight();
    m_mainApp->getBottomBar()->setMouseLabel(QString::number(w)+"X"+QString::number(h)+"  |  "+
                                             QString::number(int(uv.x()*h) + (w-h)/2)+", "+QString::number(int((1-uv.y())*h)));
}

void VulkanWindow::dirtyUndoMenu() {
    m_mainApp->dirtyUndoMenu();
}

QVulkanWindowRenderer *VulkanWindow::createRenderer() {
    setCursor(QCursor(Qt::ArrowCursor));
    requestActivate();
    return m_renderer = new Render(this, m_optimizer, m_pointList, true);
}

uint32_t VulkanWindow::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vulkanInstance()->functions()->vkGetPhysicalDeviceMemoryProperties(physicalDevice(), &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    crash("failed to find suitable memory type!");
    return 0;
}

VkDeviceSize VulkanWindow::aligned(VkDeviceSize v, VkDeviceSize byteAlign) {
    return (v + byteAlign - 1) & ~(byteAlign - 1);
}

VkShaderModule VulkanWindow::createShader(const QString &name) {
    QFile file(name);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Failed to read shader %s", qPrintable(name));
        return VK_NULL_HANDLE;
    }
    QByteArray blob = file.readAll();
    file.close();

    VkShaderModuleCreateInfo shaderInfo;
    memset(&shaderInfo, 0, sizeof(shaderInfo));
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = blob.size();
    shaderInfo.pCode = reinterpret_cast<const uint32_t *>(blob.constData());
    VkShaderModule shaderModule;
    VkResult err = vulkanInstance()->deviceFunctions(device())->
                   vkCreateShaderModule(device(), &shaderInfo, nullptr, &shaderModule);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create shader module: %d", err);
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}
