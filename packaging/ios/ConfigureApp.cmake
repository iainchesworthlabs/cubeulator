# iOS: makes <target> an iPhone application bundle with its Info.plist, icon and
# (when given a team and a profile) its code signature.
#
# Included by cube_platform_deploy_qt_runtime() in
# src/app/platform/ios/platform.cmake, inside that function, so `target` is
# the app. That hook is where a phone's app is finished: Qt is static here, so
# "deploying its runtime" means choosing which plugins are linked in, and
# Xcode's archive and export do the rest (the IPA is not made by CMake).
include("${PROJECT_SOURCE_DIR}/cmake/AppIdentity.cmake")
include(DesktopPackaging)

# --- the bundle and its Info.plist --------------------------------------------
cube_string_from_inventory("permission.system.camera" CUBEULATOR_CAMERA_USAGE_DESCRIPTION)
set(CUBEULATOR_BUNDLE_ID "${CUBEULATOR_APP_ID}")
set(CUBEULATOR_BUNDLE_NAME "${CUBEULATOR_APP_DISPLAY_NAME}")
set(CUBEULATOR_BUNDLE_EXECUTABLE "${CUBEULATOR_APP_DISPLAY_NAME}")
# iOS takes X.Y.Z only; the pre-release label stays in the file name of the IPA.
set(CUBEULATOR_BUNDLE_SHORT_VERSION "${PROJECT_VERSION}")
set(CUBEULATOR_BUNDLE_BUILD "${CUBEULATOR_IOS_BUILD_NUMBER}")
# The camera text in each language the app has it in, for the system's own prompt:
# <language>.lproj/InfoPlist.strings, written at configure time like the assets.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(_lproj_dir "${CMAKE_BINARY_DIR}/packaging/ios/lproj")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/packaging/ios/make_infoplist_strings.py"
        --catalogues "${PROJECT_SOURCE_DIR}/src/app/i18n" --out "${_lproj_dir}"
    RESULT_VARIABLE _rc
    OUTPUT_QUIET)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "make_infoplist_strings.py failed (${_rc})")
endif()
file(GLOB _catalogues CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/app/i18n/translations/cubeulator_*.ts")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/packaging/ios/make_infoplist_strings.py"
    "${PROJECT_SOURCE_DIR}/src/app/i18n/languages.json" ${_catalogues})
file(STRINGS "${_lproj_dir}/languages.txt" _lprojs)
set(CUBEULATOR_BUNDLE_LOCALIZATIONS "")
foreach(_lproj IN LISTS _lprojs)
    string(APPEND CUBEULATOR_BUNDLE_LOCALIZATIONS "        <string>${_lproj}</string>\n")
    set(_strings "${_lproj_dir}/${_lproj}.lproj/InfoPlist.strings")
    target_sources(${target} PRIVATE "${_strings}")
    set_source_files_properties("${_strings}" PROPERTIES MACOSX_PACKAGE_LOCATION "${_lproj}.lproj")
endforeach()
string(REGEX REPLACE "\n$" "" CUBEULATOR_BUNDLE_LOCALIZATIONS "${CUBEULATOR_BUNDLE_LOCALIZATIONS}")

set(_plist "${CMAKE_BINARY_DIR}/packaging/ios/Info.plist")
configure_file("${PROJECT_SOURCE_DIR}/packaging/ios/Info.plist.in" "${_plist}" @ONLY)

set_target_properties(${target} PROPERTIES
    OUTPUT_NAME "${CUBEULATOR_BUNDLE_EXECUTABLE}"
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${_plist}"
    # Xcode checks the plist's identifier against its own setting (CMake would
    # otherwise invent com.yourcompany.<name>), and the profile is matched on it.
    MACOSX_BUNDLE_GUI_IDENTIFIER "${CUBEULATOR_BUNDLE_ID}"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${CUBEULATOR_BUNDLE_ID}"
    # iPhone only for now (1); the iPad is 2.
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1"
    # The icon set Info.plist.in's CFBundleIconName names; without this Xcode
    # has nothing to compile that slot from and the home screen shows a blank.
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")

# --- the asset catalogue: icon and launch colour ------------------------------
# Written at configure time (packaging/make_platform_icons.py, standard library
# only): a source path ending in .xcassets is what tells the Xcode generator to
# compile the catalogue rather than copy it, and it has to exist to be listed.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(_assets_dir "${CMAKE_BINARY_DIR}/packaging/ios")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/packaging/make_platform_icons.py"
        --identity "${PROJECT_SOURCE_DIR}/docs/design/identity"
        --out "${_assets_dir}" --ios-assets
    RESULT_VARIABLE _rc
    OUTPUT_QUIET)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "make_platform_icons.py --ios-assets failed (${_rc})")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/packaging/make_platform_icons.py"
    "${PROJECT_SOURCE_DIR}/docs/design/identity/icon.svg"
    "${PROJECT_SOURCE_DIR}/docs/design/tokens.json")
set(_assets "${_assets_dir}/Assets.xcassets")
target_sources(${target} PRIVATE "${_assets}")
set_source_files_properties("${_assets}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

# --- code signing --------------------------------------------------------------
# Both empty for the unsigned compile check, which needs no Apple account; the
# workflow gives both together (the team and the profile's NAME, which the
# import-ios-signing action reads out of the profile; the name is the one the Apple
# developer portal shows, not the file's). They sign the application target alone:
# signing every Xcode target, as a project-wide setting does, makes Xcode
# demand a bundle identifier of the static libraries and Qt's resource targets,
# which have none.
if(CUBEULATOR_IOS_SIGNING_TEAM AND CUBEULATOR_IOS_SIGNING_PROFILE)
    set_target_properties(${target} PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "YES"
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "YES"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Distribution"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${CUBEULATOR_IOS_SIGNING_TEAM}"
        XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER "${CUBEULATOR_IOS_SIGNING_PROFILE}")
elseif(CUBEULATOR_IOS_SIGNING_TEAM OR CUBEULATOR_IOS_SIGNING_PROFILE)
    message(FATAL_ERROR
        "CUBEULATOR_IOS_SIGNING_TEAM and CUBEULATOR_IOS_SIGNING_PROFILE go together: "
        "one is set and the other is empty. Give both for a signed build, or neither "
        "for an unsigned one (docs/ios-install.md).")
endif()

# --- the plugins a static Qt links ---------------------------------------------
# Qt's iOS kit is static: a plugin that is not named is not in the program. The
# camera's permission request is answered by QDarwinCameraPermissionPlugin, so it
# is named outright whatever default the kit gives it.
cube_ios_default_plugins()
if(TARGET Qt6::QDarwinCameraPermissionPlugin)
    qt_import_plugins(${target} INCLUDE Qt6::QDarwinCameraPermissionPlugin)
endif()
