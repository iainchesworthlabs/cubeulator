# iOS. Included by cmake/Platform.cmake; the contract it fills in is described
# at the top of that file.

set(CUBEULATOR_PLATFORM_DISPLAY_NAME "iOS")
set(CUBEULATOR_PLATFORM_DESKTOP OFF)
# A native camera device, opened from the calling thread? (cmake/Platform.cmake)
# Archivo and IBM Plex Mono are compiled in and registered at start-up
# (src/app/CMakeLists.txt).
set(CUBEULATOR_PLATFORM_BUNDLE_LATIN_FONTS ON)
set(CUBEULATOR_PLATFORM_TEST_NATIVE_CAMERA OFF)

# The test suites are not built for iOS (CUBEULATOR_BUILD_TESTS is OFF in the
# preset); this is the value they would use if they were.
set(CUBEULATOR_PLATFORM_TEST_QPA_PLATFORM "offscreen")
set(CUBEULATOR_PLATFORM_TEST_DISPLAY_VARIABLES "")
set(CUBEULATOR_PLATFORM_TEST_DISPLAY_HELP "")
set(CUBEULATOR_PLATFORM_TEST_HIDE_WINDOW OFF)
set(CUBEULATOR_PLATFORM_TEST_LINK_OPTIONS "")

# Qt is a cross-compiled kit here and always comes from CMAKE_PREFIX_PATH;
# the host-side install roots the desktop platforms search are meaningless.
set(CUBEULATOR_PLATFORM_QT_SEARCH_ROOTS "")
set(CUBEULATOR_PLATFORM_QT_ARCH_DIRS "")

# Not installed by CPack: see the packaging note below.
set(CUBEULATOR_PLATFORM_NOTICES_DIR ".")

set(CUBEULATOR_PLATFORM_SOURCES
    platform/ios/camera_ios.cpp
    platform/ios/paths_ios.cpp
    platform/ios/platform_info_ios.cpp
    platform/posix/capture_thread_scope_posix.cpp
    platform/posix/crash_handler_posix.cpp
    platform/posix/graphics_probe_none.cpp)

# The Direct3D 11 Tracy GPU backend, built into cube::lib when Tracy is on.
set(CUBEULATOR_PLATFORM_PROFILING_GPU_SOURCES platform/posix/tracy_gpu_direct3d_none.cpp)

# No Vulkan backend for Tracy here (macOS and iOS render with Metal; Android and
# the browser have not been exercised).
set(CUBEULATOR_PLATFORM_PROFILING_VULKAN OFF)
set(CUBEULATOR_PLATFORM_PROFILING_OPENGL OFF)

# The vector kernels follow the architecture (cmake/Simd.cmake).
set(CUBEULATOR_PLATFORM_SIMD auto)

set(CUBEULATOR_PLATFORM_LINK_LIBRARIES "")
set(CUBEULATOR_PLATFORM_QT_LINK_LIBRARIES "")
set(CUBEULATOR_PLATFORM_COMPILE_DEFINITIONS "")

set(CUBEULATOR_PLATFORM_APP_PROPERTIES "")

function(cube_platform_prepare_dependencies)
endfunction()

# Signing, for the signed ad-hoc build only (CUBEULATOR_BUILD_APP is ON in the
# ios-device-* presets). Both are empty for the unsigned compile check, and both
# are given together by .github/workflows/ios-device.yml. The profile is named
# by its NAME in the Apple developer portal, not by its file.
set(CUBEULATOR_IOS_SIGNING_TEAM "" CACHE STRING
    "Apple team ID the application is signed for (empty: an unsigned build)")
set(CUBEULATOR_IOS_SIGNING_PROFILE "" CACHE STRING
    "Name of the ad-hoc provisioning profile the application is signed with (empty: an unsigned build)")
# CFBundleVersion: the workflow passes its run number.
set(CUBEULATOR_IOS_BUILD_NUMBER "1" CACHE STRING "CFBundleVersion of the application")

