# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

cmake_minimum_required(VERSION 3.22)

project(kcm-kdos-boxes VERSION 1.0.0 LANGUAGES CXX)

set(QT_MIN_VERSION "6.11.0")
set(KF6_MIN_VERSION "6.31.0")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)
include(ECMDeprecationSettings)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS ColorScheme Config CoreAddons I18n KCMUtils Service)

ecm_set_disabled_deprecation_versions(QT 6.11.0 KF 6.31.0)

# One domain for the C++ and the QML: KQuickConfigModule::mainUi() sets the
# QML engine's translation domain to the plugin id.
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kdos_boxes\")

add_library(kcm_kdos_boxes_static STATIC
    accentmodel.cpp accentmodel.h
    boxesmodule.cpp boxesmodule.h
    boxlistmodel.cpp boxlistmodel.h
    boxprofile.cpp boxprofile.h
    boxtool.cpp boxtool.h
    confinement.cpp confinement.h
)
set_property(TARGET kcm_kdos_boxes_static PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(kcm_kdos_boxes_static PUBLIC
    KF6::ColorScheme
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::I18n
    KF6::KCMUtilsQuick
    KF6::Service
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
)

kcmutils_add_qml_kcm(kcm_kdos_boxes SOURCES plugin.cpp)
target_link_libraries(kcm_kdos_boxes PRIVATE kcm_kdos_boxes_static)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
