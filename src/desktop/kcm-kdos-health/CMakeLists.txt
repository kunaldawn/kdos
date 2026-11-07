cmake_minimum_required(VERSION 3.22)
project(kcm-kdos-health VERSION 1.0.0 LANGUAGES CXX)

set(QT_MIN_VERSION 6.10.0)
set(KF_MIN_VERSION 6.22.0)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS CoreAddons I18n KCMUtils)

add_definitions(-DTRANSLATION_DOMAIN=\"kdos-kcm-health\")

add_library(kdoshealth STATIC
    doctormodel.cpp
    healthcontroller.cpp
)
set_target_properties(kdoshealth PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(kdoshealth PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kdoshealth PUBLIC Qt6::Core Qt6::Gui KF6::I18n)

kcmutils_add_qml_kcm(kcm_kdos_health SOURCES healthkcm.cpp INSTALL_NAMESPACE "plasma/kcms")
target_link_libraries(kcm_kdos_health PRIVATE kdoshealth KF6::CoreAddons KF6::KCMUtilsQuick Qt6::Qml)

install(PROGRAMS
    about/about-slot
    about/about-update
    about/about-accent
    about/about-tree
    about/about-libc
    about/about-boxes
    DESTINATION ${KDE_INSTALL_LIBEXECDIR}/kdos)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