# The kit's plugin targets that carry no QT_DEFAULT_PLUGIN property: Qt's plugin
# auto-linking reads it unwrapped as an operand of $<AND:...>, and an empty value
# is a generate-time error ("Parameters to $<AND> must resolve to either '0' or
# '1'"), seen on Qt 6.8.3's iOS kit by a sibling project. Whether 6.12's has it
# is not known, so only a plugin that has no value is given one, using what the
# desktop kits say: the ones the app needs are linked, the rest are opt-in.
function(cube_ios_default_plugins)
    foreach(_plugin IN ITEMS
            QDebugMessageServiceFactoryPlugin QGifPlugin QICOPlugin QIOSIntegrationPlugin
            QJpegPlugin QLocalClientConnectionFactoryPlugin QQmlDebugServerFactoryPlugin
            QQmlDebuggerServiceFactoryPlugin QQmlInspectorServiceFactoryPlugin
            QQmlNativeDebugConnectorFactoryPlugin QQmlNativeDebugServiceFactoryPlugin
            QQmlPreviewServiceFactoryPlugin QQmlProfilerServiceFactoryPlugin
            QQuickProfilerAdapterFactoryPlugin QSCNetworkReachabilityNetworkInformationPlugin
            QSecureTransportBackendPlugin QSvgIconPlugin QSvgPlugin
            QTcpServerConnectionFactoryPlugin QDarwinCameraPermissionPlugin)
        if(TARGET Qt6::${_plugin})
            get_target_property(_default Qt6::${_plugin} QT_DEFAULT_PLUGIN)
            if(_default STREQUAL "_default-NOTFOUND")
                set_target_properties(Qt6::${_plugin} PROPERTIES QT_DEFAULT_PLUGIN "1")
            endif()
        endif()
    endforeach()
    # Opt-in only: the other integrations, and the permission plugins of services
    # the app does not use (no Info.plist usage text exists for them, so linking
    # one would crash the first time its check ran).
    foreach(_plugin IN ITEMS
            QDarwinBluetoothPermissionPlugin QDarwinCalendarPermissionPlugin
            QDarwinContactsPermissionPlugin QDarwinLocationPermissionPlugin
            QDarwinMicrophonePermissionPlugin QIosOptionalPlugin_NSPhotoLibraryPlugin
            QMinimalIntegrationPlugin QOffscreenIntegrationPlugin
            QTlsBackendCertOnlyPlugin QTuioTouchPlugin)
        if(TARGET Qt6::${_plugin})
            get_target_property(_default Qt6::${_plugin} QT_DEFAULT_PLUGIN)
            if(_default STREQUAL "_default-NOTFOUND")
                set_target_properties(Qt6::${_plugin} PROPERTIES QT_DEFAULT_PLUGIN "0")
            endif()
        endif()
    endforeach()
endfunction()

# The IPA is Xcode's archive+export, not CMake's. What CMake does for the app:
# a static Qt has no runtime to put beside it, so this hook (called once the
# app target exists) finishes the bundle instead: Info.plist, icon, signing and
# the static plugins (packaging/ios/ConfigureApp.cmake).
function(cube_platform_deploy_qt_runtime target qml_dir)
    include("${PROJECT_SOURCE_DIR}/packaging/ios/ConfigureApp.cmake")
endfunction()

function(cube_platform_deploy_qt_library target qt_target)
endfunction()

macro(cube_platform_configure_packaging)
endmacro()

# A phone has no desktop shell of its own: the OS owns the window and the
# process's identity. The shared library still builds, with the implementation
# that does nothing (the screens' keep-awake request is answered "not held"
# until E4b).
function(cube_platform_configure_desktop_shell target)
    target_sources(${target} PRIVATE
        "${PROJECT_SOURCE_DIR}/src/app/platform/posix/desktop_shell_mobile.cpp")
endfunction()

# Nothing to install: Real packaging is Xcode's archive+export, not CMake.
function(cube_platform_configure_desktop_app target)
endfunction()
