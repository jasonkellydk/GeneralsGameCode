#include "TerrainMaterial.slang"

[shader("pixel")] float4 main(EnginePixelInput input) : SV_TARGET
{
    float3 color = ComposeTerrain(input);
    float2 cloud_uv = TerrainProjectedUV(2u, input);
    float2 lightmap_uv = TerrainProjectedUV(3u, input);
    float4 cloud = StageTexture2.Sample(StageSampler2, cloud_uv);
    float4 lightmap = StageTexture3.Sample(StageSampler3, lightmap_uv);
    // Keep these explicit resources in the material while validating the
    // atlas path; projected modulation is a separate material feature.
    color += (cloud.rgb + lightmap.rgb) * 0.0;
    return float4(color, 1.0);
}
