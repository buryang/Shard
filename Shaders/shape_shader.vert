#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(set = 0, location = 0) in vec2 in_position;
layout(set = 0, location = 1) in vec3 in_color;
layout(set = 0. location = 2) in vec2 in_tex_coord;
layout(push_constant) uniform CameraParameters{
	mat4 model;
	mat4 proj;
	mat4 view;
}ubo;

layout(location = 0) out vec3 frag_color;
layout(location = 1) out vec2 frag_tex_coord;

void main()
{
	gl_Position = ubo.proj*ubo.view*ubo.model*vec4(in_position, 0.0, 1.0);
	frag_color = in_color;
	frag_tex_coord = in_tex_coord;
}