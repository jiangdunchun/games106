#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inColor;
layout (location = 4) in vec4 inTangent;

layout (set = 0, binding = 0) uniform UBOScene 
{
	mat4 projection;
	mat4 view;
	mat4 model;
	vec4 lightPos;
	vec4 viewPos;
	int colorShadingRates;
} uboScene;

void main() 
{
	 vec4 frustum_pos = uboScene.projection * uboScene.view * uboScene.model * vec4(inPos.xyz, 1.0);
	 gl_Position = frustum_pos * vec4(1,1,1.01,1);
}