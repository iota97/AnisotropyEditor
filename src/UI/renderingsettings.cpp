#include "renderingsettings.h"
#include "src/UI/vulkanwindow.h"

#include <QWidget>
#include <QColorDialog>
#include <QVBoxLayout>
#include <QSlider>
#include <QPushButton>
#include <QSpinBox>

#include <memory>
#include <string>
template<typename ... Args>
std::string string_format( const std::string& format, Args ... args ) {
    int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    if( size_s <= 0 ) return "";
    auto size = static_cast<size_t>( size_s );
    std::unique_ptr<char[]> buf( new char[ size ] );
    std::snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}

RenderingSettings::RenderingSettings(VulkanWindow *window): m_window(window) {
    setWindowTitle("Rendering settings");
    setMinimumWidth(320);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(QMargins(10,10,10,10));
    layout->setSpacing(0);

    QWidget *settingWidgetBrdf = new QWidget();
    QVBoxLayout *verLayoutBrdf = new QVBoxLayout;
    settingWidgetBrdf->setLayout(verLayoutBrdf);
    verLayoutBrdf->setAlignment(Qt::AlignTop);
    layout->addWidget(settingWidgetBrdf);

    QPushButton *albedo = new QPushButton("Material color");
    verLayoutBrdf->addWidget(albedo);
    m_colorDialog = new QColorDialog(Qt::red);
    QObject::connect(albedo, &QPushButton::clicked, [&](){
        m_colorDialog->show();
    });
    QObject::connect(m_colorDialog, &QColorDialog::colorSelected, [&](const QColor &c){
        m_window->getRender()->setAlbedo(c.redF(), c.greenF(), c.blueF());
    });

    m_anisoLabel = new QLabel();
    m_anisoLabel->setText(string_format("     Anisotropy: %.2f     ", m_anisotropy).c_str());
    verLayoutBrdf->addWidget(m_anisoLabel);
    QSlider *aniso = new QSlider(Qt::Horizontal);
    aniso->setMinimum(-95);
    aniso->setMaximum(95);
    aniso->setValue(70);
    QObject::connect(aniso, &QSlider::valueChanged, [&](int value){
        m_anisotropy = value/100.0f;
        m_anisoLabel->setText(string_format("     Anisotropy: %.2f     ", m_anisotropy).c_str());
        m_window->getRender()->setAnisoValues(m_roughness, m_anisotropy);
    });
    verLayoutBrdf->addWidget(aniso);

    m_roughLabel = new QLabel();
    m_roughLabel->setText(string_format("     Roughness: %.2f     ", m_roughness).c_str());
    verLayoutBrdf->addWidget(m_roughLabel);
    QSlider *rough = new QSlider(Qt::Horizontal);
    rough->setMinimum(1);
    rough->setMaximum(100);
    rough->setValue(20);
    QObject::connect(rough, &QSlider::valueChanged, [=](int value){
        m_roughness = value/100.0f;
        m_roughLabel->setText(string_format("     Roughness: %.2f     ", m_roughness).c_str());
        m_window->getRender()->setAnisoValues(m_roughness, m_anisotropy);
    });
    verLayoutBrdf->addWidget(rough);

    QLabel *metalLabel = new QLabel("     Metalness: 1.00     ");
    verLayoutBrdf->addWidget(metalLabel);
    QSlider *metal = new QSlider(Qt::Horizontal);
    metal->setMinimum(0);
    metal->setMaximum(100);
    metal->setValue(100);
    QObject::connect(metal, &QSlider::valueChanged, [=](int value){
        metalLabel->setText(string_format("     Metalness: %.2f     ", value/100.0f).c_str());
        m_window->getRender()->setMetallic(value/100.0f);
    });
    verLayoutBrdf->addWidget(metal);

    QLabel *exposureLabel = new QLabel("     Exposure: 1.50     ");
    verLayoutBrdf->addWidget(exposureLabel);
    QSlider *exposure = new QSlider(Qt::Horizontal);
    exposure->setMinimum(10);
    exposure->setMaximum(500);
    exposure->setValue(150);
    QObject::connect(exposure, &QSlider::valueChanged, [=](int value){
        exposureLabel->setText(string_format("     Exposure: %.2f     ", value/100.0f).c_str());
        m_window->getRender()->setExposure(value/100.0f);
    });
    verLayoutBrdf->addWidget(exposure);

    QWidget *iterationWidget = new QWidget();
    layout->addWidget(iterationWidget);
    QHBoxLayout *layoutIteration = new QHBoxLayout;
    layoutIteration->setContentsMargins(QMargins(0,0,0,0));
    iterationWidget->setLayout(layoutIteration);
    QSpinBox *iteration = new QSpinBox(this);
    iteration->setRange(8, 64);
    iteration->setValue(32);
    layoutIteration->addWidget(new QLabel("Samples count (fast only): "));
    layoutIteration->addWidget(iteration);
    QObject::connect(iteration, &QSpinBox::valueChanged, [&](int val){
        m_window->getRender()->setSampleCount(val);
    });
    verLayoutBrdf->addWidget(iterationWidget);
    setLayout(layout);
}

RenderingSettings::~RenderingSettings() {
    delete m_colorDialog;
}

