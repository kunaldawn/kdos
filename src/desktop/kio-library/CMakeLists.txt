# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   kio-library — library:/, the menu's Library category, the Places entry
# ---------------------------------

cmake_minimum_required(VERSION 3.22)
project(kio-library VERSION 1.0.0 LANGUAGES CXX)

set(QT_MIN_VERSION "6.10.0")
set(KF_MIN_VERSION "6.22.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Sql)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS CoreAddons I18n KIO)

add_definitions(-DTRANSLATION_DOMAIN=\"kdos-kio-library\")

# What the library holds, as a library of its own so the tests link it with
# no worker, no session and no files of a real machine.
add_library(kdoslibrary STATIC src/library.cpp)
set_target_properties(kdoslibrary PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(kdoslibrary PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(kdoslibrary PUBLIC Qt6::Core Qt6::Sql KF6::I18n)

kcoreaddons_add_plugin(kio_library SOURCES src/libraryworker.cpp INSTALL_NAMESPACE "kf6/kio")
target_link_libraries(kio_library PRIVATE kdoslibrary KF6::KIOCore KF6::I18n)

add_executable(kdos-library-place src/placehelper.cpp)
target_link_libraries(kdos-library-place PRIVATE Qt6::Gui KF6::KIOFileWidgets KF6::I18n)
install(TARGETS kdos-library-place DESTINATION ${KDE_INSTALL_LIBEXECDIR}/kdos)

install(FILES menu/kdos-library.menu DESTINATION ${KDE_INSTALL_SYSCONFDIR}/xdg/menus/applications-merged)
install(FILES menu/kdos-library.directory DESTINATION ${KDE_INSTALL_DESKTOPDIR})
install(FILES menu/kdos-library-place.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
    include(ECMAddTests)
    ecm_add_test(tests/librarytest.cpp
        TEST_NAME librarytest
        LINK_LIBRARIES kdoslibrary Qt6::Test Qt6::Sql)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
