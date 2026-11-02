// chordcheck fixture: a KDOS effect whose ShortcutHandler takes Meta+Q,
// which plasmashell keeps for Manage Activities. The braces in the handler
// and in the string below must not end the block early.
import QtQuick
import org.kde.kwin

SceneEffect {
    id: effect
    readonly property string note: "a } in a string"

    ShortcutHandler {
        name: "KDOS Fixture"
        text: "Fixture"
        sequence: "Meta+Q"
        onActivated: { if (effect.visible) { effect.visible = false; } }
    }
}
