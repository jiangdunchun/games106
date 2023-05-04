#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inTangent;
layout (location = 3) in vec2 inUV;
layout (location = 4) in vec3 inColor;

layout (set = 0, binding = 0) uniform UBOScene
{
	mat4 projection;
	mat4 view;
	vec4 lightPos;
	vec4 viewPos;
} uboScene;

layout(push_constant) uniform PushConsts {
	mat4 model;
} primitive;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec3 outViewVec;
layout (location = 4) out vec3 outLightVec;
layout (location = 5) out mat3 outTBN;


void main() 
{
	outColor = inColor;
	outUV = inUV;
	gl_Position = uboScene.projection * uboScene.view * primitive.model * vec4(inPos.xyz, 1.0);
	
	mat4 mv = uboScene.view * primitive.model;

	vec3 pos = (mv * vec4(inPos.xyz, 1.0)).xyz;
	outNormal = mat3(mv) * inNormal;
	outLightVec = (uboScene.view * uboScene.lightPos).xyz - pos;
	outViewVec = -pos;	

	mat4 miv = uboScene.view * transpose(inverse(primitive.model));
	vec3 N = normalize(mat3(miv) * inNormal);
    vec3 T = normalize(mat3(miv) * inTangent);
	T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    outTBN = transpose(mat3(T, B, N));
}