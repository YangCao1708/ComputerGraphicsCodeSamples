#version 150

in vec3 viewPosition;
in vec3 viewNormal;

in vec4 col;
out vec4 c;
uniform int mode;

uniform vec4 La;
uniform vec4 Ld;
uniform vec4 Ls;
uniform vec3 viewLightDirection; // in view space?

uniform vec4 ka;
uniform vec4 kd;
uniform vec4 ks;
uniform float alpha;

void main()
{
  vec3 viewNormalN = normalize(viewNormal);
  // camera is at (0,0,0) after the modelview transformation
  vec3 eyedir = normalize(vec3(0, 0, 0) - viewPosition);
  // reflected light direction
  vec3 reflectDir = -reflect(viewLightDirection, viewNormalN);
  // Phong lighting
  float d = max(dot(viewLightDirection, viewNormalN), 0.0f);
  float s = max(dot(reflectDir, eyedir), 0.0f);
  c = ka * La + d * kd * Ld + pow(s, alpha) * ks * Ls;

  if (mode == 0)
  {
    c = col;
  }
}

