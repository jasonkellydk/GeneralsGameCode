#include "TerrainMaterial.slang"

[shader("pixel")] float4 main(EnginePixelInput input) : SV_TARGET
{
    return float4(ComposeShroudedTerrain(input, 1u), 1.0);
}
