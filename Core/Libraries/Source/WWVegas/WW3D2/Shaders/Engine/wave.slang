#include "WW3D2ShaderCommon.slang"

[shader("pixel")] float4 main(EnginePixelInput input) : SV_TARGET
{
    float4 bump = StageTexture0.Sample(StageSampler0, input.texcoord0.xy);
    float2 reflection_uv = LegacyTexBemUV(input.texcoord1.xy, bump,
        BumpMatrix[1], BumpParams[1]);
    float4 reflection = StageTexture1.Sample(StageSampler1, reflection_uv);
    return reflection * input.color0;
}
