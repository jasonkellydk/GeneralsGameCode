#include "WW3D2ShaderCommon.slang"

[shader("pixel")] float4 main(EnginePixelInput input) : SV_TARGET
{
    float4 texture0 = StageTexture0.Sample(StageSampler0, input.texcoord0.xy);
    float4 bump = StageTexture1.Sample(StageSampler1, input.texcoord1.xy);
    float2 reflection_uv = LegacyTexBemUV(input.texcoord2.xy, bump,
        BumpMatrix[2], BumpParams[2]);
    float4 texture2 = StageTexture2.Sample(StageSampler2, reflection_uv);
    float4 texture3 = StageTexture3.Sample(StageSampler3, input.texcoord3.xy);

    float4 result = float4(input.color0.rgb * texture0.rgb,
        input.color0.a + texture0.a);
    result.rgb += texture2.rgb * LegacyPixel[0].rgb;
    result.a *= texture3.a;
    return result;
}
