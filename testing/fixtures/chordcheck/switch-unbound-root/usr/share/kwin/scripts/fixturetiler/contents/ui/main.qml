// chordcheck fixture: an upstream script that is off, whose chord an
// upstream desktop entry already holds, and which a KDOS kwinrc names under
// [Plugins] — KDOS ships its switch, so the collision is KDOS's to settle.
// The entry reaches the tree through an absolute link, as the image's
// usr/share/kglobalaccel entries do.
import QtQuick
import org.kde.kwin

Item {
    ShortcutHandler {
        name: "FixtureGrab"
        text: "Fixture: Grab"
        sequence: "Meta+Alt+Shift+F11"
    }
}
