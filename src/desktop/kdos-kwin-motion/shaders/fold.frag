#version 140
// SPDX-FileCopyrightText: 2026 Kunal Dawn <kunal.dawn@gmail.com>
// SPDX-License-Identifier: MIT

// A window closing the way the KDOS boot splash powers off: the picture folds
// to a hairline over the first 55 per cent, the hairline pinches to a dot from
// 45 to 85 per cent and the dot fades over the last 20, while the beam's light
// gathers in what is left. u_t runs from 0 to 1 across the close.

#include "colormanagement.glsl"
#include "saturation.glsl"

uniform sampler2D sampler;
uniform vec4 modulation;
uniform float u_t;

in vec2 texcoord0;

out vec4 fragColor;

void main()
{
    float fold = max(1.0 - u_t / 0.55, 0.0);
    fold = max(fold * fold, 0.0001);
    float pinch = clamp(1.0 - (u_t - 0.45) / 0.40, 0.0001, 1.0);
    float decay = clamp(1.0 - (u_t - 0.80) / 0.20, 0.0, 1.0);
    float flash = u_t * 0.5;

    vec2 p = vec2((texcoord0.x - 0.5) / pinch + 0.5, (texcoord0.y - 0.5) / fold + 0.5);
    bool column = p.x >= 0.0 && p.x <= 1.0;
    vec4 color = vec4(0.0);
    if (column && p.y >= 0.0 && p.y <= 1.0) {
        color = texture(sampler, p);
    }
    color = sourceEncodingToNitsInDestinationColorspace(color);
    color = adjustSaturation(color);

    // the beam's light, at the brightness of the output's white
    vec4 hot = vec4(vec3(destinationReferenceLuminance), 1.0);
    color.rgb = mix(color.rgb, hot.rgb * color.a, flash * 0.45);

    // the seam spans what is left of the window's width, taken from its
    // middle row so it never crosses the transparent shadow
    if (fold < 0.999 && column) {
        float d = abs(abs(texcoord0.y - 0.5) * 2.0 - fold);
        float seam = max(1.0 - d / 0.012, 0.0);
        float body = texture(sampler, vec2(p.x, 0.5)).a;
        color = mix(color, hot * body, seam * 0.85);
    }

    fragColor = nitsToDestinationEncoding(color * decay * modulation);
}
