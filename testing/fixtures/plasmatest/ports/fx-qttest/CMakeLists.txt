cmake_minimum_required(VERSION 3.22)
project(kdos-plasmatest-fixture LANGUAGES CXX)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMAddTests)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Test)

add_library(kdosfixture MODULE plugin/fixtureplugin.cpp)
target_link_libraries(kdosfixture PRIVATE Qt6::Core)
install(TARGETS kdosfixture DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/kdosfixture)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
