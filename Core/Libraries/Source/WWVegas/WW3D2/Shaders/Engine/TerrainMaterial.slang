#include "WW3D2ShaderCommon.slang"

// Explicit terrain material interface. Terrain shaders own sampling and
// composition; the renderer supplies resources and interpolated inputs.
float4 TerrainAtlasSample(float2 uv)
{
    return StageTexture0.Sample(StageSampler0, uv);
}

float4 TerrainBlendSample(float2 uv)
{
    return StageTexture1.Sample(StageSampler1, uv);
}

float3 TerrainShroudSample(EnginePixelInput input)
{
    return StageTexture0.Sample(StageSampler0,
        TerrainProjectedUV(0u, input)).rgb;
}

float3 TerrainStageSample(uint stage, EnginePixelInput input)
{
    if (stage == 2u)
        return StageTexture2.Sample(StageSampler2,
            TerrainProjectedUV(stage, input)).rgb;
    return StageTexture3.Sample(StageSampler3,
        TerrainProjectedUV(stage, input)).rgb;
}

float2 TerrainProjectedUV(uint stage, EnginePixelInput input)
{
    return saturate(EngineStageCoordinate(stage, input).xy);
}

float3 ComposeTerrain(EnginePixelInput input)
{
    float4 atlas = TerrainAtlasSample(input.texcoord0.xy);
    float4 blend = TerrainBlendSample(input.texcoord1.xy);
    return lerp(atlas.rgb, blend.rgb, saturate(input.color0.a));
}

float3 ComposeShroudedTerrain(EnginePixelInput input, uint noise_count)
{
    float3 color = TerrainShroudSample(input);
    color *= StageTexture1.Sample(StageSampler1,
        TerrainProjectedUV(1u, input)).rgb;
    if (noise_count > 0u)
        color *= TerrainStageSample(2u, input);
    if (noise_count > 1u)
        color *= TerrainStageSample(3u, input);
    return color;
}
