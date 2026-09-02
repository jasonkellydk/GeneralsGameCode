// RA3-style terrain material.
//
// One material source owns the complete terrain contract.  The old NVP
// implementation selected several fixed-function texture-stage programs from
// W3DShaderManager.  This shader has explicit resources, explicit projected
// coordinates, and data-driven feature switches like the RA3 terrain model.

Texture2D TerrainDiffuseTexture : register(t0);
Texture2D TerrainBlendTexture : register(t1);
Texture2D TerrainCloudTexture : register(t2);
Texture2D TerrainLightMapTexture : register(t3);
Texture2D TerrainShroudTexture : register(t4);

SamplerState TerrainDiffuseSampler : register(s0);
SamplerState TerrainBlendSampler : register(s1);
SamplerState TerrainCloudSampler : register(s2);
SamplerState TerrainLightMapSampler : register(s3);
SamplerState TerrainShroudSampler : register(s4);

struct TerrainVertexInput
{
    float3 position : POSITION0;
    float4 color0 : COLOR0;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
};

struct TerrainVertexOutput
{
    float4 position : SV_POSITION;
    float4 color0 : COLOR0;
    float2 diffuse_uv : TEXCOORD0;
    float2 blend_uv : TEXCOORD1;
    float2 cloud_uv : TEXCOORD2;
    float2 lightmap_uv : TEXCOORD3;
    float2 shroud_uv : TEXCOORD4;
    float3 world_position : TEXCOORD5;
};

cbuffer TerrainVertexConstants : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 View;
    row_major float4x4 Projection;
    float4 Viewport;
    float4 TerrainCloudProjection;
    float4 TerrainLightMapProjection;
    float4 TerrainShroudProjection;
    float4 TerrainVertexOptions;
};

cbuffer TerrainPixelConstants : register(b1)
{
    // These six values preserve the backend's shared material header.  The
    // terrain material does not use the old texture-stage state that follows.
    float4 MaterialDiffuse;
    float4 MaterialAmbient;
    float4 MaterialEmissive;
    float4 MaterialSpecular;
    float4 FogColor;
    float4 TextureFactor;
    float4 TerrainFeatures;
    float4 TerrainLighting;
    float4 TerrainOptions;
};

float2 TerrainProjectedCoordinate(float3 world_position, float4 projection)
{
    return world_position.xy * projection.xy + projection.zw;
}

[shader("vertex")] TerrainVertexOutput TerrainVertexMain(TerrainVertexInput input)
{
    TerrainVertexOutput output;
    float4 world_position = mul(World, float4(input.position, 1.0));
    float4 camera_position = mul(View, world_position);

    output.position = mul(Projection, camera_position);
    output.color0 = input.color0;
    output.diffuse_uv = input.texcoord0;
    output.blend_uv = input.texcoord1;
    output.cloud_uv = TerrainProjectedCoordinate(world_position.xyz,
        TerrainCloudProjection);
    output.lightmap_uv = TerrainProjectedCoordinate(world_position.xyz,
        TerrainLightMapProjection);
    output.shroud_uv = TerrainProjectedCoordinate(world_position.xyz,
        TerrainShroudProjection);
    output.world_position = world_position.xyz;
    return output;
}

struct TerrainPixelInput
{
    float4 position : SV_POSITION;
    float4 color0 : COLOR0;
    float2 diffuse_uv : TEXCOORD0;
    float2 blend_uv : TEXCOORD1;
    float2 cloud_uv : TEXCOORD2;
    float2 lightmap_uv : TEXCOORD3;
    float2 shroud_uv : TEXCOORD4;
    float3 world_position : TEXCOORD5;
};

[shader("pixel")] float4 TerrainPixelMain(TerrainPixelInput input) : SV_TARGET
{
	if (TerrainFeatures.z > 0.5)
	{
		return float4(input.color0.rgb * TerrainLighting.rgb, 1.0);
	}

    float4 color = float4(0.0, 0.0, 0.0, 1.0);
    if (TerrainFeatures.w > 2.5)
    {
        // Third-layer tiles are an alpha overlay over the already-composed
        // terrain. Their vertex alpha is the overlay coverage.
        color = TerrainDiffuseTexture.SampleLevel(TerrainDiffuseSampler,
            input.diffuse_uv, 0.0);
        color.rgb *= input.color0.rgb;
        color.a = input.color0.a;
    }
    else if (TerrainFeatures.w > 1.5)
    {
        // The shroud texture stores its modulation in RGB (R5G6B5 in the
        // release path), so this pass is intentionally independent of alpha.
        float3 shroud = TerrainShroudTexture.SampleLevel(TerrainShroudSampler,
            input.shroud_uv, 0.0).rgb;
        return float4(shroud * TerrainLighting.rgb, 1.0);
    }

    else if (TerrainFeatures.w > 0.5)
    {
        // FlatHeightMap supplies one already-composed atlas.  Keep this a
        // distinct material mode so the flat LOD does not enter the regular
        // two-atlas blend contract when it replaces the full heightmap. The
        // mode is carried by the pixel feature block because b0 is vertex-only.
        color = TerrainDiffuseTexture.SampleLevel(TerrainDiffuseSampler,
            input.diffuse_uv, 0.0);
        color.rgb *= input.color0.rgb;
    }
    else
    {
        float4 base = TerrainDiffuseTexture.SampleLevel(TerrainDiffuseSampler,
            input.diffuse_uv, 0.0);
        float4 blend = TerrainBlendTexture.SampleLevel(TerrainBlendSampler,
            input.blend_uv, 0.0);
        color = lerp(base, blend, saturate(input.color0.a));
        color.rgb *= input.color0.rgb;
    }

    if (TerrainFeatures.x > 0.5)
    {
        color.rgb *= TerrainCloudTexture.SampleLevel(TerrainCloudSampler,
            input.cloud_uv, 0.0).rgb;
    }
    if (TerrainFeatures.y > 0.5)
    {
        color.rgb *= TerrainLightMapTexture.SampleLevel(TerrainLightMapSampler,
            input.lightmap_uv, 0.0).rgb;
    }
    color.rgb *= TerrainLighting.rgb;

    // RA3 applies shroud modulation as part of the terrain material instead
    // of issuing a second depth-equal terrain draw. This keeps visibility
    // coverage and terrain depth in the same programmable pass.
    if (TerrainOptions.y > 0.5)
    {
        color.rgb *= TerrainShroudTexture.SampleLevel(TerrainShroudSampler,
            input.shroud_uv, 0.0).rgb;
    }
    return color;
}
