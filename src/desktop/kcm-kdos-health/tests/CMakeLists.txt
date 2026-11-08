find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
include(ECMAddTests)

ecm_add_test(healthtest.cpp
    TEST_NAME healthtest
    LINK_LIBRARIES kdoshealth Qt6::Test Qt6::Gui)
target_compile_definitions(healthtest PRIVATE KDOS_FAKE_DOCTOR="${CMAKE_CURRENT_SOURCE_DIR}/fake-doctor")
