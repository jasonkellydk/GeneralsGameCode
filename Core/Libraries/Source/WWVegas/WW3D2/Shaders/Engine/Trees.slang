#include "WW3D2ShaderCommon.slang"

[shader("pixel")] float4 main(EnginePixelInput input) : SV_TARGET
{
    float4 texture = StageTexture0.Sample(StageSampler0, input.texcoord0.xy);
    float4 result = input.color0 * texture;
    result.rgb *= 2.0 * LegacyPixel[1].rgb;
    result.a = texture.a;
    return result;
}
