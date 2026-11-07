#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(set=2,binding=0) uniform sampler2D image;
layout(location=0) out vec4 outColor;
// Sharp bilinear: each texel flat, blended only across the screen pixel at
// its edge, so magnified pixel art keeps every pixel the same size at any
// scale (DOOM's 320x200 HUD on any screen). With a nearest sampler (the
// settings panel) it reads the nearest texel as before.
void main() {
    vec2 size=vec2(textureSize(image,0)),texel=vUV*size;
    vec2 width=max(fwidth(texel),vec2(1e-4));
    vec2 p=texel-0.5,q=floor(p),f=p-q;
    vec2 s=clamp((f-0.5)/width+0.5,0.0,1.0);
    outColor=textureLod(image,(q+s+0.5)/size,0.0);
}
