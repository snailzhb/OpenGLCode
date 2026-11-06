#version 330 core
out vec4 FragColor;
uniform vec3 color;
uniform float uAlpha = 1.0;
void main() {
    FragColor = vec4(color, uAlpha);
}
