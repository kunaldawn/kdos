/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Splash.qml — KSplash for every KDOS global theme
 *
 * The session's splash continues the boot splash: it draws kdos-splash's
 * settled frame — the penguin and the block wordmark, the two lines under
 * them, the rule and the segmented bar — at the positions kdos-splash puts
 * them, and the bar fills with KSplash's six stages.
 *
 * THE LAYOUT IS kdos-splash.c's layout(), TERM FOR TERM, IN DEVICE PIXELS.
 * kdos-splash measures the framebuffer and stacks everything in whole pixels
 * from the top; this file does the same arithmetic on the screen's size in
 * device pixels and divides by the device pixel ratio only to place items, so
 * at any scale the mark lands on the pixels the boot splash left it on. A
 * change to that function is a change here too, or the handover jumps.
 *
 * The picture comes from mark.js, which genlook.py writes from kdos-splash's
 * own penguin.h and kdos-splash.c at build time; there is no second copy of
 * it. Every colour is a role of the running colour scheme, so one file serves
 * all eight accents and follows a scheme changed after the global theme; the
 * only others are the mascot's own two and the black of a screen with
 * nothing on it, which mark.js carries from the same sources.
 *
 * THE MOTION IS KDOS MODERN'S: the picture fades in out of black, flat, as
 * kdos-splash's Modern power-on does. Phosphor mode needs nothing here: its
 * CRT pass is KWin's, over the whole screen this splash included, so the
 * bleed and the scanlines land on this frame as they did on the boot
 * splash's, and its kdos_login effect unfolds the desktop when KSplash
 * closes. One file, one motion, no mode to read.
 *
 * KSplash gives the root item `stage`, 1 to 6, and loads nothing but QtQuick
 * and Kirigami; the process exits on stage 6. Reduced motion (an animation
 * factor of 0) shows the settled frame at once.
 * ---------------------------------
 */

pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami

import "mark.js" as Mark

