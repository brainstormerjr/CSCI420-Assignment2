#version 150

in vec3 position;
in vec3 p_up;
in vec3 p_down;
in vec3 p_left;
in vec3 p_right;
in vec4 color;
out vec4 col;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform int mode;
uniform float scale;
uniform float exponent;

void main()
{
  if (mode == 0)
  {
    // compute the transformed and projected vertex position (into gl_Position) 
    // compute the vertex color (into col)
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0f);
    col = color;
  }
  else
  {
    // Average height (z)
    vec3 pos = vec3( position.x, position.y, ((position + p_up + p_down + p_left + p_right) / 5.0f).z);
    // Compute color from height
    col = vec4(vec3(pow(pos.z, exponent)), color.a);
    // Scale and exponentiate height
    pos.z = scale * pow(pos.z, exponent);
    // Compute transformed and projected vertex position
    gl_Position = projectionMatrix * modelViewMatrix * vec4(pos, 1.0f);
  }
}

