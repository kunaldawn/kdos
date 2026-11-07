# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   plasmoid-kdos-checklist — the first-run checklist: the QML module
#   org.kdos.checklist and its tray entry, org.kdos.checklist
# ---------------------------------

cmake_minimum_required(VERSION 3.22)
project(plasmoid-kdos-checklist VERSION 1.0.0 LANGUAGES CXX)

set(QT_MIN_VERSION "6.10.0")
set(KF_MIN_VERSION "6.22.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQmlModule)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Qml Quick)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS CoreAddons I18n)
find_package(Plasma 6.8 REQUIRED)

add_definitions(-DTRANSLATION_DOMAIN=\"kdos-plasmoid-checklist\")

# The model and the view, as a QML module of their own: the tray entry and
# the welcome tour's last page both import org.kdos.checklist.
ecm_add_qml_module(kdoschecklist URI "org.kdos.checklist" GENERATE_PLUGIN_SOURCE)
target_sources(kdoschecklist PRIVATE src/checklist.cpp)
target_include_directories(kdoschecklist PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
ecm_target_qml_sources(kdoschecklist SOURCES qml/ChecklistView.qml)
target_link_libraries(kdoschecklist PRIVATE Qt6::Core Qt6::Qml Qt6::Quick KF6::I18n)
ecm_finalize_qml_module(kdoschecklist DESTINATION ${KDE_INSTALL_QMLDIR})

add_subdirectory(applet)

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
    include(ECMAddTests)
    add_library(kdoschecklisttest STATIC src/checklist.cpp)
    target_include_directories(kdoschecklisttest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(kdoschecklisttest PUBLIC Qt6::Core Qt6::Qml KF6::I18n)
    ecm_add_test(tests/checklisttest.cpp
        TEST_NAME checklisttest
        LINK_LIBRARIES kdoschecklisttest Qt6::Test)
    target_compile_definitions(checklisttest PRIVATE KDOS_FAKE_KDOS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fake")
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
