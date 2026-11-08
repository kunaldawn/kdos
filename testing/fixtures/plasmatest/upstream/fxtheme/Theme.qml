import QtQuick

// Upstream's file, as a package the image has would install it. Its last
// line reads a member Item does not have, which qmllint reports; the line is
// upstream's, not KDOS's.
Item {
    id: root

    property Item box: Item {}
    readonly property int length: root.box.text.length
}
