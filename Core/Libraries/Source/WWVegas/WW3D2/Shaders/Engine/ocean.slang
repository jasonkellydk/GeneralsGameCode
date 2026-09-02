// Modern ocean material for the RA3 water resource model.
//
// The material consumes the complete surface contract: animated static
// displacement, two normal octaves, foam, planar reflection, scene
// refraction, environment reflection, shroud, and underwater caustics.

struct OceanVertexInput
{
    float3 position : POSITION0;
    float4 color0 : COLOR0;
    float2 texcoord0 : TEXCOORD0;
};

struct OceanVertexOutput
{
    float4 position : SV_POSITION;
    float4 color0 : COLOR0;
    float2 surface_uv : TEXCOORD0;
    float2 normal_uv0 : TEXCOORD1;
    float2 normal_uv1 : TEXCOORD2;
    float2 foam_uv0 : TEXCOORD3;
    float2 foam_uv1 : TEXCOORD4;
    float2 shroud_uv : TEXCOORD5;
    float3 projective_position : TEXCOORD6;
    float3 world_position : TEXCOORD7;
};

cbuffer OceanVertexConstants : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 View;
    row_major float4x4 Projection;
    float4 Viewport;
    float4 WaterShroudProjection;
    float4 WaterAnimation;
    float4 WaterCameraPosition;
};

float2 Rotate_Ocean_UV(float2 uv)
{
    const float cosine = 0.5735764;
    const float sine = 0.8191520;
    return float2(
        dot(uv, float2(cosine, -sine)),
        dot(uv, float2(sine, cosine)));
}

Texture2D OceanDisplacementTexture : register(t1);
SamplerState OceanDisplacementSampler : register(s1);

[shader("vertex")] OceanVertexOutput OceanVertexMain(
    OceanVertexInput input)
{
    OceanVertexOutput output;
    float4 world_position = mul(World, float4(input.position, 1.0));
    const float phase = WaterAnimation.z;

    // RA3's high path combines a static displacement map with three broad
    // directional waves. The map is sampled explicitly in the vertex stage,
    // which is the DX11 equivalent of OceanDisplacement.fx.
    float2 displacement_uv = world_position.xy * 0.0022 +
        WaterAnimation.xy * 0.06;
    float static_height = OceanDisplacementTexture.SampleLevel(
        OceanDisplacementSampler, displacement_uv, 0.0).r;
    world_position.z += (static_height - 0.5) * 0.075;

    float wave0 = sin(dot(world_position.xy, float2(0.2, -0.7)) * 0.1 + phase) * 0.10;
    float wave1 = sin(dot(world_position.xy, float2(-1.0, -0.7)) * 0.25 + phase * 0.5) * 0.05;
    float wave2 = sin(dot(world_position.xy, float2(-1.0, 0.2)) * 0.15 + phase * 1.5) * 0.05;
    world_position.z += wave0 + wave1 + wave2;

    float4 camera_position = mul(View, world_position);
    float4 clip_position = mul(Projection, camera_position);
    output.position = clip_position;
    output.color0 = input.color0;

    output.surface_uv = world_position.xy * 0.004 + WaterAnimation.xy * 0.1;
    float2 normal_base = world_position.xy * 0.0025;
    output.normal_uv0 = Rotate_Ocean_UV(normal_base) +
        float2(phase * 0.015, phase * 0.004);
    output.normal_uv1 = Rotate_Ocean_UV(float2(
        dot(normal_base, float2(0.5735764, 0.8191520)),
        dot(normal_base, float2(-0.8191520, 0.5735764)))) +
        float2(-phase * 0.011, phase * 0.006);
    output.foam_uv0 = world_position.xy * 0.005 +
        float2(phase * 0.011, -phase * 0.003);
    output.foam_uv1 = world_position.xy * 0.004 +
        float2(-phase * 0.011, phase * 0.004);
    output.shroud_uv = world_position.xy * WaterShroudProjection.xy +
        WaterShroudProjection.zw;

    // Projective coordinates are shared by the reflection and refraction
    // render targets, just as in RA3's Ocean.fx.
    output.projective_position = float3(
        clip_position.x * 0.5 + clip_position.w * 0.5,
        clip_position.w * 0.5 - clip_position.y * 0.5,
        clip_position.w);
    output.world_position = world_position.xyz;
    return output;
}

Texture2D OceanSurfaceTexture : register(t0);
Texture2D OceanNormalTexture : register(t2);
Texture2D OceanFoamTexture : register(t3);
Texture2D OceanReflectionTexture : register(t4);
Texture2D OceanRefractionTexture : register(t5);
Texture2D OceanEnvironmentTexture : register(t6);
Texture2D OceanShroudTexture : register(t7);
SamplerState OceanSurfaceSampler : register(s0);
SamplerState OceanNormalSampler : register(s2);
SamplerState OceanFoamSampler : register(s3);
SamplerState OceanReflectionSampler : register(s4);
SamplerState OceanRefractionSampler : register(s5);
SamplerState OceanEnvironmentSampler : register(s6);
SamplerState OceanShroudSampler : register(s7);

cbuffer OceanPixelConstants : register(b1)
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

struct OceanPixelInput
{
    float4 position : SV_POSITION;
    float4 color0 : COLOR0;
    float2 surface_uv : TEXCOORD0;
    float2 normal_uv0 : TEXCOORD1;
    float2 normal_uv1 : TEXCOORD2;
    float2 foam_uv0 : TEXCOORD3;
    float2 foam_uv1 : TEXCOORD4;
    float2 shroud_uv : TEXCOORD5;
    float3 projective_position : TEXCOORD6;
    float3 world_position : TEXCOORD7;
};

