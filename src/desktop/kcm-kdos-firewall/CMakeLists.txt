cmake_minimum_required(VERSION 3.22)
project(kcm-kdos-firewall VERSION 1.0.0 LANGUAGES C CXX)

set(QT_MIN_VERSION 6.8.0)
set(KF_MIN_VERSION 6.22.0)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core DBus Gui Qml Quick)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS CoreAddons I18n KCMUtils)
find_package(PkgConfig REQUIRED)
pkg_check_modules(BASU REQUIRED IMPORTED_TARGET basu)

add_definitions(-DTRANSLATION_DOMAIN=\"kdos-kcm-firewall\")

set(KDOS_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/../../libs)

add_library(kdosfirewall STATIC
    firewalltypes.cpp
    power1client.cpp
    polkitcheck.cpp
    servicemodel.cpp
    firewallcontroller.cpp
)
set_target_properties(kdosfirewall PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(kdosfirewall PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kdosfirewall PUBLIC Qt6::Core Qt6::DBus KF6::I18n)

kcmutils_add_qml_kcm(kcm_kdos_firewall SOURCES firewallkcm.cpp)
target_link_libraries(kcm_kdos_firewall PRIVATE kdosfirewall KF6::CoreAddons KF6::KCMUtilsQuick Qt6::Qml)

file(GLOB KDOS_LIBKBASE ${KDOS_LIBS}/libkbase/*.c)
add_executable(kdos-fwask fwask/main.c fwask/fwcore.c ${KDOS_LIBKBASE})
set_target_properties(kdos-fwask PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_compile_definitions(kdos-fwask PRIVATE _GNU_SOURCE)
target_include_directories(kdos-fwask PRIVATE ${KDOS_LIBS}/libkbase fwask)
target_link_libraries(kdos-fwask PRIVATE PkgConfig::BASU)
install(TARGETS kdos-fwask DESTINATION ${KDE_INSTALL_LIBEXECDIR}/kdos)
configure_file(fwask/kdos-fwask.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/kdos-fwask.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kdos-fwask.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
