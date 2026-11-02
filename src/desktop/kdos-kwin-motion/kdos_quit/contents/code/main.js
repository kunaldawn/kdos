/*
    SPDX-FileCopyrightText: 2026 Kunal Dawn <kunal.dawn@gmail.com>
    SPDX-License-Identifier: MIT
*/

"use strict";

// The session powering down. While it quits, each window it closes folds away
// the way the boot splash powers off, and the desktop is held on screen until
// KWin itself exits, so the last window never leaves a flash of black before
// the greeter. This replaces the stock sessionquit effect, which only holds
// the desktop (kwinrc [Plugins] sessionquitEnabled=false while this is on).
// The collapse of the whole screen is kdos_crt's; this is what an output that
// effect does not draw on shows.
class QuitEffect {
    constructor() {
        this.foldShader = QuitEffect.shader("fold.frag");

        effect.configChanged.connect(this.loadConfig.bind(this));
        effects.windowClosed.connect(this.slotWindowClosed.bind(this));

        this.loadConfig();
    }

    // A shader that does not build leaves the folds out; the desktop is
    // still held.
    static shader(file) {
        try {
            return effect.addFragmentShader(Effect.MapTexture, file);
        } catch (error) {
            console.warn(`kdos_quit: ${file}: ${error}`);
            return 0;
        }
    }

    // The fold follows AnimationDurationFactor; the hold is not motion and
    // does not.
    loadConfig() {
        this.duration = animationTime(450);
    }

    static isFoldWindow(window) {
        if (window.popupWindow || window.lockScreen || window.outline || !window.managed) {
            return false;
        }
        return window.hasDecoration || window.normalWindow || window.dialog;
    }

    slotWindowClosed(window) {
        if (effects.sessionState !== Globals.Quitting) {
            return;
        }
        if (window.desktopWindow) {
            this.hold(window);
            return;
        }
        if (!this.foldShader || this.duration <= 1) {
            return;
        }
        if (!window.visible || window.skipsCloseAnimation || !QuitEffect.isFoldWindow(window)) {
            return;
        }
        if (!effect.grab(window, Effect.WindowClosedGrabRole)) {
            return;
        }
        animate({
            window: window,
            duration: this.duration,
            curve: QEasingCurve.Linear,
            fragmentShader: this.foldShader,
            animations: [{
                type: Effect.ShaderUniform,
                uniform: "u_t",
                from: 0.0,
                to: 1.0
            }]
        });
    }

    // A closed desktop window kept as a still picture for longer than any
    // logout takes; KWin's exit ends it.
    hold(window) {
        if (!effect.grab(window, Effect.WindowClosedGrabRole)) {
            return;
        }
        animate({
            window: window,
            duration: 30 * 1000,
            type: Effect.Generic,
            from: 0.0,
            to: 0.0
        });
    }
}

new QuitEffect();