float3 Decode_Water_Normal(float4 sample_value)
{
    return normalize(float3(sample_value.rg * 2.0 - 1.0, 1.0));
}

float2 Project_Water_Position(float3 projective_position)
{
    return projective_position.xy / max(abs(projective_position.z), 0.000001);
}

float2 Environment_UV(float3 direction)
{
    const float pi = 3.14159265;
    float longitude = atan2(direction.y, direction.x) / (2.0 * pi);
    float latitude = asin(clamp(direction.z, -1.0, 1.0)) / pi;
    return float2(longitude + 0.5, 0.5 - latitude);
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

[shader("pixel")] float4 OceanPixelMain(OceanPixelInput input) : SV_TARGET
{
    float phase = WaterLegacyConstants[0].z;
    float4 surface0 = OceanSurfaceTexture.Sample(OceanSurfaceSampler,
        input.surface_uv);
    float4 surface1 = OceanSurfaceTexture.Sample(OceanSurfaceSampler,
        input.surface_uv * 1.37 + float2(0.23, -0.17));
    float3 surface = lerp(surface0.rgb, surface1.rgb, 0.35);

    float3 normal0 = Decode_Water_Normal(OceanNormalTexture.Sample(
        OceanNormalSampler, input.normal_uv0));
    float3 normal1 = Decode_Water_Normal(OceanNormalTexture.Sample(
        OceanNormalSampler, input.normal_uv1));
    float3 surface_normal = normalize(float3(
        (normal0.xy + normal1.xy) * 0.5, 1.0));
    float3 view_direction = normalize(WaterLegacyConstants[1].xyz -
        input.world_position);
    float fresnel = pow(1.0 - saturate(abs(dot(surface_normal,
        view_direction))), 5.0);

    float3 color = surface * input.color0.rgb * WaterTint.rgb;
    color *= 0.55 + surface_normal.z * 0.45;

    float foam0 = dot(OceanFoamTexture.Sample(OceanFoamSampler,
        input.foam_uv0).rgb, float3(0.333333, 0.333333, 0.333333));
    float foam1 = dot(OceanFoamTexture.Sample(OceanFoamSampler,
        input.foam_uv1).rgb, float3(0.333333, 0.333333, 0.333333));
    float foam = saturate(foam0 * foam1);

    float2 projective_uv = Project_Water_Position(input.projective_position);
    float2 distorted_uv = saturate(projective_uv + surface_normal.xy *
        (0.02 + fresnel * 0.09));

    if (WaterEffects.z > 0.5)
    {
        float3 refracted = OceanRefractionTexture.SampleLevel(
            OceanRefractionSampler, distorted_uv, 0.0).rgb;
        float3 refracted_base = OceanRefractionTexture.SampleLevel(
            OceanRefractionSampler, saturate(projective_uv), 0.0).rgb;
        // Keep geometry immediately below the surface from bleeding through
        // as a hard color discontinuity, matching the RA3 refraction guard.
        float refraction_delta = length(refracted - refracted_base);
        float refraction_weight = (1.0 - fresnel) *
            (0.35 + saturate(refraction_delta * 4.0) * 0.35);
        color = lerp(color, refracted * WaterTint.rgb,
            refraction_weight * 0.55);
    }

    if (WaterEffects.x > 0.001)
    {
        float2 reflection_uv = saturate(projective_uv + surface_normal.xy *
            (0.035 + fresnel * 0.12));
        float3 reflection = OceanReflectionTexture.SampleLevel(
            OceanReflectionSampler, reflection_uv, 0.0).rgb;
        color += reflection * WaterEffects.x * (0.45 + fresnel * 0.85);
    }

    float3 environment_direction = reflect(-view_direction, surface_normal);
    float3 environment = OceanEnvironmentTexture.Sample(
        OceanEnvironmentSampler, saturate(Environment_UV(
            environment_direction))).rgb;
    color += environment * (0.08 + fresnel * 0.42);
    color += pow(foam, 2.0) * (0.08 + fresnel * 0.24);

    if (WaterEffects.w > 0.5)
    {
        float depth = saturate((WaterLegacyConstants[0].w -
            input.world_position.z) / max(abs(WaterLegacyConstants[0].w), 4.0));
        float depth_fade = OceanEnvironmentTexture.Sample(
            OceanEnvironmentSampler, float2(depth, 0.5)).r;
        float caustic0 = OceanFoamTexture.Sample(OceanFoamSampler,
            input.world_position.xy * 0.018 + float2(phase * 0.010,
            -phase * 0.007)).r;
        float caustic1 = OceanFoamTexture.Sample(OceanFoamSampler,
            input.world_position.yx * 0.025 + float2(-phase * 0.013,
            phase * 0.009)).g;
        float caustics = pow(saturate(caustic0 * caustic1), 2.0) *
            depth_fade;
        color += caustics * float3(0.32, 0.64, 0.68);
        color = lerp(color, color * float3(0.18, 0.58, 0.80), 0.42);
    }

    if (WaterEffects.y > 0.5)
        color *= OceanShroudTexture.Sample(OceanShroudSampler,
            input.shroud_uv).rgb;

    color = lerp(color, FogColor.rgb, Water_Fog_Factor(input.world_position));
    return float4(saturate(color), saturate(input.color0.a * WaterTint.a));
}
