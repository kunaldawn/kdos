# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   kded-kdos — the KDOS kded6 module
# ---------------------------------

cmake_minimum_required(VERSION 3.22)
project(kded-kdos VERSION 1.0.0 LANGUAGES CXX)

set(QT_MIN_VERSION "6.8.0")
set(KF_MIN_VERSION "6.20.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQtDeclareLoggingCategory)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core DBus Concurrent)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    Config CoreAddons DBusAddons I18n Notifications Solid)

add_definitions(-DTRANSLATION_DOMAIN=\"kdos-kded-kdos\")

# The decisions, as a library of their own so the tests link them without a
# session, a bus or a kded to load them into.
add_library(kdedkdoslogic STATIC src/logic.cpp)
set_target_properties(kdedkdoslogic PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(kdedkdoslogic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(kdedkdoslogic PUBLIC Qt6::Core)

kcoreaddons_add_plugin(kdos
    SOURCES src/module.cpp src/accentsync.cpp src/sharewatch.cpp src/stickoffer.cpp
    INSTALL_NAMESPACE "kf6/kded")
ecm_qt_declare_logging_category(kdos
    HEADER debug.h
    IDENTIFIER KDOS_KDED
    CATEGORY_NAME io.kdos.kded
    DEFAULT_SEVERITY Info
    DESCRIPTION "KDOS kded module")
target_link_libraries(kdos PRIVATE
    kdedkdoslogic
    Qt6::DBus
    Qt6::Concurrent
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::DBusAddons
    KF6::I18n
    KF6::Notifications
    KF6::Solid)

install(FILES src/kded_kdos.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
    include(ECMAddTests)
    ecm_add_test(tests/logictest.cpp
        TEST_NAME logictest
        LINK_LIBRARIES kdedkdoslogic Qt6::Test)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
