find_package(Qt6 REQUIRED COMPONENTS Test QuickTest)

function(kamlynk_add_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE ${ARGN} Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

kamlynk_add_test(tst_propertycodec kamlynk_core)
kamlynk_add_test(tst_cameraproperty kamlynk_core)
kamlynk_add_test(tst_cameracontroller kamlynk_core)
kamlynk_add_test(tst_geometry kamlynk_core)
kamlynk_add_test(tst_qml kamlynk_ui kamlynk_uiplugin Qt6::Quick)
kamlynk_add_test(tst_usbsetup kamlynk_core)
kamlynk_add_test(tst_desktopintegration kamlynk_core)
kamlynk_add_test(tst_devicehistory kamlynk_core)
kamlynk_add_test(tst_language kamlynk_core)

# End-to-end: the real executable must load the Sony SDK and its CrAdapter plugins.
add_test(NAME sdk_runtime COMMAND kamlynk --sdk-check)
set_tests_properties(sdk_runtime PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    PASS_REGULAR_EXPRESSION "init: ok"
    FAIL_REGULAR_EXPRESSION "FAILED")

# Regression: system libssh2 (pulled in by Qt Network -> libproxy -> libcurl) must not hijack
# the SDK's own libssh2. Needs an SSH server on localhost; skipped (exit 2) otherwise.
add_test(NAME sdk_ssh_fingerprint COMMAND kamlynk --ssh-check 127.0.0.1)
set_tests_properties(sdk_ssh_fingerprint PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    SKIP_RETURN_CODE 2
    PASS_REGULAR_EXPRESSION "fingerprint: 0x0 ")

# AppImage start-up hook: fallback libraries only when the system lacks them.
add_test(NAME apprun_hook COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_apprun_hook.sh
         ${CMAKE_SOURCE_DIR}/packaging/portable/apprun-hook.sh)
set_tests_properties(apprun_hook PROPERTIES PASS_REGULAR_EXPRESSION "apprun hook: OK")
