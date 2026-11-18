/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   50-Library.qml — what KDOS knows with no network
 *
 * The offline library is whatever the machine carries: Kiwix archives, the
 * KDOS book (and KDOS from Scratch once kdos-kfs is installed), manual
 * pages, Zeal docsets, maps. KRunner searches all of it, so the page teaches
 * its chord and the `lib` verb that narrows a search to the library. The
 * books paragraph says whether KDOS from Scratch is here, and the launchers
 * are only for the library applications this machine has: one it does not
 * have is left out rather than shown dead, and with none the heading over
 * them goes too.
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

    heading: i18ndc("kdos-welcome", "@title:window", "Offline Library")
    description: i18ndc("kdos-welcome", "@info:usagetip", "KDOS works with no network. Encyclopaedias, manuals and maps you install are searched from one place, and stay on this machine.")

    // KRunner's launch action carries several chords, which kglobalaccel
    // returns in no stable order: the page names the one the Keys page
    // teaches while it is bound, else the first bound one with a modifier,
    // so every open, and both pages, name the same chord.
    readonly property string krunner: {
        const bound = Welcome.Utils.globalShortcutTexts("org.kde.krunner.desktop", "_launch");
        if (bound.indexOf("Meta+Space") >= 0) {
            return "Meta+Space";
        }
        return bound.find(t => t.indexOf("+") > 0) ?? (bound.length > 0 ? bound[0] : "");
    }

    readonly property Welcome.ApplicationInfo kfs: Welcome.ApplicationInfo {
        desktopName: "kdos-library-kfs-book"
    }

    readonly property list<Welcome.ApplicationInfo> libraryApps: [
        Welcome.ApplicationInfo {
            desktopName: "org.kiwix.desktop"
        },
        Welcome.ApplicationInfo {
            desktopName: "org.kde.khelpcenter"
        },
        Welcome.ApplicationInfo {
            desktopName: "app.organicmaps.desktop"
        },
        Welcome.ApplicationInfo {
            desktopName: "org.zealdocs.zeal"
        },
        Welcome.ApplicationInfo {
            desktopName: "org.kde.marble"
        }
    ]
    readonly property bool anyLibraryApp: libraryApps.some(application => application.exists)

    view: Kdos.Flick {
        Kdos.Point {
            icon: "search"
            title: i18ndc("kdos-welcome", "@title", "Search everything")
            text: i18ndc("kdos-welcome", "@info %1 is a keyboard shortcut", "Press %1 and type: matching articles, book pages, manual pages and documentation appear beside your applications and files. Start with <b>lib</b> and your words to search only the library.", page.krunner)
        }

        Kdos.Point {
            icon: "help-contents"
            title: i18ndc("kdos-welcome", "@title", "The KDOS books")
            text: page.kfs.exists
                ? i18ndc("kdos-welcome", "@info", "The KDOS book explains every part of the system, and <i>KDOS from Scratch</i> how it is built. Both are on this machine, and open in the browser from the Help page.")
                : i18ndc("kdos-welcome", "@info", "The KDOS book explains every part of the system. It is on this machine, and opens in the browser from the Help page. <i>KDOS from Scratch</i>, how the system is built, is installed with <b>sudo kdos update install kdos-kfs</b>.")
        }

        Kirigami.Heading {
            level: 3
            visible: page.anyLibraryApp
            text: i18ndc("kdos-welcome", "@title:group", "On this machine")
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: page.anyLibraryApp
                ? i18ndc("kdos-welcome", "@info", "Archives, maps and docsets are packs in Discover; the applications that open them are below.")
                : i18ndc("kdos-welcome", "@info", "Archives, maps and docsets are packs in Discover; the applications that open them, such as Kiwix and Zeal, are installed from Discover too.")
        }

        Flow {
            id: apps

            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            Accessible.role: Accessible.Grouping
            Accessible.name: i18ndc("kdos-welcome", "@title:group", "Library applications")

            Repeater {
                model: page.libraryApps

                // A tile of its own rather than Welcome.ApplicationIcon: under
                // the desktop style a ToolButton's background paints the
                // button's icon and text itself, so a button that also draws
                // them in its content shows both twice. This one gives the
                // style neither, and draws them once in its content.
                delegate: QQC2.ToolButton {
                    id: tile

                    required property Welcome.ApplicationInfo modelData

                    readonly property Welcome.ApplicationInfo application: modelData

                    visible: application.exists
                    display: QQC2.AbstractButton.IconOnly
                    onClicked: Welcome.Utils.launchApp(application.desktopName)

                    contentItem: ColumnLayout {
                        spacing: Kirigami.Units.largeSpacing

                        Kirigami.Icon {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: Kirigami.Units.iconSizes.huge
                            Layout.preferredHeight: Kirigami.Units.iconSizes.huge
                            source: tile.application.icon || "application-x-executable"
                        }

                        Kirigami.Heading {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.bottomMargin: Kirigami.Units.gridUnit
                            text: tile.application.name ?? ""
                            wrapMode: Text.WordWrap
                            level: 3
                        }
                    }

                    Accessible.name: application.name ?? ""
                    Accessible.description: i18ndc("kdos-welcome", "@info:tooltip %1 is an application", "Launch %1 now", application.name ?? "")

                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: i18ndc("kdos-welcome", "@info:tooltip %1 is an application", "Launch %1 now", application.name ?? "")
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                }
            }
        }
    }
}
