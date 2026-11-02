/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   60-Keys.qml — the chords worth knowing, as they are bound now
 *
 * Each row is read from kglobalaccel when the page opens
 * (Utils.globalShortcutText), never typed here, so a chord the person
 * rebound shows as they bound it, and one nobody bound is left out instead
 * of being promised. The list is short on purpose; the key card (the first
 * row) shows all of them, and System Settings → Shortcuts changes them.
 * ---------------------------------
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

import org.kde.plasma.welcome as Welcome

import "kdos" as Kdos

Welcome.ScrollablePage {
    id: page

    heading: i18ndc("kdos-welcome", "@title:window", "Keys")
    description: i18ndc("kdos-welcome", "@info:usagetip", "A few chords do most of the work. The key card lists every one, from the bindings in force.")

    Kdos.Run {
        id: run
    }

    // component, action id, what it does
    readonly property var chords: [
        ["kwin", "KDOS Key Card",i18ndc("kdos-welcome", "@info what a shortcut does", "Show the key card: every chord, searchable")],
        ["org.kde.krunner.desktop", "_launch", i18ndc("kdos-welcome", "@info what a shortcut does", "Search, run a command, or type a two-letter app code")],
        ["org.kde.konsole.desktop", "_launch", i18ndc("kdos-welcome", "@info what a shortcut does", "Open a terminal")],
        ["kwin", "Overview", i18ndc("kdos-welcome", "@info what a shortcut does", "See every window and desktop")],
        ["kwin", "Window Close", i18ndc("kdos-welcome", "@info what a shortcut does", "Close the window")],
        ["kwin", "Show Desktop", i18ndc("kdos-welcome", "@info what a shortcut does", "Peek at the desktop")],
        ["plasmashell", "show-on-mouse-pos", i18ndc("kdos-welcome", "@info what a shortcut does", "Paste from the clipboard history")],
        ["org.kde.spectacle.desktop", "RectangularRegionScreenShot", i18ndc("kdos-welcome", "@info what a shortcut does", "Take a screenshot of a region")],
        ["org.kde.plasma.emojier.desktop", "_launch", i18ndc("kdos-welcome", "@info what a shortcut does", "Insert an emoji")],
        ["ksmserver", "Lock Session", i18ndc("kdos-welcome", "@info what a shortcut does", "Lock the screen")],
    ]

    // globalShortcutText's word for an action with no chord, compared so
    // an unbound action is dropped rather than listed as "Unassigned".
    readonly property string unassigned: Welcome.Utils.globalShortcutText("", "")

    view: Kdos.Flick {
        Repeater {
            model: page.chords

            delegate: RowLayout {
                id: row

                required property var modelData

                readonly property string chord: Welcome.Utils.globalShortcutText(modelData[0], modelData[1])

                visible: chord.length > 0 && chord !== page.unassigned
                Layout.fillWidth: true
                spacing: Kirigami.Units.largeSpacing * 2

                Accessible.role: Accessible.Row
                Accessible.name: i18ndc("kdos-welcome", "@info accessible %1 is a chord, %2 what it does", "%1: %2", chord, modelData[2])

                QQC2.Label {
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 10
                    horizontalAlignment: Text.AlignRight
                    text: row.chord
                    font.family: Kirigami.Theme.fixedWidthFont.family
                    font.bold: true
                    textFormat: Text.PlainText
                    Accessible.ignored: true
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: row.modelData[2]
                    Accessible.ignored: true
                }
            }
        }

        QQC2.Button {
            text: i18ndc("kdos-welcome", "@action:button", "All Shortcuts…")
            icon.name: "preferences-desktop-keyboard-shortcut"
            onClicked: run.run(["kcmshell6", "kcm_keys"], () => {})
            Accessible.name: text
        }
    }
}
