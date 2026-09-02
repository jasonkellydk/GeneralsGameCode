#include "TerrainMaterial.slang"

[shader("pixel")] float4 main(EnginePixelInput input) : SV_TARGET
{
    return float4(StageTexture1.Sample(StageSampler1,
        TerrainProjectedUV(1u, input)).rgb, 1.0);
}
