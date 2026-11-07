// The colour-literal check's fixture: every line ending in `// literal` sets
// a colour from a literal and must be reported; no other line may be. The
// line ending in `// allowed` is let through by the one allowlist entry the
// check is given for this file, and the literal after it on the same line
// still is not.
import QtQuick
import org.kde.kirigami as Kirigami

Rectangle {
    color: Kirigami.Theme.backgroundColor
    border.color: "#00ff00" // literal
    property color accent: '#80ffffff' // literal
    property string issue: "Issue #123456 is not a colour"
    property url page: "https://example.invalid/book#abcdef"
    /* "#abcdef" inside a block comment is no colour,
       and neither is Qt.rgba(1, 0, 0, 1) */
    // nor "#ff0000" in a line comment

    Text {
        color: Kirigami.Theme.textColor
        text: i18n("Ready")
    }
    Rectangle { color: Qt.rgba(1, 0, 0, 0.5) } // literal
    Rectangle { color: Qt.hsla(0.5, 1, 0.5, 1) } // literal
    Rectangle { color: Qt.rgba(0, 0, 0, 1) } // allowed
    Rectangle { color: Qt.rgba(0, 0, 0, 1); border.color: Qt.rgba(0, 0, 0, 0.5) } // literal
    Rectangle { color: "red" } // literal
    Rectangle { color: "transparent" }
    Rectangle { color: Qt.alpha(Kirigami.Theme.highlightColor, 0.3) }
    Rectangle { color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.2) }
    Rectangle { border.color: Qt.lighter(Kirigami.Theme.highlightColor, 1.2) }
}
