import QtQuick
import org.kde.discover as Discover

// Discover registers org.kde.discover in C++ and installs no qmldir for it;
// a backend's delegate imports it and reads its types through the alias.
Item {
    id: root

    required property Discover.AbstractResource resource

    Discover.Activatable.active: root.resource !== null
    Accessible.name: "fixture"
}
