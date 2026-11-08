find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
include(ECMAddTests)

add_library(kdosfirewallfakes STATIC fakeservices.cpp)
target_link_libraries(kdosfirewallfakes PUBLIC kdosfirewall Qt6::DBus)

ecm_add_test(firewallcontrollertest.cpp
    TEST_NAME firewallcontrollertest
    LINK_LIBRARIES kdosfirewall kdosfirewallfakes Qt6::Test)

ecm_add_test(fwasktest.cpp
    TEST_NAME fwasktest
    LINK_LIBRARIES kdosfirewallfakes Qt6::Test)
target_compile_definitions(fwasktest PRIVATE KDOS_FWASK_BINARY="$<TARGET_FILE:kdos-fwask>")
add_dependencies(fwasktest kdos-fwask)

add_executable(fwcoretest fwcoretest.c ../fwask/fwcore.c ${KDOS_LIBKBASE})
set_target_properties(fwcoretest PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_compile_definitions(fwcoretest PRIVATE _GNU_SOURCE)
target_include_directories(fwcoretest PRIVATE ${KDOS_LIBS}/libkbase ../fwask)
add_test(NAME fwcoretest COMMAND fwcoretest ${CMAKE_CURRENT_BINARY_DIR}/fwcore-scratch)
