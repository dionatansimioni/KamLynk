#include "app/AppSetup.h"

#include "app/LiveViewItem.h"
#include "core/CameraController.h"
#include "core/DebugLog.h"
#include "core/Language.h"

#include <QQmlEngine>

namespace kl {

void registerQmlTypes(CameraController *controller, Language *language, DebugLog *debugLog)
{
    constexpr const char *uri = "KamLynk.Backend";
    qmlRegisterSingletonInstance(uri, 1, 0, "Camera", controller);
    qmlRegisterSingletonInstance(uri, 1, 0, "Language", language);
    qmlRegisterSingletonInstance(uri, 1, 0, "DebugLog", debugLog);
    qmlRegisterUncreatableType<CameraController>(uri, 1, 0, "CameraController",
                                                 QStringLiteral("Use the Camera singleton"));
    qmlRegisterUncreatableType<CameraProperty>(uri, 1, 0, "CameraProperty",
                                               QStringLiteral("Provided by Camera"));
    qmlRegisterUncreatableType<UsbSetup>(uri, 1, 0, "UsbSetup", QStringLiteral("Provided by Camera.usb"));
    qmlRegisterType<LiveViewItem>(uri, 1, 0, "LiveView");
}

void retranslateOnLanguageChange(Language *language, QQmlEngine *engine, CameraController *controller)
{
    QObject::connect(language, &Language::changed, engine, [engine, controller] {
        controller->retranslate();
        engine->retranslate();
    });
}

} // namespace kl
