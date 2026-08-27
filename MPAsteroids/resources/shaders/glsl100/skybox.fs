#version 100

precision mediump float;

// Input vertex attributes (from vertex shader)
varying vec3 fragPosition;

// Input uniform values
uniform samplerCube environmentMap;
uniform bool vflipped;
uniform bool doGamma;

void main()
{
    // Fetch color from texture map
    vec3 color = vec3(0.0);

    if (vflipped) color = textureCube(environmentMap, vec3(fragPosition.x, -fragPosition.y, fragPosition.z)).rgb;
    else color = textureCube(environmentMap, fragPosition).rgb;

    if (doGamma)// Apply gamma correction
    {
        color = color/(color + vec3(1.0));
        color = pow(color, vec3(1.0/2.2));
    }

    // Calculate final fragment color
    gl_FragColor = vec4(color, 1.0);
}
