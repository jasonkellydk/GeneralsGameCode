#include "WW3D2ShaderCommon.slang"

[shader("pixel")] float4 main(EnginePixelInput input) : SV_TARGET
{
    float4 terrain = StageTexture0.Sample(StageSampler0, input.texcoord0.xy);
    float4 blend = StageTexture1.Sample(StageSampler1, input.texcoord1.xy);
    // The legacy shader samples t2 after the terrain path switches stage 2
    // to camera-space coordinates and supplies a texture transform.  Terrain
    // vertices only contain UV0/UV1, so texcoord2 is not a valid substitute.
    float4 noise = StageTexture2.Sample(StageSampler2,
        EngineStageCoordinate(2, input).xy);
    float3 color = lerp(terrain, blend, input.color0.a).rgb * noise.rgb;
    return float4(color, 1.0);
}
