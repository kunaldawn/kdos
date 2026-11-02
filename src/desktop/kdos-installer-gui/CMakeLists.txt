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

project(kdos-installer-gui VERSION 1.0.0 LANGUAGES CXX)

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

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core DBus Gui Qml Quick QuickControls2 Widgets)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS ColorScheme Config CoreAddons I18n Kirigami)
find_package(KF6ItemModels ${KF6_MIN_VERSION} REQUIRED)

ecm_set_disabled_deprecation_versions(QT 6.11.0 KF 6.31.0)

add_definitions(-DTRANSLATION_DOMAIN=\"kdos-installer-gui\")

set(KDOS_KINSTALL "/usr/bin/kinstall" CACHE STRING "The kinstall binary the pkexec policy names")

qt_policy(SET QTP0001 NEW)
qt_standard_project_setup(REQUIRES 6.8)

add_library(kdosinstaller STATIC
    answers.cpp answers.h
    eventstream.cpp eventstream.h
    keyboardmodel.cpp keyboardmodel.h
    backend.cpp backend.h
)
set_property(TARGET kdosinstaller PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(kdosinstaller PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(kdosinstaller PRIVATE KDOS_KINSTALL="${KDOS_KINSTALL}")
target_link_libraries(kdosinstaller PUBLIC
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Qml
    KF6::ColorScheme
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::I18n
)

qt_add_executable(kdos-installer-gui main.cpp)
target_compile_definitions(kdos-installer-gui PRIVATE KDOS_INSTALLER_VERSION="${PROJECT_VERSION}")

qt_add_qml_module(kdos-installer-gui
    URI org.kdos.installer
    VERSION 1.0
    NO_IMPORT_SCAN
    SOURCES answers.h eventstream.h keyboardmodel.h backend.h
    QML_FILES
        qml/Main.qml
        qml/WizardPage.qml
        qml/WelcomePage.qml
        qml/KeyboardPage.qml
        qml/DiskPage.qml
        qml/EncryptionPage.qml
        qml/AccountPage.qml
        qml/LookPage.qml
        qml/AppsPage.qml
        qml/SummaryPage.qml
        qml/InstallPage.qml
        qml/DonePage.qml
)

target_link_libraries(kdos-installer-gui PRIVATE
    kdosinstaller
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    KF6::I18nQml
)

install(TARGETS kdos-installer-gui ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
# The live medium's boot step puts these where the session and polkit find
# them; an installed system carries them here only, where nothing reads them.
install(FILES kdos-installer.desktop 49-kdos-installer-live.rules
        DESTINATION ${KDE_INSTALL_DATADIR}/kdos/installer)
install(FILES io.kdos.installer.policy DESTINATION ${KDE_INSTALL_DATADIR}/polkit-1/actions)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
