#version 100

precision mediump float;

varying vec3 fragPosition;

uniform samplerCube environmentMap;
uniform bool vflipped;
uniform bool doGamma;

void main()
{
    vec3 color = vec3(0.0);

    if (vflipped) color = textureCube(environmentMap, vec3(fragPosition.x, -fragPosition.y, fragPosition.z)).rgb;
    else color = textureCube(environmentMap, fragPosition).rgb;

    if (doGamma)
    {
        color = color/(color + vec3(1.0));
        color = pow(color, vec3(1.0/2.2));
    }

    gl_FragColor = vec4(color, 1.0);
}
