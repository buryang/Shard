#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_separate_shader_objects : enable

layout(location=0) in vec2 frag_coord;
layout(binding=1) uniform sampler2D tex_map;
layout(location=0) out vec4 out_color;

void main()
{
	out_color = texture(tex_map, frag_coord);
}