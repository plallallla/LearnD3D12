cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 worldViewProj;
    float4x4 model;
    float4x4 normalMatrix;
    float4 eye;
    float4 lightPos;
    float4 lightColor;
};

Texture2D g_texture_albedo : register(t0);
Texture2D g_texture_ao : register(t1);
Texture2D g_texture_metallic : register(t2);
Texture2D g_texture_normal : register(t3);
Texture2D g_texture_roughness : register(t4);
SamplerState g_sampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLD_POS;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

float3 GetNormalFromMap(Texture2D normalMap, SamplerState normalSampler, float2 TexCoords, float3 WorldPos, float3 Normal)
{
    float3 tangentNormal = normalMap.Sample(normalSampler, TexCoords).xyz * 2.0 - 1.0;

    float3 Q1 = ddx(WorldPos);
    float3 Q2 = ddy(WorldPos);
    float2 st1 = ddx(TexCoords);
    float2 st2 = ddy(TexCoords);

    float3 N = normalize(Normal);
    float3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    float3 B = -normalize(cross(N, T));

    float3x3 TBN = float3x3(T, B, N);

    return normalize(mul(tangentNormal, TBN));
}

float D_GGX(float NdotH, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    return alpha2 / (3.1415926535 * denom * denom);
}

float G_Smith(float NdotV, float NdotL, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float GL = NdotL / (NdotL * (1.0 - k) + k);
    float GV = NdotV / (NdotV * (1.0 - k) + k);
    return GL * GV;
}

float3 F_Schlick(float3 F0, float HdotV)
{
    return F0 + (1.0 - F0) * pow(1.0 - HdotV, 5.0);
}

PSInput VSMain(float3 position : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD)
{
    PSInput result;
    float4 worldPos = mul(model, float4(position, 1.0));
    result.worldPos = worldPos.xyz;
    result.position = mul(worldViewProj, float4(position, 1.0));

    float3 N = mul(normalMatrix, float4(normal, 0.0)).xyz;
    result.normal = normalize(N);

    result.uv = uv;
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 albedo = pow(g_texture_albedo.Sample(g_sampler, input.uv).rgb, 2.2);
    float ao = g_texture_ao.Sample(g_sampler, input.uv).r;
    float metallic = g_texture_metallic.Sample(g_sampler, input.uv).r;
    float roughness = g_texture_roughness.Sample(g_sampler, input.uv).r;
    roughness = max(roughness, 0.04);

    float3 F0 = 0.04;
    F0 = lerp(F0, albedo, metallic);

    float3 L = normalize(lightPos.xyz - input.worldPos);
    float3 V = normalize(eye.xyz - input.worldPos);
    float3 N = GetNormalFromMap(g_texture_normal, g_sampler, input.uv, input.worldPos, input.normal);

    float3 H = normalize(L + V);
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));
    float HdotV = saturate(dot(H, V));

    if (NdotL <= 0.0)
        return float4(albedo * ao * 0.15, 1.0);

    float D = D_GGX(NdotH, roughness);
    float G = G_Smith(NdotV, NdotL, roughness);
    float3 F = F_Schlick(F0, HdotV);

    float3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 1e-5);

    float3 KS = F;
    float3 KD = 1.0 - KS;
    KD *= (1.0 - metallic);
    float3 diffuse = (KD * albedo) / 3.1415926535;

    float3 radiance = lightColor.xyz * NdotL;
    float3 Lo = (diffuse + specular) * radiance;

    float3 ambient = albedo * ao * 0.15;

    float3 finalColor = ambient + Lo;
    finalColor = pow(finalColor, 1.0 / 2.2);

    return float4(finalColor, 1.0);
}
