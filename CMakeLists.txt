cmake_minimum_required(VERSION 3.21)

project(KamLynk VERSION 0.7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(KAMLYNK_BUILD_TESTS "Build the unit and QML tests" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick Qml QuickControls2 LinguistTools)
# Saved camera logins in the desktop keychain (apt: qtkeychain-qt6-dev).
find_package(Qt6Keychain REQUIRED)
qt_standard_project_setup(REQUIRES 6.5)

set(CRSDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/crsdk)
set(CRSDK_INCLUDE_DIR ${CRSDK_DIR}/include)
set(CRSDK_LIB_DIR ${CRSDK_DIR}/lib)
# Sony's license does not allow redistributing the SDK, so it is not in the repository.
if(NOT EXISTS ${CRSDK_LIB_DIR}/libCr_Core.so OR NOT EXISTS ${CRSDK_INCLUDE_DIR}/CameraRemote_SDK.h)
    message(FATAL_ERROR "Sony Camera Remote SDK not found in ${CRSDK_DIR}. Download it from "
        "https://support.d-imaging.sony.co.jp/app/sdk/en/index.html and run "
        "./scripts/setup-sdk.sh <CrSDK_..._Linux64PC.zip>")
endif()

# libCr_Core.so needs libxml2.so.2, which newer distros no longer ship.
set(LIBXML2_COMPAT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libxml2/lib CACHE PATH
    "Directory with the libxml2.so.2 bundled for the SDK")
# Portable (AppImage) builds link against an older system libstdc++ than the SDK needs
# (GLIBCXX_3.4.30); the AppImage supplies a newer one at run time when the host lacks it.
option(KAMLYNK_PORTABLE "Build for the portable AppImage (glibc 2.34 base)" OFF)
if(KAMLYNK_PORTABLE)
    # --as-needed: Qt's CMake config adds OpenGL/GLX to every executable, which the app does
    # not use; requiring libOpenGL.so.0 would fail on desktops without it.
    add_link_options(LINKER:--allow-shlib-undefined LINKER:--as-needed)
endif()
if(NOT EXISTS ${LIBXML2_COMPAT_DIR}/libxml2.so.2)
    message(FATAL_ERROR "Missing bundled libxml2.so.2. Run ./scripts/build-libxml2.sh first.")
endif()

# Sony Camera Remote SDK (prebuilt). CrAdapter/ must stay next to libCr_Core.so.
add_library(crsdk SHARED IMPORTED)
set_target_properties(crsdk PROPERTIES
    IMPORTED_LOCATION ${CRSDK_LIB_DIR}/libCr_Core.so
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${CRSDK_INCLUDE_DIR}
    INTERFACE_LINK_OPTIONS "LINKER:-rpath-link,${LIBXML2_COMPAT_DIR}"
)

# Runtime layout required by the SDK (same in the build tree, install and AppImage):
#   bin/kamlynk + bin/CrAdapter/   the SDK looks for CrAdapter/ next to the executable
#   lib/libCr_Core.so + lib/libxml2.so.2   libCr_Core has RUNPATH=$ORIGIN
set(KAMLYNK_RUNTIME_LIB_DIR ${CMAKE_BINARY_DIR}/lib)
add_custom_target(crsdk_runtime
    COMMAND ${CMAKE_COMMAND} -E make_directory ${KAMLYNK_RUNTIME_LIB_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${CRSDK_LIB_DIR}/libCr_Core.so ${LIBXML2_COMPAT_DIR}/libxml2.so.2
            ${KAMLYNK_RUNTIME_LIB_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CRSDK_LIB_DIR}/CrAdapter ${CMAKE_BINARY_DIR}/CrAdapter
    COMMENT "Assembling SDK runtime libraries"
)

# Camera-independent logic: value codecs, controller, fake backend.
# Uses only enum constants from the SDK headers, so it does not link the SDK.
add_library(kamlynk_core STATIC
    src/core/PropertyInfo.h
    src/core/Geometry.h
    src/core/PropertyCodec.h src/core/PropertyCodec.cpp
    src/core/CameraBackend.h src/core/CameraBackend.cpp
    src/core/FakeBackend.h src/core/FakeBackend.cpp
    src/core/CameraProperty.h src/core/CameraProperty.cpp
    src/core/CameraController.h src/core/CameraController.cpp
    src/core/UsbSetup.h src/core/UsbSetup.cpp
    src/core/DesktopIntegration.h src/core/DesktopIntegration.cpp
    src/core/CredentialStore.h src/core/CredentialStore.cpp
    src/core/DeviceHistory.h src/core/DeviceHistory.cpp
    src/core/Language.h src/core/Language.cpp
)
target_compile_definitions(kamlynk_core PRIVATE KAMLYNK_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(kamlynk_core PUBLIC src)
target_include_directories(kamlynk_core SYSTEM PUBLIC ${CRSDK_INCLUDE_DIR})
target_link_libraries(kamlynk_core PUBLIC Qt6::Core Qt6::Gui)
target_compile_options(kamlynk_core PRIVATE -Wall -Wextra -fsigned-char)

# Real camera backend on top of the Sony SDK.
add_library(kamlynk_sdk STATIC
    src/sdk/SonyBackend.h src/sdk/SonyBackend.cpp
)
target_link_libraries(kamlynk_sdk PUBLIC kamlynk_core crsdk)
target_compile_options(kamlynk_sdk PRIVATE -Wall -Wextra -fsigned-char)

# QML UI + Qt Quick glue, shared by the app and the QML smoke test.
add_library(kamlynk_ui STATIC
    src/app/AppSetup.h src/app/AppSetup.cpp
    src/app/LiveViewItem.h src/app/LiveViewItem.cpp
)
target_link_libraries(kamlynk_ui PUBLIC kamlynk_core Qt6::Quick Qt6::Qml Qt6::QuickControls2)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(kamlynk_ui
    URI KamLynk
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/ConnectPage.qml
        qml/ShootPage.qml
        qml/components/RecordButton.qml
        qml/components/StatusBar.qml
        qml/components/OptionPicker.qml
        qml/components/FocusPanel.qml
        qml/components/ZoomRocker.qml
        qml/components/IconButton.qml
        qml/components/SectionHeader.qml
        qml/components/CredentialsDialog.qml
        qml/components/AboutDialog.qml
        qml/components/BatteryIcon.qml
        qml/components/SettingTile.qml
        qml/components/MediaIndicator.qml
        qml/components/SettingsDialog.qml
        qml/components/TermsDialog.qml
    RESOURCES
        qml/icons/camera.svg
        qml/icons/eye.svg
        qml/icons/battery-low.svg
        qml/icons/fullscreen.svg
        qml/icons/fullscreen-exit.svg
        qml/icons/eye-off.svg
        qml/icons/grid.svg
        qml/icons/info.svg
        qml/icons/key.svg
        qml/icons/mirror.svg
        qml/icons/sdcard.svg
        qml/icons/settings.svg
        qml/icons/thermo.svg
)
# App icon: window icon (X11 and Wayland xdg-toplevel-icon) and the About dialog.
qt_add_resources(kamlynk_ui "appicon" PREFIX "/" FILES packaging/kamlynk.svg)

qt_add_executable(kamlynk src/app/main.cpp src/app/KeychainStore.h src/app/KeychainStore.cpp)
target_compile_definitions(kamlynk PRIVATE KAMLYNK_VERSION="${PROJECT_VERSION}")
target_link_libraries(kamlynk PRIVATE kamlynk_ui kamlynk_uiplugin kamlynk_sdk qt6keychain)
add_dependencies(kamlynk crsdk_runtime)
set_target_properties(kamlynk PROPERTIES
    BUILD_RPATH "${KAMLYNK_RUNTIME_LIB_DIR}"
    INSTALL_RPATH "$ORIGIN/../lib"
)

install(TARGETS kamlynk RUNTIME DESTINATION bin)
# libmonitor_protocol*.so are only needed for ControlMonitoring/MoviePlayback, unused here.
install(FILES
    ${CRSDK_LIB_DIR}/libCr_Core.so
    ${LIBXML2_COMPAT_DIR}/libxml2.so.2
    DESTINATION lib)
install(DIRECTORY ${CRSDK_LIB_DIR}/CrAdapter DESTINATION bin)
install(FILES packaging/99-kamlynk-sony-camera.rules DESTINATION share/kamlynk)
# License texts travel with the binaries (AppImage: usr/share/doc/kamlynk).
install(FILES LICENSE THIRD_PARTY_NOTICES.md DESTINATION share/doc/kamlynk)
install(DIRECTORY LICENSES/ DESTINATION share/doc/kamlynk/licenses)

# UI translations. Source texts are pt-BR; the compiled .qm files are embedded at :/i18n
# (in kamlynk_core, so the tests can load them too). After changing texts, run
# `cmake --build build --target update_translations` and translate the new entries.
qt_add_translations(TARGETS kamlynk_core
    SOURCE_TARGETS kamlynk_core kamlynk_sdk kamlynk_ui kamlynk
    TS_FILES i18n/kamlynk_en.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -no-obsolete -locations none -source-language pt_BR)

if(KAMLYNK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
