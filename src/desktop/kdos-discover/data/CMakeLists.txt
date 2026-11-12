# Discover's featured list, kept on the machine; the Debian packages the
# Debian lane never offers; the MIME types of .ktar and .kpack; and the
# hidden entry that opens both in Discover.

install(FILES featuredurlrc DESTINATION ${KDE_INSTALL_DATADIR}/discover)
install(FILES featured.json DESTINATION ${KDE_INSTALL_DATADIR}/kdos/discover)
install(FILES kdos-discover.xml DESTINATION ${KDE_INSTALL_MIMEDIR})
install(FILES io.kdos.discover.packs.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES debian-exclude DESTINATION ${KDE_INSTALL_DATADIR}/kdos/discover)
