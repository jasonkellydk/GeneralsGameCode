#include "WW3D2ShaderCommon.slang"

[shader("pixel")] float4 main(EnginePixelInput input) : SV_TARGET
{
    float4 texture = StageTexture0.Sample(StageSampler0, input.texcoord0.xy);
    float grey = dot(texture.xyz, LegacyPixel[0].xyz);
    float4 filtered = grey * LegacyPixel[1];
    return lerp(texture, filtered, LegacyPixel[2]);
}
