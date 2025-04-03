#version 150

in vec2 tc; // Texture coordinates passed through from vertex shader
out vec4 c; // Output a color

uniform sampler2D textureImage; // The texture to use

void main()
{
  // Look up the color from the texture image
  c = texture(textureImage, tc);
}

