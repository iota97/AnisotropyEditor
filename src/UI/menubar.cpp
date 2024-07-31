#include "menubar.h"
#include <QFileDialog>
#include <QApplication>
#include <QActionGroup>
#include <QStandardPaths>
#include "src/Field/optimizer.h"
#include "src/UI/mainapp.h"
#include <QMessageBox>

MenuBar::~MenuBar() {
    delete m_helpTextEdit;
    delete m_renderingSettings;
    delete m_newProject;
    delete m_optimizerSettings;
}

void MenuBar::close(QCloseEvent *event) {
    (void) event;
    m_helpTextEdit->close();
    m_renderingSettings->close();
    m_newProject->close();
    m_optimizerSettings->close();
}

MenuBar::MenuBar(VulkanWindow *vulkanWindow) : m_window(vulkanWindow) {
    QMenu *fileMenu = addMenu("&File");
    QAction *referenceRender = new QAction("Reference");
    m_newProject = new NewProject(vulkanWindow);
    QAction *newTexture = fileMenu->addAction("New project");
    newTexture->setShortcut(Qt::CTRL | Qt::Key_N);
    QObject::connect(newTexture, &QAction::triggered, [=](){
        vulkanWindow->clearDownKeys();
        m_newProject->show();
    });

    QAction *newFromReference = fileMenu->addAction("New project from reference");
    newFromReference->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_N);
    QObject::connect(newFromReference, &QAction::triggered, [=](){
        vulkanWindow->clearDownKeys();
        QString fileName = QFileDialog::getOpenFileName(nullptr, "Open image",
            getFileDir(),
            "Images (*.png *.jpg *.jpeg *.bmp)");
        if (fileName != "") {
            m_fileDir = QFileInfo(fileName).absolutePath();
            vulkanWindow->getRender()->setReferenceImage(fileName, true);
            vulkanWindow->getOptimizer()->clear();
            referenceRender->setEnabled(true);
            vulkanWindow->getRender()->resetMeshTransform();
        }
    });

    fileMenu->addSeparator();
    QAction *save = fileMenu->addAction("Save project");
    save->setShortcut(Qt::CTRL | Qt::Key_S);
    QObject::connect(save, &QAction::triggered, [=](){
        vulkanWindow->clearDownKeys();
        QString fileName = QFileDialog::getSaveFileName(nullptr, "Save project",
            getFileDir() + "/project.cst",
             "Projects (*.cst)");
        if (fileName != "") {
            m_fileDir = QFileInfo(fileName).absolutePath();
            vulkanWindow->getOptimizer()->save(fileName);
        }
    });

    static QAction *unloadMesh;
    QAction **unload = &unloadMesh;
    QAction *load = fileMenu->addAction("Open project");
    load->setShortcut(Qt::CTRL | Qt::Key_O);
    QObject::connect(load, &QAction::triggered, [=](){
        vulkanWindow->clearDownKeys();
        QString fileName = QFileDialog::getOpenFileName(nullptr, "Open project",
            getFileDir(),
            "Projects (*.cst)");
        if (fileName != "") {
            m_fileDir = QFileInfo(fileName).absolutePath();
            vulkanWindow->getOptimizer()->load(fileName);
            vulkanWindow->getRender()->unloadMesh();
            unloadMesh->setEnabled(false);
            vulkanWindow->switchTool(VulkanWindow::Tools::Select, true);
            vulkanWindow->clearDownKeys();
            vulkanWindow->getRender()->resetMeshTransform();
        }
    });

    fileMenu->addSeparator();
    QAction *loadMesh = fileMenu->addAction("Load mesh");
    loadMesh->setShortcut(Qt::CTRL | Qt::Key_M);
    QObject::connect(loadMesh, &QAction::triggered, [=](){
        vulkanWindow->clearDownKeys();
        QString fileName = QFileDialog::getOpenFileName(nullptr, "Open mesh",
                                                        getFileDir(),
                                                        "mesh (*.obj)");
        if (fileName != "") {
            m_fileDir = QFileInfo(fileName).absolutePath();
            if (!vulkanWindow->getRender()->setMesh(fileName)) {
                QMessageBox *errorBox = new QMessageBox();
                errorBox->critical(0, "Mesh can not be loaded!", "Mesh is missing normals or texture coordinates");
                errorBox->setAttribute(Qt::WA_DeleteOnClose);
            } else {
                (*unload)->setEnabled(true);
                vulkanWindow->getOptimizer()->clear();
                vulkanWindow->switchTool(VulkanWindow::Tools::None);
            }
        }
        vulkanWindow->clearDownKeys();
    });

    unloadMesh = fileMenu->addAction("Unload mesh");
    unloadMesh->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_M);
    unloadMesh->setEnabled(false);
    QObject::connect(unloadMesh, &QAction::triggered, [=](){
        vulkanWindow->getRender()->unloadMesh();
        unloadMesh->setEnabled(false);
        vulkanWindow->switchTool(VulkanWindow::Tools::Select, true);
        vulkanWindow->clearDownKeys();
    });

    fileMenu->addSeparator();
    QAction *loadReferenceImage = fileMenu->addAction("Load reference image");
    loadReferenceImage->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_R);
    QObject::connect(loadReferenceImage, &QAction::triggered, [=](){
        vulkanWindow->clearDownKeys();
        QString fileName = QFileDialog::getOpenFileName(nullptr, "Open image",
            getFileDir(),
            "Images (*.png *.jpg *.jpeg *.bmp)");
        if (fileName != "") {
            m_fileDir = QFileInfo(fileName).absolutePath();
            vulkanWindow->getRender()->setReferenceImage(fileName);
            referenceRender->setEnabled(true);
        }
    });
    fileMenu->addSeparator();

    m_imageLoadTimer.setSingleShot(true);
    QObject::connect(&m_imageLoadTimer, &QTimer::timeout, [=]() {
        if (m_hotReload && QFileInfo(m_imageFileName).size() > 0) {
            vulkanWindow->getOptimizer()->clear();
            if (m_inputImageIsDirection) {
                vulkanWindow->getRender()->setAnisoDirTexture(m_imageFileName);
            } else {
                vulkanWindow->getRender()->setAnisoAngleTexture(m_imageFileName, m_inputImageIsHalf);
            }
        }
    });

    QObject::connect(&m_inputImageWatcher, &QFileSystemWatcher::fileChanged, [=](const QString& fileName) {
        m_imageFileName = fileName;
        m_imageLoadTimer.stop();
        m_imageLoadTimer.start(500);
    });

    QAction *loadAnisoAngle = fileMenu->addAction("Import anisotropy angles [0, 180]");
    loadAnisoAngle->setShortcut(Qt::CTRL | Qt::Key_T);
    QObject::connect(loadAnisoAngle, &QAction::triggered, [=](){
        vulkanWindow->clearDownKeys();
        QString fileName = QFileDialog::getOpenFileName(nullptr, "Open File",
                                                        getFileDir(),
                                                        "Images (*.png *.jpg *.jpeg *.bmp)");
        if (fileName != "") {
            m_fileDir = QFileInfo(fileName).absolutePath();
            vulkanWindow->getOptimizer()->clear();
            vulkanWindow->getRender()->setAnisoAngleTexture(fileName);

            if (!m_inputImageWatcher.files().empty()) {
                m_inputImageWatcher.removePath(m_inputImageWatcher.files()[0]);
            }
            m_inputImageIsDirection = false;
            m_inputImageIsHalf = false;
            m_inputImageWatcher.addPath(fileName);
        }
    });

    QAction *loadAnisoAngleHalf = fileMenu->addAction("Import anisotropy angles [0, 90]");
    loadAnisoAngleHalf->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_T);
    QObject::connect(loadAnisoAngleHalf, &QAction::triggered, [=](){
        vulkanWindow->clearDownKeys();
        QString fileName = QFileDialog::getOpenFileName(nullptr, "Open File",
                                                        getFileDir(),
                                                        "Images (*.png *.jpg *.jpeg *.bmp)");
        if (fileName != "") {
            m_fileDir = QFileInfo(fileName).absolutePath();
            vulkanWindow->getOptimizer()->clear();
            vulkanWindow->getRender()->setAnisoAngleTexture(fileName, true);

            if (!m_inputImageWatcher.files().empty()) {
                m_inputImageWatcher.removePath(m_inputImageWatcher.files()[0]);
            }
            m_inputImageIsDirection = false;
            m_inputImageIsHalf = true;
            m_inputImageWatcher.addPath(fileName);
        }
    });

    QAction *loadAnisoDir = fileMenu->addAction("Import anisotropy vectors");
    loadAnisoDir->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_T);
    QObject::connect(loadAnisoDir, &QAction::triggered, [=](){
        vulkanWindow->clearDownKeys();
        QString fileName = QFileDialog::getOpenFileName(nullptr, "Open image",
            getFileDir(),
            "Images (*.png *.jpg *.jpeg *.bmp)");
        if (fileName != "") {
            m_fileDir = QFileInfo(fileName).absolutePath();
            vulkanWindow->getOptimizer()->clear();
            vulkanWindow->getRender()->setAnisoDirTexture(fileName);

            if (!m_inputImageWatcher.files().empty()) {
                m_inputImageWatcher.removePath(m_inputImageWatcher.files()[0]);
            }
            m_inputImageIsDirection = true;
            m_inputImageWatcher.addPath(fileName);
        }
    });

    QAction *hotReload = fileMenu->addAction("Hot reload anisotropy");
    hotReload->setCheckable(true);
    hotReload->setChecked(false);
    QObject::connect(hotReload, &QAction::triggered, [=](bool val){
        m_hotReload = val;
    });

    fileMenu->addSeparator();
    QAction *saveAnisoAngle = fileMenu->addAction("Export anisotropy angles [0, 180]");
    saveAnisoAngle->setShortcut(Qt::CTRL | Qt::Key_E);
    QObject::connect(saveAnisoAngle, &QAction::triggered, [=](){
        vulkanWindow->clearDownKeys();
        QString fileName = QFileDialog::getSaveFileName(nullptr, "Save image",
                                                        getFileDir() + "/angle.png",
                                                        "Images (*.png)");
        if (fileName != "") {
            m_fileDir = QFileInfo(fileName).absolutePath();
            vulkanWindow->getRender()->saveAnisoAngle(fileName);
        }
    });

    QAction *saveAnisoDir = fileMenu->addAction("Export anisotropy vectors");
    saveAnisoDir->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_E);
    QObject::connect(saveAnisoDir, &QAction::triggered, [=](){
        vulkanWindow->clearDownKeys();
        QString fileName = QFileDialog::getSaveFileName(nullptr, "Save image",
                                                        getFileDir() + "/vector.png",
                                                        "Images (*.png)");
        if (fileName != "") {
            m_fileDir = QFileInfo(fileName).absolutePath();
            vulkanWindow->getRender()->saveAnisoDir(fileName);
        }
    });

    QAction *saveZebra = fileMenu->addAction("Export sine field");
    saveZebra->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_E);
    QObject::connect(saveZebra, &QAction::triggered, [=](){
        vulkanWindow->clearDownKeys();
        QString fileName = QFileDialog::getSaveFileName(nullptr, "Save image",
                                                        getFileDir() + "/sines.png",
                                                        "Images (*.png)");
        if (fileName != "") {
            m_fileDir = QFileInfo(fileName).absolutePath();
            vulkanWindow->getRender()->saveZebra(fileName);
        }
    });

    fileMenu->addSeparator();
    QAction *quit = fileMenu->addAction("Quit");
    QObject::connect(quit, &QAction::triggered, [&](){ QApplication::quit(); });
    quit->setShortcut(Qt::CTRL | Qt::Key_Q);

    QMenu *editMenu = addMenu("&Edit");
    m_undo = editMenu->addAction("Undo");
    m_undo->setShortcut(Qt::CTRL | Qt::Key_Z);
    m_undo->setEnabled(false);
    QObject::connect(m_undo, &QAction::triggered, [=](){
        vulkanWindow->getOptimizer()->revertChange();
    });

    m_redo = editMenu->addAction("Redo");
    m_redo->setShortcut(Qt::CTRL | Qt::Key_Y);
    m_redo->setEnabled(false);
    QObject::connect(m_redo, &QAction::triggered, [=](){
        vulkanWindow->getOptimizer()->redoChange();
    });

    editMenu->addSeparator();

    QAction *rendingSettings = editMenu->addAction("Rendering settings");
    rendingSettings->setShortcut(Qt::Key_F2);
    QObject::connect(rendingSettings, &QAction::triggered, [=](){
        vulkanWindow->clearDownKeys();
        m_renderingSettings->show();
    });


    QAction *optimizerSettings = editMenu->addAction("Optimizer settings");
    optimizerSettings->setShortcut(Qt::Key_F3);
    QObject::connect(optimizerSettings, &QAction::triggered, [=](){
        vulkanWindow->clearDownKeys();
        m_optimizerSettings->show();
    });

    editMenu->addSeparator();

    QAction *resetPos = editMenu->addAction("Reset view");
    resetPos->setShortcut(Qt::CTRL | Qt::Key_R);
    QObject::connect(resetPos, &QAction::triggered, [=](){
        vulkanWindow->getRender()->resetMeshTransform();
    });

    QMenu *viewMenu = addMenu("&View");
    QActionGroup *renderingGroup = new QActionGroup(viewMenu);
    renderingGroup->setExclusive(true);

    QAction *covarainceRender = viewMenu->addAction("Fast");
    covarainceRender->setShortcut(Qt::Key_1);
    QObject::connect(covarainceRender, &QAction::triggered, [=](){
        vulkanWindow->getRender()->setPipeline("fast");
    });
    covarainceRender->setCheckable(true);
    covarainceRender->setChecked(true);
    renderingGroup->addAction(covarainceRender);

    QAction *filamentRender = viewMenu->addAction("Accurate");
    filamentRender->setShortcut(Qt::Key_2);
    QObject::connect(filamentRender, &QAction::triggered, [=](){
        vulkanWindow->getRender()->setPipeline("accurate", true);
    });
    filamentRender->setCheckable(true);
    renderingGroup->addAction(filamentRender);

    QAction *anisoDirRender = viewMenu->addAction("Direction");
    anisoDirRender->setShortcut(Qt::Key_3);
    QObject::connect(anisoDirRender, &QAction::triggered, [=](){
        vulkanWindow->getRender()->setPipeline("direction");
    });
    anisoDirRender->setCheckable(true);
    renderingGroup->addAction(anisoDirRender);

    QAction *twilightRender = viewMenu->addAction("Twilight");
    twilightRender->setShortcut(Qt::Key_4);
    QObject::connect(twilightRender, &QAction::triggered, [=](){
        vulkanWindow->getRender()->setPipeline("twilight");
    });
    twilightRender->setCheckable(true);
    renderingGroup->addAction(twilightRender);

    QAction *sineRender = viewMenu->addAction("Sine Field");
    sineRender->setShortcut(Qt::Key_5);
    QObject::connect(sineRender, &QAction::triggered, [=](){
        vulkanWindow->getRender()->setPipeline("sine");
    });
    sineRender->setCheckable(true);
    renderingGroup->addAction(sineRender);

    viewMenu->addAction(referenceRender);
    referenceRender->setShortcut(Qt::Key_6);
    QObject::connect(referenceRender, &QAction::triggered, [=](){
        vulkanWindow->getRender()->setPipeline("reference");
    });
    referenceRender->setCheckable(true);
    referenceRender->setEnabled(false);
    renderingGroup->addAction(referenceRender);

    QAction *bentRender = viewMenu->addAction("Legacy");
    bentRender->setShortcut(Qt::Key_7);
    QObject::connect(bentRender, &QAction::triggered, [=](){
        vulkanWindow->getRender()->setPipeline("bent");
    });
    bentRender->setCheckable(true);
    renderingGroup->addAction(bentRender);

    viewMenu->addSeparator();
    QAction *showConstraints = viewMenu->addAction("Show Constraints");
    showConstraints->setShortcut(Qt::Key_V);
    showConstraints->setCheckable(true);
    showConstraints->setChecked(true);
    QObject::connect(showConstraints, &QAction::triggered, [=](bool val){
        vulkanWindow->getRender()->showConstraints(val);
    });

    viewMenu->addSeparator();
    QAction *fullscreen = viewMenu->addAction("Fullscreen");
    fullscreen->setShortcut(Qt::Key_F11);
    fullscreen->setCheckable(true);
    QObject::connect(fullscreen, &QAction::triggered, [=](bool val){
        if (!val) {
            vulkanWindow->getMainApp()->showNormal();
        } else {
            vulkanWindow->getMainApp()->showFullScreen();
        }
    });

    QMenu *helpMenu = addMenu("&Help");
    m_helpTextEdit = new QTextEdit;
    m_helpTextEdit->setReadOnly(true);
    m_helpTextEdit->setWindowTitle("Help");
    m_helpTextEdit->resize(640, 480);
    QAction *help = helpMenu->addAction("Show help");
    help->setShortcut(Qt::Key_F1);

    QObject::connect(help, &QAction::triggered, [=](){
        static bool loaded = false;
        if (!loaded) {
            loaded = true;
            QFile file(QCoreApplication::applicationDirPath()+"/assets/help.md");
            file.open(QIODeviceBase::ReadOnly);
            m_helpTextEdit->setMarkdown(file.readAll());
            file.close();
        }
        m_helpTextEdit->show();
    });

    m_renderingSettings = new RenderingSettings(vulkanWindow);
    m_optimizerSettings = new OptimizerSettings(vulkanWindow);
}

void MenuBar::dirtyUndoMenu() {
    if (m_undo && m_redo && m_window && m_window->getOptimizer()) {
        m_undo->setEnabled(m_window->getOptimizer()->canUndo());
        m_redo->setEnabled(m_window->getOptimizer()->canRedo());
    }
}

QString MenuBar::getFileDir() {
    return m_fileDir == "" ? QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) : m_fileDir;
}
