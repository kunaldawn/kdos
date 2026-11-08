#version 140
// SPDX-FileCopyrightText: 2026 Kunal Dawn <kunal.dawn@gmail.com>
// SPDX-License-Identifier: MIT

// A window opening the way the KDOS boot splash powers on: the picture unfolds
// from a hairline across its middle, easing out over the first 55 per cent,
// while a wash of the beam's light burns off it by 60 per cent and a hot seam
// rides the edge of the unfold. u_t runs from 0 to 1 across the open.

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
    float vopen = max(1.0 - pow(1.0 - u, 3.0), 0.0001);
    float flash = max(1.0 - u_t / 0.6, 0.0);
    flash *= flash;

    float y = (texcoord0.y - 0.5) / vopen + 0.5;
    vec4 color = vec4(0.0);
    if (y >= 0.0 && y <= 1.0) {
        color = texture(sampler, vec2(texcoord0.x, y));
    }
    color = sourceEncodingToNitsInDestinationColorspace(color);
    color = adjustSaturation(color);

    // the beam's light, at the brightness of the output's white
    vec4 hot = vec4(vec3(destinationReferenceLuminance), 1.0);
    color.rgb = mix(color.rgb, hot.rgb * color.a, flash * 0.45);

    // the seam spans the window's own width: the middle row says where the
    // window is opaque, so the seam never crosses its transparent shadow
    if (vopen < 0.999) {
        float d = abs(abs(texcoord0.y - 0.5) * 2.0 - vopen);
        float seam = max(1.0 - d / 0.012, 0.0);
        float body = texture(sampler, vec2(texcoord0.x, 0.5)).a;
        color = mix(color, hot * body, seam * 0.85);
    }

    fragColor = nitsToDestinationEncoding(color * modulation);
}
