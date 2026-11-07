# kdos-discover — Discover's KDOS backend, its update-tray notifier plugin and the
# AppStream catalogue generator for host ports.
#
# libdiscover is private to Discover: its headers are installed only because the
# discover recipe copies them to /usr/include/discover, and both plugin
# interfaces carry discover's full version in their IID. Discover's loaders skip
# a plugin whose IID differs, without an error a user would see, so this
# configure refuses any discover other than KDOS_DISCOVER_VERSION. A discover
# bump, point releases included, changes that line after the backend and the
# notifier are re-tested against the new headers.

cmake_minimum_required(VERSION 3.22)

project(kdos-discover VERSION 0.1.0 LANGUAGES C CXX)

set(KDOS_DISCOVER_VERSION "6.8.2")

set(QT_MIN_VERSION "6.11.0")
set(KF6_MIN_VERSION "6.30.0")
set(KDE_COMPILERSETTINGS_LEVEL "5.85")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQtDeclareLoggingCategory)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED CONFIG COMPONENTS Core Gui DBus Network Qml)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS CoreAddons Config I18n Archive Service KIO JobWidgets)
find_package(AppStreamQt 1.0.4 CONFIG REQUIRED)
find_package(QCoro6 REQUIRED COMPONENTS Core)
if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED CONFIG COMPONENTS Test Widgets)
endif()

find_path(DISCOVER_INCLUDE_DIR NAMES resources/AbstractResourcesBackend.h PATH_SUFFIXES discover REQUIRED)
find_library(DISCOVER_COMMON_LIBRARY NAMES DiscoverCommon PATH_SUFFIXES plasma-discover REQUIRED)
find_library(DISCOVER_NOTIFIERS_LIBRARY NAMES DiscoverNotifiers PATH_SUFFIXES plasma-discover REQUIRED)

file(STRINGS "${DISCOVER_INCLUDE_DIR}/DiscoverConfig.h" _kdos_iid REGEX "define DISCOVER_PLUGIN_IID")
string(REGEX MATCH "org\\.kde\\.discover\\.([^\"]+)\\.AbstractResourcesBackendFactory" _kdos_match "${_kdos_iid}")
if(NOT CMAKE_MATCH_1 STREQUAL KDOS_DISCOVER_VERSION)
    message(FATAL_ERROR "kdos-discover is written against discover ${KDOS_DISCOVER_VERSION}; "
        "${DISCOVER_INCLUDE_DIR}/DiscoverConfig.h is discover '${CMAKE_MATCH_1}'. "
        "Re-test the backend and the notifier against that release, then change KDOS_DISCOVER_VERSION.")
endif()

get_filename_component(DISCOVER_LIBRARY_DIR "${DISCOVER_COMMON_LIBRARY}" DIRECTORY)

add_library(Discover::Common SHARED IMPORTED)
set_target_properties(Discover::Common PROPERTIES
    IMPORTED_LOCATION "${DISCOVER_COMMON_LIBRARY}"
    IMPORTED_SONAME "libDiscoverCommon.so"
    INTERFACE_INCLUDE_DIRECTORIES "${DISCOVER_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "Qt6::Core;Qt6::Qml;Qt6::Gui;KF6::I18n;QCoro6::Core"
)
add_library(Discover::Notifiers SHARED IMPORTED)
set_target_properties(Discover::Notifiers PROPERTIES
    IMPORTED_LOCATION "${DISCOVER_NOTIFIERS_LIBRARY}"
    IMPORTED_SONAME "libDiscoverNotifiers.so"
    INTERFACE_INCLUDE_DIRECTORIES "${DISCOVER_INCLUDE_DIR};${DISCOVER_INCLUDE_DIR}/notifiers"
    INTERFACE_LINK_LIBRARIES "Qt6::Core"
)

add_definitions(-DQT_NO_SIGNALS_SLOTS_KEYWORDS -DQT_NO_URL_CAST_FROM_STRING)
add_definitions(-DTRANSLATION_DOMAIN=\"kdos-discover\")
list(APPEND CMAKE_AUTOMOC_MACRO_NAMES "DISCOVER_BACKEND_PLUGIN")

add_subdirectory(model)
add_subdirectory(backend)
add_subdirectory(notifier)
add_subdirectory(catalog)
add_subdirectory(data)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

ecm_qt_install_logging_categories(
    EXPORT KDOS_DISCOVER
    FILE kdos-discover.categories
    DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
