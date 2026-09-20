#pragma once

namespace renderer_shaders {

inline constexpr const char* kVertexShader = R"(#version 300 es
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;
uniform vec2 uResolution;
out vec4 vColor;
void main() {
    vec2 p = vec2(
        (aPosition.x / uResolution.x) * 2.0 - 1.0,
        1.0 - (aPosition.y / uResolution.y) * 2.0
    );
    gl_Position = vec4(p, 0.0, 1.0);
    vColor = aColor;
}
)";

inline constexpr const char* kFragmentShader = R"(#version 300 es
precision mediump float;
in vec4 vColor;
out vec4 outColor;
void main() {
    outColor = vColor;
}
)";

inline constexpr const char* kTextureVertexShader = R"(#version 300 es
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec2 aLocalCoord;
uniform vec2 uResolution;
out vec2 vTexCoord;
out vec2 vLocalCoord;
void main() {
    vec2 p = vec2(
        (aPosition.x / uResolution.x) * 2.0 - 1.0,
        1.0 - (aPosition.y / uResolution.y) * 2.0
    );
    gl_Position = vec4(p, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vLocalCoord = aLocalCoord;
}
)";

inline constexpr const char* kTextureFragmentShader = R"(#version 300 es
precision mediump float;
in vec2 vTexCoord;
in vec2 vLocalCoord;
uniform sampler2D uTexture;
uniform float uAlpha;
uniform vec4 uTint;
uniform vec2 uRectSize;
uniform float uRadius;
out vec4 outColor;
void main() {
    vec4 sampled = texture(uTexture, vTexCoord);
    float mask = 1.0;
    if (uRadius > 0.0) {
        vec2 halfSize = uRectSize * 0.5;
        vec2 q = abs(vLocalCoord - halfSize) - (halfSize - vec2(uRadius));
        float distanceToEdge = length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - uRadius;
        mask = 1.0 - smoothstep(-1.0, 1.0, distanceToEdge);
    }
    outColor = vec4(sampled.rgb * uTint.rgb, sampled.a * uTint.a * uAlpha * mask);
}
)";

inline constexpr const char* kExternalVertexShader = R"(#version 300 es
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
uniform vec2 uResolution;
uniform mat4 uTransform;
out vec2 vTexCoord;
void main() {
    vec2 p = vec2(
        (aPosition.x / uResolution.x) * 2.0 - 1.0,
        1.0 - (aPosition.y / uResolution.y) * 2.0
    );
    gl_Position = vec4(p, 0.0, 1.0);
    vec2 sourceCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
    vec4 transformed = uTransform * vec4(sourceCoord, 0.0, 1.0);
    vTexCoord = transformed.xy;
}
)";

inline constexpr const char* kExternalFragmentShader = R"(#version 300 es
#extension GL_OES_EGL_image_external_essl3 : require
precision mediump float;
in vec2 vTexCoord;
uniform samplerExternalOES uTexture;
uniform float uAlpha;
out vec4 outColor;
void main() {
    vec4 sampled = texture(uTexture, vTexCoord);
    outColor = vec4(sampled.rgb, sampled.a * uAlpha);
}
)";

} // namespace renderer_shaders
