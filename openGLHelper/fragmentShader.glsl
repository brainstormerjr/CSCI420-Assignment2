#version 150

in vec3 viewPosition;
in vec3 viewNormal;

out vec4 c;

// Light properties
uniform vec4 La;
uniform vec4 Ld;
uniform vec4 Ls;
in vec3 viewLightDirection;

// Material properties
uniform vec4 ka;
uniform vec4 kd;
uniform vec4 ks;
uniform float alpha;

void main()
{
  // Camera is at (0,0,0) after the modelview transformation
  vec3 eyedir = normalize(vec3(0, 0, 0) - viewPosition);
  // reflected light direction
  vec3 reflectDir = -reflect(viewLightDirection, viewNormal);
  // Phong lighting
  float d = max(dot(viewLightDirection, viewNormal), 0.0f);
  float s = max(dot(reflectDir, eyedir), 0.0f);
  // compute the final color
  c = ka * La + d * kd * Ld + pow(s, alpha) * ks * Ls;
}

