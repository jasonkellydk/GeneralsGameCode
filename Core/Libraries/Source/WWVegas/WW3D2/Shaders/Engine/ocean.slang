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
    float2 displacement_uv : TEXCOORD6;
    float3 projective_position : TEXCOORD7;
    float3 world_position : TEXCOORD8;
    float4 depth_projection : TEXCOORD9;
};

cbuffer OceanVertexConstants : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 View;
    row_major float4x4 Projection;
    float4 Viewport;
    float4 LegacyVertex[96];
};

#define WaterShroudProjection LegacyVertex[0]
#define WaterAnimation LegacyVertex[1]
#define WaterCameraPosition LegacyVertex[2]
#define WaterDisplacementDomain LegacyVertex[3]

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

    // OceanDisplacement.fx produces this field in a separate render pass.
    // Keep the authored static DDS out of this sampling path; it is an input
    // to the producer, not the dynamic displacement resource.
    float2 displacement_uv = (world_position.xy -
        WaterDisplacementDomain.xy) /
        max(abs(WaterDisplacementDomain.zw), float2(1.0, 1.0));
    displacement_uv = saturate(float2(displacement_uv.x,
        1.0 - displacement_uv.y));
    float3 wave_displacement = OceanDisplacementTexture.SampleLevel(
        OceanDisplacementSampler, displacement_uv, 0.0).xyz;
    wave_displacement = min(wave_displacement, 0.13);
    wave_displacement.x *= -1.0;
    world_position.xyz += wave_displacement * 300.0;

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
    output.displacement_uv = displacement_uv;

    // Projective coordinates are shared by the reflection and refraction
    // render targets, just as in RA3's Ocean.fx.
    output.projective_position = float3(
        clip_position.x * 0.5 + clip_position.w * 0.5,
        clip_position.w * 0.5 - clip_position.y * 0.5,
        clip_position.w);
    output.world_position = world_position.xyz;
    output.depth_projection = float4(
        Projection[2][2], Projection[2][3],
        Projection[3][2], Projection[3][3]);
    return output;
}

Texture2D OceanSurfaceTexture : register(t0);
Texture2D OceanNormalTexture : register(t2);
Texture2D OceanFoamTexture : register(t3);
Texture2D OceanReflectionTexture : register(t4);
Texture2D OceanRefractionTexture : register(t5);
Texture2D OceanEnvironmentTexture : register(t6);
Texture2D OceanShroudTexture : register(t7);
Texture2D OceanSceneDepthTexture : register(t8);
SamplerState OceanSurfaceSampler : register(s0);
SamplerState OceanNormalSampler : register(s2);
SamplerState OceanFoamSampler : register(s3);
SamplerState OceanReflectionSampler : register(s4);
SamplerState OceanRefractionSampler : register(s5);
SamplerState OceanEnvironmentSampler : register(s6);
SamplerState OceanShroudSampler : register(s7);
SamplerState OceanSceneDepthSampler : register(s8);

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
    float4 WaterMaterialConstants[6];
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
    float2 displacement_uv : TEXCOORD6;
    float3 projective_position : TEXCOORD7;
    float3 world_position : TEXCOORD8;
    float4 depth_projection : TEXCOORD9;
};

float3 Decode_Water_Normal(float4 sample_value)
{
    return sample_value.rgb * 2.0 - 1.0;
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
    float distance_to_camera = length(WaterMaterialConstants[1].xyz -
        world_position);
    return saturate((distance_to_camera - WaterFogState.x) /
        max(WaterFogState.y - WaterFogState.x, 0.0001));
}

float Linearize_Ocean_Depth(float depth, float4 projection)
{
    float numerator = projection.y - depth * projection.w;
    float denominator = depth * projection.z - projection.x;
    return abs(numerator / (abs(denominator) > 0.00001 ? denominator :
        0.00001));
}

[shader("pixel")] float4 OceanPixelMain(OceanPixelInput input) : SV_TARGET
{
    float phase = WaterMaterialConstants[0].z;
    float4 surface0 = OceanSurfaceTexture.Sample(OceanSurfaceSampler,
        input.surface_uv);
    float4 surface1 = OceanSurfaceTexture.Sample(OceanSurfaceSampler,
        input.surface_uv * 1.37 + float2(0.23, -0.17));
    float3 surface = lerp(surface0.rgb, surface1.rgb, 0.35);

    float3 static_displacement0 = Decode_Water_Normal(OceanNormalTexture.Sample(
        OceanNormalSampler, input.normal_uv0));
    float3 static_displacement1 = Decode_Water_Normal(OceanNormalTexture.Sample(
        OceanNormalSampler, input.normal_uv1));
    float3 static_displacement = static_displacement0 +
        static_displacement1;
    float height_center = OceanDisplacementTexture.SampleLevel(
        OceanDisplacementSampler, input.displacement_uv, 0.0).z;
    float height_x = OceanDisplacementTexture.SampleLevel(
        OceanDisplacementSampler, input.displacement_uv +
            float2(1.0 / 256.0, 0.0), 0.0).z;
    float height_y = OceanDisplacementTexture.SampleLevel(
        OceanDisplacementSampler, input.displacement_uv +
            float2(0.0, 1.0 / 256.0), 0.0).z;
    float3 displacement_normal = float3(height_x - height_center,
        height_y - height_center, 0.01) * 50.0;
    displacement_normal = normalize(displacement_normal +
        static_displacement * 0.2);
    float3 surface_normal = normalize(displacement_normal * 50.0 +
        float3(0.0, 0.0, 1.0));
    float3 view_direction = normalize(WaterMaterialConstants[1].xyz -
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

    // Generals' TransparentWaterDepth describes the physical thickness at
    // which the water reaches its deep-water opacity. The opaque scene depth
    // snapshot supplies the terrain/geometry depth behind this water pixel.
    float water_depth_fade = 0.0;
    float authored_alpha = saturate(input.color0.a * WaterTint.a);
    float water_alpha = authored_alpha;
    if (WaterMaterialConstants[3].w > 0.5)
    {
        float scene_depth = OceanSceneDepthTexture.SampleLevel(
            OceanSceneDepthSampler, saturate(projective_uv), 0.0).r;
        if (scene_depth >= 0.9999)
        {
            // Clear depth is the deep-water fallback: no opaque surface is
            // available behind this pixel for a thickness measurement.
            water_depth_fade = 1.0;
            water_alpha = saturate(WaterMaterialConstants[3].z);
        }
        else
        {
            float water_view_depth = Linearize_Ocean_Depth(input.position.z,
                input.depth_projection);
            float scene_view_depth = Linearize_Ocean_Depth(scene_depth,
                input.depth_projection);
            float thickness = max(scene_view_depth - water_view_depth, 0.0);
            water_depth_fade = saturate(thickness /
                max(WaterMaterialConstants[3].y, 0.001));
            // TransparentWaterMinOpacity is the deep-water opacity. Keep the
            // shoreline transparent until terrain thickness accumulates.
            water_alpha = water_depth_fade *
                saturate(WaterMaterialConstants[3].z);
        }
    }

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
            refraction_weight * lerp(1.0, 0.35, water_depth_fade) * 0.55);
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
        float depth = saturate((WaterMaterialConstants[0].w -
            input.world_position.z) / max(abs(WaterMaterialConstants[0].w), 4.0));
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
    return float4(saturate(color), water_alpha);
}
