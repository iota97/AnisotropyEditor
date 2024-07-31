#include <QApplication>
#include <QLoggingCategory>
#include <QFile>
#include <QScreen>
#include "src/UI/vulkanwindow.h"
#include "src/UI/mainapp.h"
#include <cstdlib>
#include <ctime>

Q_LOGGING_CATEGORY(lcVk, "qt.vulkan")

int main(int argc, char *argv[]) {
    srand(static_cast <unsigned> (time(0)));

    QApplication app(argc, argv);
    QLoggingCategory::setFilterRules(QStringLiteral("qt.vulkan=true"));

    MainApp mainApp;
    mainApp.resize(1024, 576);
    mainApp.show();

    QVulkanInstance inst;
    inst.setApiVersion(QVersionNumber(1, 1));
#ifdef QT_DEBUG
    inst.setLayers({ "VK_LAYER_KHRONOS_validation" });
#endif
    if (!inst.create())
        qFatal("Failed to create Vulkan instance: %d", inst.errorCode());

    VulkanWindow vulkanWindow(&mainApp);
    vulkanWindow.setDeviceExtensions({ "VK_EXT_descriptor_indexing" });
    vulkanWindow.setVulkanInstance(&inst);

    VkPhysicalDeviceDescriptorIndexingFeatures indexFeatures = {};
    VkPhysicalDeviceHostQueryResetFeatures resetFeatures = {};
    indexFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    resetFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES;
    vulkanWindow.setEnabledFeaturesModifier([&](VkPhysicalDeviceFeatures2& features) {
        features.pNext = &indexFeatures;
        indexFeatures.pNext = &resetFeatures;
        vulkanWindow.vulkanInstance()->functions()->
            vkGetPhysicalDeviceFeatures2(vulkanWindow.physicalDevice(), &features);
    });

    const float prio[] = {0};
    vulkanWindow.setQueueCreateInfoModifier([&](
        const VkQueueFamilyProperties *properties, uint32_t count, QList<VkDeviceQueueCreateInfo> &info) {
        for (size_t i = 0; i < count; ++i) {
            if (properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                bool alreadyPresent = false;
                for (auto &createInfo: info) {
                    if (createInfo.queueFamilyIndex == i) {
                        alreadyPresent = true;
                    }
                }
                if (!alreadyPresent) {
                    VkDeviceQueueCreateInfo addQueueInfo {};
                    addQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                    addQueueInfo.queueFamilyIndex = i;
                    addQueueInfo.queueCount = 1;
                    addQueueInfo.pQueuePriorities = prio;
                    info.append(addQueueInfo);
                }
                vulkanWindow.setComputeQueueFamilyIndex(i);
                break;
            }
        }
    });

    mainApp.init(&vulkanWindow);
    return app.exec();
}
