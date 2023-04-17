#version 450

layout (set = 1, binding = 0) uniform sampler2D samplerColorMap;
layout (set = 1, binding = 1) uniform sampler2D samplerNormalMap;
layout (set = 1, binding = 2) uniform sampler2D samplerMetallicRoughnessMap;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inViewVec;
layout (location = 4) in vec3 inLightVec;
layout (location = 5) in mat3 inTBN;

layout (location = 0) out vec4 outFragColor;

const float PI = 3.14159265359;
  
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  

void main() 
{
    vec3 light = vec3(10);
    float ao = 1.0;



	vec4 color = texture(samplerColorMap, inUV) * vec4(inColor, 1.0);
	vec4 normal = texture(samplerNormalMap, inUV);
	vec4 metallicRoughness = texture(samplerMetallicRoughnessMap, inUV);
    
    
    vec3 albedo = pow(color.rgb, vec3(2.2));
	float metallic = metallicRoughness.r;
	float roughness = metallicRoughness.g;

	vec3 N = normalize(inverse(inTBN) * (normal.rgb * 2.0f - 1.0f));
	vec3 V = normalize(inViewVec);

	vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

	vec3 Lo = vec3(0.0);
    //for(int i = 0; i < 4; ++i) 
    //{
        // calculate per-light radiance
        vec3 L = normalize(inLightVec);
        vec3 H = normalize(V + L);
        float distance    = length(inLightVec);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance     = light * attenuation;        
        
        // cook-torrance brdf
        float NDF = DistributionGGX(N, H, roughness);        
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	  
        
        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular     = numerator / denominator;  
            
        // add to outgoing radiance Lo
        float NdotL = max(dot(N, L), 0.0);                
        Lo += (kD * albedo / PI + specular) * radiance * NdotL; 
    //}   
  
    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 o_color = ambient + Lo;
	
    o_color = o_color / (o_color + vec3(1.0));
    o_color = pow(o_color, vec3(1.0/2.2));

	outFragColor = vec4(o_color, 1.0);
}