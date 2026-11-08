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
 * KDOS and KFS books, manual pages, Zeal docsets, maps. KRunner searches all
 * of it, so the page teaches its chord and the `?` that narrows a search to
 * the library, and shows a launcher for each library application this image
 * has; one it does not have is left out rather than shown dead.
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

    readonly property string krunner: Welcome.Utils.globalShortcutText("org.kde.krunner.desktop", "_launch")

    view: Kdos.Flick {
        Kdos.Point {
            icon: "search"
            title: i18ndc("kdos-welcome", "@title", "Search everything")
            text: i18ndc("kdos-welcome", "@info %1 is a keyboard shortcut", "Press %1 and type: matching articles, book pages, manual pages and documentation appear beside your applications and files. Start with <b>?</b> to search only the library.", page.krunner)
        }

        Kdos.Point {
            icon: "help-contents"
            title: i18ndc("kdos-welcome", "@title", "The KDOS books")
            text: i18ndc("kdos-welcome", "@info", "The KDOS book explains every part of the system, and <i>KDOS from Scratch</i> how it is built. Both are installed, and open in the Help Center.")
        }

        Kirigami.Heading {
            level: 3
            text: i18ndc("kdos-welcome", "@title:group", "On this machine")
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: i18ndc("kdos-welcome", "@info", "Archives, maps and docsets are packs in Discover; the applications that open them are below when they are installed.")
        }

        Flow {
            id: apps

            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            Accessible.role: Accessible.Grouping
            Accessible.name: i18ndc("kdos-welcome", "@title:group", "Library applications")

            Repeater {
                model: ["org.kiwix.desktop", "org.kde.khelpcenter", "app.organicmaps.desktop", "org.zealdocs.zeal", "org.kde.marble"]

                delegate: Welcome.ApplicationIcon {
                    id: launcher

                    required property string modelData

                    application: Welcome.ApplicationInfo {
                        desktopName: launcher.modelData
                    }
                    size: Kirigami.Units.iconSizes.huge
                    visible: application.exists
                    Accessible.name: application.name
                }
            }
        }
    }
}
