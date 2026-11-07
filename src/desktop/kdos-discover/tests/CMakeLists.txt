include(ECMAddTests)

ecm_add_test(modeltest.cpp
    TEST_NAME kdos-discover-modeltest
    LINK_LIBRARIES kdos-discover-model Qt6::Test
)

# The backend as Discover drives it, against fake tools on PATH, a fake
# kdos-packd socket and fixture catalogue roots.
ecm_add_test(backendtest.cpp ../backend/resources.qrc
    TEST_NAME kdos-discover-backendtest
    LINK_LIBRARIES kdos-backend-core Qt6::Test
)

# The tray plugin, compiled into its test: the plugin library itself is
# loaded only through Discover's notifier factory.
set(notifiertest_SRCS notifiertest.cpp ../notifier/KdosNotifier.cpp)
ecm_qt_declare_logging_category(notifiertest_SRCS
    HEADER kdos_notifier_debug.h
    IDENTIFIER KDOS_NOTIFIER_LOG
    CATEGORY_NAME org.kdos.discover.notifier
)
ecm_add_test(${notifiertest_SRCS}
    TEST_NAME kdos-discover-notifiertest
    LINK_LIBRARIES kdos-discover-model Discover::Notifiers Qt6::Test Qt6::Widgets Qt6::DBus KF6::CoreAddons KF6::I18n KF6::JobWidgets
)
target_include_directories(kdos-discover-notifiertest PRIVATE ${CMAKE_SOURCE_DIR}/notifier)
