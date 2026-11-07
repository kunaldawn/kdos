add_executable(kdos-discover-catalog main.cpp)
target_link_libraries(kdos-discover-catalog PRIVATE Qt6::Core AppStreamQt)
install(TARGETS kdos-discover-catalog ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
