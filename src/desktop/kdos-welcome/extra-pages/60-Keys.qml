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
 * Each row is read from kglobalaccel (Utils.globalShortcutTexts, from
 * plasma-welcome's shortcut-texts.patch), never typed here, so a chord the
 * person rebound shows as they bound it, and one nobody bound is left out
 * instead of being promised. An action bound to several chords shows the
 * one KDOS teaches (the fourth field of a row) while it is bound, and
 * otherwise the first bound chord with a modifier: kglobalaccel returns an
 * action's chords in no stable order, so "the first" alone would change
 * from one session to the next.
 *
 * The rows are read again whenever the page is shown and whenever the
 * window comes back to the front. An action is registered only once the
 * program that owns it has started, which at a first login can be after
 * this page was made (the clipboard history's), and a chord rebound in
 * System Settings is shown when the person comes back.
 *
 * The chord column is as wide as the widest chord, so a long one is never
 * cut; it elides only in a window too narrow for it.
 *
 * The list is short on purpose; the key card (the first row) shows all of
 * them, and System Settings → Shortcuts changes them.
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

    // component, action id, what it does, the chord KDOS teaches for it
    readonly property var chords: [
        ["kwin", "KDOS Key Card", i18ndc("kdos-welcome", "@info what a shortcut does", "Show the key card: every chord, searchable"), "Meta+F1"],
        ["org.kde.krunner.desktop", "_launch", i18ndc("kdos-welcome", "@info what a shortcut does", "Search, run a command, or type a two-letter app code"), "Meta+Space"],
        ["org.kde.konsole.desktop", "_launch", i18ndc("kdos-welcome", "@info what a shortcut does", "Open a terminal"), "Ctrl+Alt+T"],
        ["kwin", "Overview", i18ndc("kdos-welcome", "@info what a shortcut does", "See every window and desktop"), "Meta+W"],
        ["kwin", "Window Close", i18ndc("kdos-welcome", "@info what a shortcut does", "Close the window"), "Meta+Q"],
        ["kwin", "Show Desktop", i18ndc("kdos-welcome", "@info what a shortcut does", "Peek at the desktop"), "Meta+D"],
        ["plasmashell", "show-on-mouse-pos", i18ndc("kdos-welcome", "@info what a shortcut does", "Paste from the clipboard history"), "Meta+V"],
        ["org.kde.spectacle.desktop", "RectangularRegionScreenShot", i18ndc("kdos-welcome", "@info what a shortcut does", "Take a screenshot of a region"), "Shift+Print"],
        ["org.kde.plasma.emojier.desktop", "_launch", i18ndc("kdos-welcome", "@info what a shortcut does", "Insert an emoji"), "Meta+."],
        ["ksmserver", "Lock Session", i18ndc("kdos-welcome", "@info what a shortcut does", "Lock the screen"), "Meta+L"],
    ]

    // Bumped to make every row read its chords again.
    property int generation: 0
    readonly property bool shown: page.isCurrentPage && Window.active
    onShownChanged: if (shown) generation++

    function pick(bound: var, taught: string): string {
        if (bound.indexOf(taught) >= 0) {
            return taught;
        }
        return bound.find(t => t.indexOf("+") > 0) ?? (bound.length > 0 ? bound[0] : "");
    }

    // The widest chord's text: each row's first child is its chord label,
    // whose text is empty in a row with no chord.
    readonly property real chordWidth: {
        let w = Kirigami.Units.gridUnit * 6;
        for (let i = 0; i < rows.count; i++) {
            const r = rows.itemAt(i);
            if (r && r.children.length > 0) {
                w = Math.max(w, r.children[0].implicitWidth);
            }
        }
        return w;
    }

    view: Kdos.Flick {
        Repeater {
            id: rows

            model: page.chords

            delegate: RowLayout {
                id: row

                required property var modelData

                readonly property var bound: page.generation >= 0 ? Welcome.Utils.globalShortcutTexts(modelData[0], modelData[1]) : []
                readonly property string chord: page.pick(bound, modelData[3])

                visible: chord.length > 0
                Layout.fillWidth: true
                spacing: Kirigami.Units.largeSpacing * 2

                Accessible.role: Accessible.Row
                Accessible.name: i18ndc("kdos-welcome", "@info accessible %1 is a chord, %2 what it does", "%1: %2", chord, modelData[2])

                QQC2.Label {
                    Layout.preferredWidth: page.chordWidth
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
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
            onClicked: run.launch(["kcmshell6", "kcm_keys"], (code, text) => {})
            Accessible.name: text
        }
    }
}
