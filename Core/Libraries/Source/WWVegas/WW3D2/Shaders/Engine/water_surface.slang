// Modern river/standing-water material for the RA3 water resource model.

struct WaterSurfaceVertexInput
{
    float3 position : POSITION0;
    float3 normal : NORMAL0;
    float4 color0 : COLOR0;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
};

struct WaterSurfaceVertexOutput
{
    float4 position : SV_POSITION;
    float4 color0 : COLOR0;
    float2 flow_uv : TEXCOORD0;
    float2 normal_uv : TEXCOORD1;
    float2 shroud_uv : TEXCOORD2;
    float3 projective_position : TEXCOORD3;
    float3 world_position : TEXCOORD4;
    float3 normal : TEXCOORD5;
};

cbuffer WaterSurfaceVertexConstants : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 View;
    row_major float4x4 Projection;
    float4 Viewport;
    float4 WaterShroudProjection;
    float4 WaterAnimation;
    float4 WaterCameraPosition;
};

[shader("vertex")] WaterSurfaceVertexOutput WaterSurfaceVertexMain(
    WaterSurfaceVertexInput input)
{
    WaterSurfaceVertexOutput output;
    float4 world_position = mul(World, float4(input.position, 1.0));
    float4 camera_position = mul(View, world_position);
    float4 clip_position = mul(Projection, camera_position);

    output.position = clip_position;
    output.color0 = input.color0;
    output.flow_uv = input.texcoord0;
    output.normal_uv = input.texcoord1;
    output.shroud_uv = world_position.xy * WaterShroudProjection.xy +
        WaterShroudProjection.zw;
    output.projective_position = float3(
        clip_position.x * 0.5 + clip_position.w * 0.5,
        clip_position.w * 0.5 - clip_position.y * 0.5,
        clip_position.w);
    output.world_position = world_position.xyz;
    output.normal = normalize(mul(World, float4(input.normal, 0.0)).xyz);
    return output;
}

Texture2D WaterSurfaceTexture : register(t0);
Texture2D WaterNormalTexture : register(t1);
Texture2D WaterFoamTexture : register(t2);
Texture2D WaterEdgeTexture : register(t3);
Texture2D WaterReflectionTexture : register(t4);
Texture2D WaterRefractionTexture : register(t5);
Texture2D WaterEnvironmentTexture : register(t6);
Texture2D WaterShroudTexture : register(t7);
SamplerState WaterSurfaceSampler : register(s0);
SamplerState WaterNormalSampler : register(s1);
SamplerState WaterFoamSampler : register(s2);
SamplerState WaterEdgeSampler : register(s3);
SamplerState WaterReflectionSampler : register(s4);
SamplerState WaterRefractionSampler : register(s5);
SamplerState WaterEnvironmentSampler : register(s6);
SamplerState WaterShroudSampler : register(s7);

cbuffer WaterSurfacePixelConstants : register(b1)
{
    float4 MaterialDiffuse;
    float4 MaterialAmbient;
    float4 MaterialEmissive;
    float4 MaterialSpecular;
    float4 FogColor;
    float4 TextureFactor;
    float4 WaterTint;
    float4 WaterEffects;
    float4 WaterLegacyConstants[6];
    float4 WaterFogState;
};

struct WaterSurfacePixelInput
{
    float4 position : SV_POSITION;
    float4 color0 : COLOR0;
    float2 flow_uv : TEXCOORD0;
    float2 normal_uv : TEXCOORD1;
    float2 shroud_uv : TEXCOORD2;
    float3 projective_position : TEXCOORD3;
    float3 world_position : TEXCOORD4;
    float3 normal : TEXCOORD5;
};

float3 Decode_River_Normal(float4 sample_value)
{
    return normalize(float3(sample_value.rg * 2.0 - 1.0, 1.0));
}

float2 Project_River_Position(float3 projective_position)
{
    return projective_position.xy / max(abs(projective_position.z), 0.000001);
}

float2 Environment_UV(float3 direction)
{
    const float pi = 3.14159265;
    return float2(
        atan2(direction.y, direction.x) / (2.0 * pi) + 0.5,
        0.5 - asin(clamp(direction.z, -1.0, 1.0)) / pi);
}

float Water_Fog_Factor(float3 world_position)
{
    if (WaterFogState.z < 0.5)
        return 0.0;
    float distance_to_camera = length(WaterLegacyConstants[1].xyz -
        world_position);
    return saturate((distance_to_camera - WaterFogState.x) /
        max(WaterFogState.y - WaterFogState.x, 0.0001));
}

