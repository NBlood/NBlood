#version 450

layout(location = 0) out vec2 texCoord;

void main()
{
	gl_Position = vec4((gl_VertexIndex & 2) * 2 - 1, ((gl_VertexIndex & 1) * 4) - 1, 0, 1);
	texCoord = vec2(gl_VertexIndex & 2, (gl_VertexIndex & 1) * 2);
}
