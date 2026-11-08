#version 140
// SPDX-FileCopyrightText: 2026 Kunal Dawn <kunal.dawn@gmail.com>
// SPDX-License-Identifier: MIT

// The splash handing over to the desktop the way a CRT powers on: the desktop
// opens out of a hairline across the middle of the closing splash, easing out
// over the first 55 per cent, with a hot seam on the edge of the opening and a
// wash of the beam's light over the desktop that burns off by 60 per cent.
// u_t runs from 0 to 1 across the handover.

#include "colormanagement.glsl"
#include "saturation.glsl"

uniform sampler2D sampler;
uniform vec4 modulation;
uniform float u_t;

in vec2 texcoord0;

out vec4 fragColor;

void main()
{
    float u = clamp(u_t / 0.55, 0.0, 1.0);
    float vopen = 1.0 - pow(1.0 - u, 3.0);
    float flash = max(1.0 - u_t / 0.6, 0.0);
    flash *= flash;

    float edge = abs(texcoord0.y - 0.5) * 2.0;
    vec4 hot = vec4(vec3(destinationReferenceLuminance), 1.0);
    vec4 color;
    if (edge < vopen) {
        // inside the opening the splash is gone: the desktop shows through
        // under the beam's light
        color = hot * (flash * 0.45);
    } else {
        color = sourceEncodingToNitsInDestinationColorspace(texture(sampler, texcoord0));
        color = adjustSaturation(color);
    }

    if (vopen < 0.999) {
        float d = abs(edge - vopen);
        float seam = max(1.0 - d / 0.012, 0.0);
        color = mix(color, hot, seam * 0.85);
    }

    fragColor = nitsToDestinationEncoding(color * modulation);
}
