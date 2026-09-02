#include "WW3D2ShaderCommon.slang"

[shader("pixel")] float4 main(EnginePixelInput input) : SV_TARGET
{
    float4 texture0 = StageTexture0.Sample(StageSampler0, input.texcoord0.xy);
    float4 texture1 = StageTexture1.Sample(StageSampler1, input.texcoord1.xy);
    float4 texture2 = StageTexture2.Sample(StageSampler2, input.texcoord2.xy);
    float4 texture3 = StageTexture3.Sample(StageSampler3, input.texcoord3.xy);

    float4 result = input.color0 * texture0;
    result.rgb = texture1.rgb * texture2.rgb + result.rgb;
    result.rgb *= texture3.rgb;
    return result;
}
