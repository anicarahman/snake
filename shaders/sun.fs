#version 330
uniform sampler2D plasma;
uniform sampler1D lave;
uniform float cycle;
in vec2 vsoTexCoord;

out vec4 fragColor;

void main(void) {
  vec2 t = vec2(2.0, 1.0) * vsoTexCoord;
  if(t.x > 1.0)
    t.x = 2 - t.x;
  fragColor = texture(lave, texture(plasma, t).r + cycle);
}