[shader("pixel")] float4 WaterSurfacePixelMain(
    WaterSurfacePixelInput input) : SV_TARGET
{
    float phase = WaterLegacyConstants[0].z;

    // RiverWater.fx uses two flow samples and two divergent normal octaves.
    float2 river_uv0 = input.flow_uv * float2(1.0, 0.5) +
        float2(0.0, -phase * 0.004);
    float2 river_uv1 = input.flow_uv.yx + float2(0.0, -phase * 0.016);
    float4 river0 = WaterSurfaceTexture.Sample(WaterSurfaceSampler,
        river_uv0);
    float4 river1 = WaterSurfaceTexture.Sample(WaterSurfaceSampler,
        river_uv1);
    float3 river = river0.rgb + river1.rgb;

    float2 normal_uv0 = input.normal_uv * float2(1.0, 0.9) +
        float2(0.0, phase * 0.03);
    float2 normal_uv1 = input.normal_uv.yx * float2(1.0, 2.0) +
        float2(0.0, -phase * 0.08);
    float3 normal0 = Decode_River_Normal(WaterNormalTexture.Sample(
        WaterNormalSampler, normal_uv0));
    float3 normal1 = Decode_River_Normal(WaterNormalTexture.Sample(
        WaterNormalSampler, normal_uv1));
    float3 surface_normal = normalize(lerp(input.normal,
        normalize(float3((normal0.xy + normal1.xy) * 0.5, 1.0)), 0.8));
    float3 view_direction = normalize(WaterLegacyConstants[1].xyz -
        input.world_position);
    float fresnel = pow(1.0 - saturate(abs(dot(surface_normal,
        view_direction))), 5.0);
    float3 color = river * input.color0.rgb * WaterTint.rgb * 0.62;

    float foam0 = dot(WaterFoamTexture.Sample(WaterFoamSampler,
        input.world_position.xy * 0.005 + float2(phase * 0.011, 0.0)).rgb,
        float3(0.333333, 0.333333, 0.333333));
    float foam1 = dot(WaterFoamTexture.Sample(WaterFoamSampler,
        input.world_position.xy * 0.004 + float2(-phase * 0.011, 0.0)).rgb,
        float3(0.333333, 0.333333, 0.333333));
    float foam = saturate(foam0 * foam1);

    float2 projective_uv = Project_River_Position(input.projective_position);
    float2 distorted_uv = saturate(projective_uv + (normal0.xy + normal1.xy) *
        (0.045 + fresnel * 0.11));
    if (WaterEffects.z > 0.5)
    {
        float3 refracted = WaterRefractionTexture.SampleLevel(
            WaterRefractionSampler, distorted_uv, 0.0).rgb;
        color = lerp(color, refracted * WaterTint.rgb,
            (1.0 - fresnel) * 0.42);
    }

    if (WaterEffects.x > 0.001)
    {
        float3 reflection = WaterReflectionTexture.SampleLevel(
            WaterReflectionSampler, distorted_uv, 0.0).rgb;
        color += reflection * WaterEffects.x * (0.40 + fresnel * 0.85);
    }

    float3 environment_direction = reflect(-view_direction, surface_normal);
    color += WaterEnvironmentTexture.Sample(WaterEnvironmentSampler,
        saturate(Environment_UV(environment_direction))).rgb *
        (0.05 + fresnel * 0.26);
    color += pow(foam, 2.0) * (0.12 + fresnel * 0.28);

    if (WaterEffects.w > 0.5)
    {
        float depth = saturate((WaterLegacyConstants[0].w -
            input.world_position.z) / max(abs(WaterLegacyConstants[0].w), 4.0));
        float depth_fade = WaterEnvironmentTexture.Sample(
            WaterEnvironmentSampler, float2(depth, 0.5)).r;
        float caustic0 = WaterFoamTexture.Sample(WaterFoamSampler,
            input.world_position.xy * 0.018 + float2(phase * 0.010,
            -phase * 0.007)).r;
        float caustic1 = WaterFoamTexture.Sample(WaterFoamSampler,
            input.world_position.yx * 0.025 + float2(-phase * 0.013,
            phase * 0.009)).g;
        color += pow(saturate(caustic0 * caustic1), 2.0) * depth_fade *
            float3(0.25, 0.55, 0.70);
        color = lerp(color, color * float3(0.18, 0.58, 0.80), 0.42);
    }

    if (WaterEffects.y > 0.5)
        color *= WaterShroudTexture.Sample(WaterShroudSampler,
            input.shroud_uv).rgb;

    float edge_sample = WaterEdgeTexture.Sample(WaterEdgeSampler,
        input.normal_uv).r;
    float edge = max(edge_sample, 0.001);
    float alpha = saturate(input.color0.a * max(river0.a, 0.75) * edge);
    color = lerp(color, FogColor.rgb, Water_Fog_Factor(input.world_position));
    return float4(saturate(color), min(alpha, 0.95));
}
