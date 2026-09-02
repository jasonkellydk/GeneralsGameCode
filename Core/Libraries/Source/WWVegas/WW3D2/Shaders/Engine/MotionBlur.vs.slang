// Unused GenMD motion-blur vertex program, retained as a real Slang shader
// so every engine shader asset has a build-time DXBC representation.

struct VSInput
{
    float3 position : POSITION0;
    float4 color0 : COLOR0;
    float2 texcoord0 : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color0 : COLOR0;
    float4 color1 : COLOR1;
    float4 texcoord0 : TEXCOORD0;
    float4 texcoord1 : TEXCOORD1;
    float4 texcoord2 : TEXCOORD2;
    float4 texcoord3 : TEXCOORD3;
};

[shader("vertex")] VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0);
    output.color0 = input.color0;
    output.color1 = float4(0.0, 0.0, 0.0, 1.0);
    output.texcoord0 = float4(input.texcoord0, 0.0, 0.0);
    output.texcoord1 = output.texcoord0;
    output.texcoord2 = output.texcoord0;
    output.texcoord3 = output.texcoord0;
    return output;
}
