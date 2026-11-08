# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   kdos-transcribe — "Transcribe" in Dolphin's menu for audio and video
# ---------------------------------

cmake_minimum_required(VERSION 3.22)
project(kdos-transcribe VERSION 1.0.0 LANGUAGES CXX)

set(QT_MIN_VERSION "6.10.0")
set(KF_MIN_VERSION "6.22.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Widgets)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS CoreAddons I18n KIO)

add_definitions(-DTRANSLATION_DOMAIN=\"kdos-transcribe\")

add_library(kdostranscribelogic STATIC src/transcribe.cpp)
set_target_properties(kdostranscribelogic PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(kdostranscribelogic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(kdostranscribelogic PUBLIC Qt6::Core)

kcoreaddons_add_plugin(kdostranscribe SOURCES src/transcribeaction.cpp INSTALL_NAMESPACE "kf6/kfileitemaction")
target_link_libraries(kdostranscribe PRIVATE kdostranscribelogic Qt6::Widgets KF6::KIOWidgets KF6::I18n KF6::CoreAddons)

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
    include(ECMAddTests)
    ecm_add_test(tests/transcribetest.cpp
        TEST_NAME transcribetest
        LINK_LIBRARIES kdostranscribelogic Qt6::Test)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
