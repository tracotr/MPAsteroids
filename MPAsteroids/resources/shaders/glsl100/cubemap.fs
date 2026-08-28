#version 100

precision mediump float;

varying vec3 fragPosition;

uniform sampler2D equirectangularMap;

// Maps a direction vector onto equirectangular texture coordinates.
vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= vec2(0.1591, 0.3183);
    uv += 0.5;
    return uv;
}

void main()
{
    vec2 uv = SampleSphericalMap(normalize(fragPosition));
    gl_FragColor = vec4(texture2D(equirectangularMap, uv).rgb, 1.0);
}
