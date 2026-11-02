/*
    SPDX-FileCopyrightText: 2026 Kunal Dawn <kunal.dawn@gmail.com>
    SPDX-License-Identifier: MIT
*/

"use strict";

// When KSplash's window closes, the desktop opens out of a hairline across its
// middle the way the boot splash powers on, in place of the stock login
// effect's fade. The stock effect is turned off while this one is on (kwinrc
// [Plugins] loginEnabled=false); with both on, both animate the splash.
class LoginEffect {
    constructor() {
        this.shader = LoginEffect.shader("poweron.frag");

        effect.configChanged.connect(this.loadConfig.bind(this));
        effects.windowClosed.connect(this.slotWindowClosed.bind(this));

        this.loadConfig();
    }

    // A shader that does not build leaves the splash to close at once.
    static shader(file) {
        try {
            return effect.addFragmentShader(Effect.MapTexture, file);
        } catch (error) {
            console.warn(`kdos_login: ${file}: ${error}`);
            return 0;
        }
    }

    // animationTime() follows AnimationDurationFactor; at 0 it returns 1 ms and
    // the splash goes in one frame.
    loadConfig() {
        this.duration = animationTime(900);
    }

    static isLoginSplash(window) {
        return window.windowClass === "ksplashqml ksplashqml";
    }

    slotWindowClosed(window) {
        if (!LoginEffect.isLoginSplash(window) || !this.shader || this.duration <= 1) {
            return;
        }
        if (!effect.grab(window, Effect.WindowClosedGrabRole)) {
            return;
        }
        animate({
            window: window,
            duration: this.duration,
            curve: QEasingCurve.Linear,
            fragmentShader: this.shader,
            animations: [{
                type: Effect.ShaderUniform,
                uniform: "u_t",
                from: 0.0,
                to: 1.0
            }]
        });
    }
}

new LoginEffect();
