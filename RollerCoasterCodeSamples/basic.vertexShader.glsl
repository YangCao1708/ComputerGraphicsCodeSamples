#version 150

in vec3 position;
in vec3 normal;

out vec4 col;
out vec3 viewPosition;
out vec3 viewNormal;

uniform int mode;

uniform mat4 modelViewMatrix;
uniform mat4 normalMatrix;
uniform mat4 projectionMatrix;

void main()
{
  // compute the transformed and projected vertex position (into gl_Position) 
  // compute the vertex color (into col)

  if (mode == 1)
  {
    vec4 viewPosition4 = modelViewMatrix * vec4(position, 1.0f);
    viewPosition = viewPosition4.xyz;
    gl_Position = projectionMatrix * viewPosition4;
    viewNormal = (normalMatrix*vec4(normal, 0.0f)).xyz;

    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0f);
  }
  if (mode == 0)
  {
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0f);
    col = vec4(1, 1, 1, 1);
  }
}

