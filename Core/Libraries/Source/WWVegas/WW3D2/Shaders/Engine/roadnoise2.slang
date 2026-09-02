#include "WW3D2ShaderCommon.slang"

[shader("pixel")] float4 main(EnginePixelInput input) : SV_TARGET
{
    float4 road = StageTexture0.Sample(StageSampler0, input.texcoord0.xy);
    float4 cloud = StageTexture1.Sample(StageSampler1, input.texcoord1.xy);
    float4 noise = StageTexture2.Sample(StageSampler2, input.texcoord2.xy);
    return road * cloud * noise * input.color0;
}
