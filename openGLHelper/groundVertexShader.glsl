#version 150

in vec3 position;
// Given a texture coordinate, we pass it through to the fragment shader
in vec2 texCoord;
out vec2 tc;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;

void main()
{
  // compute the transformed and projected vertex position (into gl_Position) 
  // compute the vertex color (into col)
  gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0f);
  // Pass-through texture coordinate
  tc = texCoord;
}

