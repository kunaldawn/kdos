# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   ksystemstats-kdos — the KDOS sensors, sensor face and System Monitor page
# ---------------------------------

cmake_minimum_required(VERSION 3.22)
project(ksystemstats-kdos VERSION 1.0.0 LANGUAGES CXX)

set(QT_MIN_VERSION "6.10.0")
set(KF_MIN_VERSION "6.22.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Network)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS Config CoreAddons I18n Package)
find_package(KSysGuard 6.8 REQUIRED)

add_definitions(-DTRANSLATION_DOMAIN=\"kdos-ksystemstats-kdos\")

# What the sensors are read from, as a library of its own so the tests link
# it without a daemon to load it into.
add_library(kdosstats STATIC src/kdosstats.cpp)
set_target_properties(kdosstats PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(kdosstats PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(kdosstats PUBLIC Qt6::Core)

add_library(ksystemstats_plugin_kdos MODULE src/plugin.cpp)
target_link_libraries(ksystemstats_plugin_kdos PRIVATE
    kdosstats
    Qt6::Network
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::I18n
    KSysGuard::SystemStats)
install(TARGETS ksystemstats_plugin_kdos DESTINATION ${KDE_INSTALL_PLUGINDIR}/ksystemstats)

kpackage_install_package(face org.kdos.meters sensorfaces ksysguard)

install(FILES page/kdos-overview.page page/kdos.page DESTINATION ${KDE_INSTALL_DATADIR}/plasma-systemmonitor)

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
    include(ECMAddTests)
    ecm_add_test(tests/kdosstatstest.cpp
        TEST_NAME kdosstatstest
        LINK_LIBRARIES kdosstats Qt6::Test)
    # The plugin's sources built into the test: a MODULE cannot be linked.
    ecm_add_test(tests/kdosplugintest.cpp src/plugin.cpp
        TEST_NAME kdosplugintest
        LINK_LIBRARIES kdosstats Qt6::Network Qt6::Test KF6::ConfigCore KF6::CoreAddons KF6::I18n KSysGuard::SystemStats KSysGuard::Formatter)
    target_include_directories(kdosplugintest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
