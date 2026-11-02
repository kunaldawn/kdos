cmake_minimum_required(VERSION 3.22)
project(kcm-kdos-look VERSION 1.0.0 LANGUAGES CXX)

set(QT_MIN_VERSION 6.8.0)
set(KF_MIN_VERSION 6.22.0)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core DBus Gui Qml Quick)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS ColorScheme Config CoreAddons I18n KCMUtils)

add_definitions(-DTRANSLATION_DOMAIN=\"kdos-kcm-look\")

add_library(kdoslook STATIC
    kdoscommand.cpp
    kdedclient.cpp
    accentmodel.cpp
    lookcontroller.cpp
)
set_target_properties(kdoslook PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(kdoslook PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kdoslook PUBLIC Qt6::Core Qt6::DBus Qt6::Gui KF6::ColorScheme KF6::ConfigCore KF6::CoreAddons KF6::I18n KF6::KCMUtils)

kcmutils_add_qml_kcm(kcm_kdos_look SOURCES lookkcm.cpp)
target_link_libraries(kcm_kdos_look PRIVATE kdoslook KF6::CoreAddons KF6::KCMUtilsQuick Qt6::Qml)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
