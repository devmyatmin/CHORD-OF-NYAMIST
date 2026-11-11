#version 450

layout(location = 0) out vec3 vertexColor;

layout(push_constant) uniform SurfaceTransform {
    vec4 matrix;
} surfaceTransform;

const vec2 positions[3] = vec2[](
    vec2(0.0, -0.68),
    vec2(0.68, 0.62),
    vec2(-0.68, 0.62)
);

const vec3 colors[3] = vec3[](
    vec3(1.0, 0.22, 0.18),
    vec3(0.20, 0.86, 0.42),
    vec3(0.18, 0.48, 1.0)
);

void main() {
    vec2 position = positions[gl_VertexIndex];
    vec2 rotatedPosition = vec2(
        surfaceTransform.matrix.x * position.x + surfaceTransform.matrix.y * position.y,
        surfaceTransform.matrix.z * position.x + surfaceTransform.matrix.w * position.y
    );
    gl_Position = vec4(rotatedPosition, 0.0, 1.0);
    vertexColor = colors[gl_VertexIndex];
}
