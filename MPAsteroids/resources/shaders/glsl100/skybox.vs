#version 100

attribute vec3 vertexPosition;

uniform mat4 matProjection;
uniform mat4 matView;

varying vec3 fragPosition;

void main()
{
    fragPosition = vertexPosition;

    // Strip translation so the skybox stays centred on the camera.
    mat4 rotView = mat4(mat3(matView));

    gl_Position = matProjection*rotView*vec4(vertexPosition, 1.0);
}
