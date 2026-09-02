#include "WW3D2ShaderCommon.slang"

[shader("pixel")] float4 main(EnginePixelInput input) : SV_TARGET
{
    float4 source = StageTexture0.Sample(StageSampler0, input.texcoord0.xy);
    float red = source.r;
    float green = source.g;
    float blue = source.b;
    float3 result = blue * LegacyPixel[0].rgb +
        green * LegacyPixel[1].rgb + red * LegacyPixel[2].rgb;
    return float4(result, source.a);
}
