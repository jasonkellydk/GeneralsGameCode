#include "WW3D2ShaderCommon.slang"

[shader("pixel")] float4 main(EnginePixelInput input) : SV_TARGET
{
    float4 texture0 = StageTexture0.Sample(StageSampler0, input.texcoord0.xy);
    float4 bump = StageTexture1.Sample(StageSampler1, input.texcoord1.xy);
    float2 reflection_uv = LegacyTexBemUV(input.texcoord2.xy, bump,
        BumpMatrix[2], BumpParams[2]);
    float4 reflection = StageTexture2.Sample(StageSampler2, reflection_uv);

    float4 result = input.color0 * texture0;
    result.rgb += reflection.rgb * LegacyPixel[0].rgb;
    return result;
}
