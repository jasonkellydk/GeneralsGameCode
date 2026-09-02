#include "TerrainMaterial.slang"

[shader("pixel")] float4 main(EnginePixelInput input) : SV_TARGET
{
    return float4(ComposeShroudedTerrain(input, 2u), 1.0);
}
