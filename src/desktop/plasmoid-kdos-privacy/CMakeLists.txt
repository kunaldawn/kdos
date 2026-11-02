# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   plasmoid-kdos-privacy — the privacy lamps, org.kdos.privacy
# ---------------------------------

cmake_minimum_required(VERSION 3.22)
project(plasmoid-kdos-privacy VERSION 1.0.0 LANGUAGES CXX)

set(QT_MIN_VERSION "6.10.0")
set(KF_MIN_VERSION "6.22.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS CoreAddons I18n)
find_package(Plasma 6.8 REQUIRED)

add_definitions(-DTRANSLATION_DOMAIN=\"kdos-plasmoid-privacy\")

# What the helper says and the watch that keeps it running, as a library of
# its own so the tests link it without a shell to load it into.
add_library(kdosprivacy STATIC src/privacystate.cpp src/privacywatch.cpp)
set_target_properties(kdosprivacy PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(kdosprivacy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(kdosprivacy PUBLIC Qt6::Core KF6::I18n)

plasma_add_applet(org.kdos.privacy
    QML_SOURCES
        qml/main.qml
        qml/Lamp.qml
        qml/FullRepresentation.qml
    CPP_SOURCES
        src/privacyapplet.cpp
)
target_link_libraries(org.kdos.privacy PRIVATE kdosprivacy Qt6::Gui Qt6::Qml KF6::CoreAddons KF6::I18n Plasma::Plasma)

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
    include(ECMAddTests)
    ecm_add_test(tests/privacytest.cpp
        TEST_NAME privacytest
        LINK_LIBRARIES kdosprivacy Qt6::Test)
    target_compile_definitions(privacytest PRIVATE KDOS_FAKE_KDOS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fake")
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
