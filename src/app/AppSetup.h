#pragma once

class QQmlEngine;

namespace kl {

class CameraController;
class DebugLog;
class Language;

// Exposes the C++ types to QML under the "KamLynk.Backend" import, with `controller`
// available as the `Camera` singleton, `language` as `Language` and `debugLog` as `DebugLog`.
// Call once, before loading QML.
void registerQmlTypes(CameraController *controller, Language *language, DebugLog *debugLog);

// Retranslates the UI loaded in `engine` (and the controller's texts) whenever the
// language changes.
void retranslateOnLanguageChange(Language *language, QQmlEngine *engine, CameraController *controller);

} // namespace kl