Rectangle {
    id: root

    // Every handover of KDOS's power-on starts and ends on full black, so a
    // mode set falls inside a black frame and the picture rises out of it.
    // KSplash's window is transparent, so without this the desktop starting
    // underneath would show through the fade.
    color: Mark.screenOff

    property int stage

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    readonly property bool motion: Kirigami.Units.longDuration > 0

    // ── kdos-splash's layout(), in device pixels ────────────────────────

    readonly property real dpr: Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1
    readonly property int fbw: Math.round(width * dpr)
    readonly property int fbh: Math.round(height * dpr)

    // C's integer division, which truncates towards zero.
    function cdiv(a: int, b: int): int {
        return Math.trunc(a / b);
    }

    // kdos-splash draws with an 8x16 PSF font.
    readonly property int glyphW: 8
    readonly property int glyphH: 16
    readonly property int lineCols: 40
    readonly property int barSegs: 26

    readonly property int bodyScale: Math.max(1, Math.min(3, cdiv(fbw, 640)))
    readonly property int cell: glyphH * bodyScale
    readonly property int artCw: Math.max(4, Math.min(60, cdiv(fbw, 85)))
    readonly property int artCh: artCw * 2
    readonly property int artLt: Math.max(1, cdiv(artCw, 3))
    readonly property int artCols: Mark.art[0].length
    readonly property int artRows: Mark.art.length
    readonly property int artW: artCols * artCw
    readonly property int artH: artRows * artCh
    readonly property int pengScale: Math.max(1, cdiv(cdiv(artH * 115, 100) + cdiv(Mark.penguinHeight, 2), Mark.penguinHeight))
    readonly property int pengW: Mark.penguinWidth * pengScale
    readonly property int pengH: Mark.penguinHeight * pengScale
    readonly property int gap: artCw * 2
    readonly property int groupW: pengW + gap + artW
    readonly property int groupH: Math.max(pengH, artH)
    readonly property int pengX: cdiv(fbw - groupW, 2)
    readonly property int pengY: cdiv(fbh * 9, 100)
    readonly property int artX: pengX + pengW + gap
    readonly property int artY: pengY + cdiv(groupH - artH, 2)
    readonly property int subY: pengY + groupH + cell
    readonly property int tagY: subY + cell * 2
    readonly property int ruleY: tagY + cell * 2
    readonly property int bodyStep: cell + bodyScale * 4
    readonly property int bodyW: (lineCols + 6) * glyphW * bodyScale
    readonly property int bodyX: Math.max(0, cdiv(fbw - bodyW, 2))
    readonly property int barY: ruleY + bodyStep

    // Device pixels to the logical coordinates items are placed in.
    function px(v: real): real {
        return v / dpr;
    }

    // The text is set so one column is as wide as kdos-splash's 8-pixel
    // glyph at its scale, measured on "M": a monospace face's widest glyph
    // is not its cell, and sizing by it would widen every line.
    FontMetrics {
        id: probe
        font.family: Kirigami.Theme.fixedWidthFont.family
        font.pixelSize: 100
    }
    readonly property real textPx: {
        const m = probe.advanceWidth("M");
        const fit = m > 0 ? 100 * px(glyphW * bodyScale) / m : px(cell);
        return Math.min(fit, px(cell));
    }

    // ── the scheme's roles under kdos-splash's names ─────────────────────

    readonly property color ground: Kirigami.Theme.backgroundColor
    readonly property color ink: Kirigami.Theme.textColor
    readonly property color accent: Kirigami.Theme.highlightColor
    readonly property color accentDark: Kirigami.Theme.hoverColor
    readonly property color secondary: Kirigami.Theme.neutralTextColor
    readonly property color quiet: Kirigami.Theme.disabledTextColor
    readonly property color unlit: Kirigami.ColorUtils.linearInterpolation(ground, accentDark, 0.5)
    readonly property color artLower: Kirigami.ColorUtils.linearInterpolation(accent, accentDark, Mark.artLowerFraction)
    readonly property color mascotRim: Kirigami.ColorUtils.linearInterpolation(ink, Mark.glowTowards, Mark.glowFraction)

    // kdos-splash's mascot palette: 1 is the mascot's own body and 2 its rim,
    // the ink mixed towards the mascot's glow colour; both belong to the
    // mascot, which is drawn the same way on every accent. The rest are roles.
    function mascot(index: int): color {
        switch (index) {
        case 1:
            return Mark.mascotBody;
        case 2:
            return mascotRim;
        case 3:
            return secondary;
        case 4:
            return accent;
        default:
            return accentDark;
        }
    }

    // The wordmark's cells as kdos-splash's draw_art_cell() fills them:
    // a block, or a box-drawing border reduced to one thick stroke. Rows of
    // solid blocks are joined into one rectangle each.
    function artRects(): var {
        const out = [];
        const cw = artCw, ch = artCh, lt = artLt;
        for (let j = 0; j < artRows; j++) {
            const line = Mark.art[j];
            const lower = j >= cdiv(artRows, 2);
            let i = 0;
            while (i < artCols) {
                const c = line[i];
                const x = i * cw, y = j * ch;
                const mx = x + cdiv(cw, 2) - cdiv(lt, 2);
                const my = y + cdiv(ch, 2) - cdiv(lt, 2);
                if (c === "#") {
                    let n = 1;
                    while (i + n < artCols && line[i + n] === "#") {
                        n++;
                    }
                    out.push([x, y, cw * n, ch, lower]);
                    i += n;
                    continue;
                }
                if (c === "=") {
                    out.push([x, my, cw, lt, lower]);
                } else if (c === "|") {
                    out.push([mx, y, lt, ch, lower]);
                } else if (c === "{") {
                    out.push([mx, my, x + cw - mx, lt, lower], [mx, my, lt, y + ch - my, lower]);
                } else if (c === "}") {
                    out.push([x, my, mx + lt - x, lt, lower], [mx, my, lt, y + ch - my, lower]);
                } else if (c === "[") {
                    out.push([mx, my, x + cw - mx, lt, lower], [mx, y, lt, my + lt - y, lower]);
                } else if (c === "]") {
                    out.push([x, my, mx + lt - x, lt, lower], [mx, y, lt, my + lt - y, lower]);
                }
                i++;
            }
        }
        return out;
    }

    readonly property var wordmark: artRects()

    // ── the bar ─────────────────────────────────────────────────────────

    readonly property int filledSegs: Math.min(barSegs, cdiv(stage * barSegs, 6))
    readonly property int percent: Math.min(99, cdiv(stage * 100, 6))
    property bool pulse: false

    Timer {
        interval: 250
        repeat: true
        running: root.motion
        onTriggered: root.pulse = !root.pulse
    }

    // ── the picture ─────────────────────────────────────────────────────

    Item {
        id: picture

        anchors.fill: parent
        opacity: 0

        Rectangle {
            anchors.fill: parent
            color: root.ground
        }

        Item {
            x: root.px(root.artX)
            y: root.px(root.artY)

            Repeater {
                model: root.wordmark

                delegate: Rectangle {
                    required property var modelData

                    x: root.px(modelData[0])
                    y: root.px(modelData[1])
                    width: root.px(modelData[2])
                    height: root.px(modelData[3])
                    color: modelData[4] ? root.artLower : root.accent
                }
            }
        }

        Item {
            id: penguin

            x: root.px(root.pengX)
            y: root.px(root.pengY)
            width: root.px(root.pengW)
            height: root.px(root.pengH)

            Accessible.role: Accessible.Graphic
            Accessible.name: i18nd("kdos-plasma-look", "KDOS")

            Repeater {
                model: Mark.penguinRects.length / 5

                delegate: Rectangle {
                    required property int index

                    readonly property int base: index * 5

                    x: root.px(Mark.penguinRects[base] * root.pengScale)
                    y: root.px(Mark.penguinRects[base + 1] * root.pengScale)
                    width: root.px(Mark.penguinRects[base + 2] * root.pengScale)
                    height: root.px(Mark.penguinRects[base + 3] * root.pengScale)
                    color: root.mascot(Mark.penguinRects[base + 4])
                }
            }
        }

        Text {
            y: root.px(root.subY)
            anchors.horizontalCenter: parent.horizontalCenter
            text: Mark.subtitle
            color: root.ink
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.pixelSize: root.textPx
            textFormat: Text.PlainText
            Accessible.name: text
        }

        Text {
            y: root.px(root.tagY)
            anchors.horizontalCenter: parent.horizontalCenter
            text: Mark.tagline
            color: root.quiet
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.pixelSize: root.textPx
            textFormat: Text.PlainText
            Accessible.name: text
        }

        Rectangle {
            x: root.px(root.bodyX)
            y: root.px(root.ruleY)
            width: root.px(root.bodyW)
            height: root.px(root.bodyScale)
            color: root.unlit
        }

        Item {
            id: bar

            readonly property int glyph: root.glyphW * root.bodyScale
            readonly property int pctW: 5 * glyph
            readonly property int segAreaX: root.bodyX + glyph + root.bodyScale * 2
            readonly property int segAreaW: root.bodyW - glyph * 2 - root.bodyScale * 4 - pctW
            readonly property int segGap: root.bodyScale * 2
            readonly property int segW: root.cdiv(segAreaW - segGap * (root.barSegs - 1), root.barSegs)
            readonly property int segH: root.glyphH * root.bodyScale - root.bodyScale * 6

            anchors.fill: parent
            visible: segW >= 2

            Accessible.role: Accessible.ProgressBar
            Accessible.name: i18nd("kdos-plasma-look", "Starting the desktop")

            Text {
                x: root.px(root.bodyX - root.cdiv(bar.glyph, 2))
                y: root.px(root.barY)
                text: "["
                color: root.ink
                font.family: Kirigami.Theme.fixedWidthFont.family
                font.pixelSize: root.textPx
            }

            Text {
                x: root.px(bar.segAreaX + root.barSegs * (bar.segW + bar.segGap) - bar.segGap + root.bodyScale * 2)
                y: root.px(root.barY)
                text: "]"
                color: root.ink
                font.family: Kirigami.Theme.fixedWidthFont.family
                font.pixelSize: root.textPx
            }

            Repeater {
                model: root.barSegs

                delegate: Rectangle {
                    required property int index

                    x: root.px(bar.segAreaX + index * (bar.segW + bar.segGap))
                    y: root.px(root.barY + root.bodyScale * 3)
                    width: root.px(bar.segW)
                    height: root.px(bar.segH)
                    color: {
                        if (index < root.filledSegs) {
                            return root.accent;
                        }
                        if (index === root.filledSegs && root.pulse) {
                            return root.secondary;
                        }
                        return root.unlit;
                    }
                }
            }

            Text {
                x: root.px(root.bodyX + root.bodyW - bar.pctW + bar.glyph)
                y: root.px(root.barY)
                text: i18ndc("kdos-plasma-look", "progress readout, a number and a percent sign, four columns wide", "%1%", String(root.percent).padStart(3, " "))
                color: root.accent
                font.family: Kirigami.Theme.fixedWidthFont.family
                font.pixelSize: root.textPx
                textFormat: Text.PlainText
            }
        }

        // KDE's credit. The KDOS mark stands where KDE's logo stands in
        // the Breeze splash; the credit stays.
        Text {
            anchors {
                right: parent.right
                bottom: parent.bottom
                margins: Kirigami.Units.gridUnit
            }
            text: i18nd("kdos-plasma-look", "Plasma by KDE")
            color: root.quiet
            font: Kirigami.Theme.smallFont
            textFormat: Text.PlainText
            Accessible.name: text
        }
    }

    OpacityAnimator {
        id: fadeIn

        target: picture
        from: 0
        to: 1
        duration: Kirigami.Units.veryLongDuration * 2
        easing.type: Easing.InOutQuad
    }

    Component.onCompleted: {
        if (motion) {
            fadeIn.start();
        } else {
            picture.opacity = 1;
        }
    }
}
