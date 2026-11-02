find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
include(ECMAddTests)

ecm_add_test(looktest.cpp
    TEST_NAME looktest
    LINK_LIBRARIES kdoslook Qt6::Test Qt6::Gui)
target_compile_definitions(looktest PRIVATE KDOS_FAKE_KDOS="${CMAKE_CURRENT_SOURCE_DIR}/fake-kdos")
