/*
    SPDX-FileCopyrightText: 2026 Kunal Dawn <kunal.dawn@gmail.com>
    SPDX-License-Identifier: MIT
*/

"use strict";

// Windows that another effect animates, or that must appear at once.
const blacklist = {
    "ksmserver ksmserver": [],
    "ksmserver-logout-greeter ksmserver-logout-greeter": [],
    "ksplashqml ksplashqml": [],
    "spectacle org.kde.spectacle": ["region-editor"],
};

// Windows unfold from a line as they open and fold to a dot as they close.
// The effect takes the open and close grabs, so a stock open/close effect left
// enabled beside it (scale, fade, glide) steps aside instead of animating the
// same window twice.
class UnfoldEffect {
    constructor() {
        this.unfoldShader = UnfoldEffect.shader("unfold.frag");
        this.foldShader = UnfoldEffect.shader("fold.frag");

        effect.configChanged.connect(this.loadConfig.bind(this));
        effects.windowAdded.connect(this.slotWindowAdded.bind(this));
        effects.windowClosed.connect(this.slotWindowClosed.bind(this));
        effects.windowDataChanged.connect(this.slotWindowDataChanged.bind(this));

        this.loadConfig();
    }

    // A shader that does not build leaves the effect loaded and still: windows
    // then open and close as they would with no effect at all.
    static shader(file) {
        try {
            return effect.addFragmentShader(Effect.MapTexture, file);
        } catch (error) {
            console.warn(`kdos_unfold: ${file}: ${error}`);
            return 0;
        }
    }

    // animationTime() follows AnimationDurationFactor, and at 0 returns 1 ms:
    // reduced motion is no animation at all, so nothing is grabbed either.
    loadConfig() {
        this.openDuration = animationTime(160);
        this.closeDuration = animationTime(140);
    }

    static isUnfoldWindow(window) {
        // plasmashell draws the panel, the desktop and its popups in one
        // window class; of those, only its decorated dialogs are windows.
        if (window.windowClass === "plasmashell plasmashell"
                || window.windowClass === "plasmashell org.kde.plasmashell") {
            return window.hasDecoration;
        }
        const tags = blacklist[window.windowClass];
        if (tags && (tags.length === 0 || tags.includes(window.tag))) {
            return false;
        }
        if (window.hasDecoration) {
            return true;
        }
        if (window.popupWindow || window.lockScreen || window.outline || !window.managed) {
            return false;
        }
        return window.normalWindow || window.dialog;
    }

    play(window, shader, duration) {
        return animate({
            window: window,
            duration: duration,
            curve: QEasingCurve.Linear,
            fragmentShader: shader,
            animations: [{
                type: Effect.ShaderUniform,
                uniform: "u_t",
                from: 0.0,
                to: 1.0
            }]
        });
    }

    slotWindowAdded(window) {
        if (!this.unfoldShader || this.openDuration <= 1) {
            return;
        }
        if (effects.hasActiveFullScreenEffect || !window.visible || !UnfoldEffect.isUnfoldWindow(window)) {
            return;
        }
        if (!effect.grab(window, Effect.WindowAddedGrabRole)) {
            return;
        }
        window.kdosUnfold = this.play(window, this.unfoldShader, this.openDuration);
    }

    slotWindowClosed(window) {
        // The windows a logout closes are kdos_quit's.
        if (effects.sessionState === Globals.Quitting) {
            return;
        }
        if (!this.foldShader || this.closeDuration <= 1) {
            return;
        }
        if (effects.hasActiveFullScreenEffect || !window.visible || window.skipsCloseAnimation
                || !UnfoldEffect.isUnfoldWindow(window)) {
            return;
        }
        if (!effect.grab(window, Effect.WindowClosedGrabRole)) {
            return;
        }
        // A window closed while it still unfolds folds from the whole picture:
        // the two shaders share no state to hand over.
        if (window.kdosUnfold) {
            cancel(window.kdosUnfold);
            delete window.kdosUnfold;
        }
        window.kdosFold = this.play(window, this.foldShader, this.closeDuration);
    }

    // Another effect that takes a window's grab after this one did owns its
    // animation from then on.
    slotWindowDataChanged(window, role) {
        if (role === Effect.WindowAddedGrabRole) {
            if (window.kdosUnfold && effect.isGrabbed(window, role)) {
                cancel(window.kdosUnfold);
                delete window.kdosUnfold;
            }
        } else if (role === Effect.WindowClosedGrabRole) {
            if (window.kdosFold && effect.isGrabbed(window, role)) {
                cancel(window.kdosFold);
                delete window.kdosFold;
            }
        }
    }
}

new UnfoldEffect();
