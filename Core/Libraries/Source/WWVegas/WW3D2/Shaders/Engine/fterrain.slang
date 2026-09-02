#include "TerrainMaterial.slang"

[shader("pixel")] float4 main(EnginePixelInput input) : SV_TARGET
{
    return float4(ComposeShroudedTerrain(input, 0u), 1.0);
}
