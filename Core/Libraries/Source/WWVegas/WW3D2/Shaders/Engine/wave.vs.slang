// Compiled form of the GenMD wave vertex program. LegacyVertex[n] mirrors
// the engine's Set_Vertex_Shader_Constant register ABI.

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
};

cbuffer VertexConstants : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 View;
    row_major float4x4 Projection;
    float4 Viewport;
    float4 LegacyVertex[96];
};

[shader("vertex")] VSOutput main(VSInput input)
{
    VSOutput output;
    float4 position = float4(input.position, 1.0);
    float4 clip_position = float4(
        dot(position, LegacyVertex[2]),
        dot(position, LegacyVertex[3]),
        dot(position, LegacyVertex[4]),
        dot(position, LegacyVertex[5]));
    float reciprocal_w = 1.0 / clip_position.w;

    output.position = clip_position;
    output.color0 = input.color0;
    output.color1 = float4(0.0, 0.0, 0.0, 1.0);
    output.texcoord0 = float4(input.texcoord0, 0.0, 0.0);
    output.texcoord1 = float4(
        clip_position.x * reciprocal_w * LegacyVertex[6].x + LegacyVertex[6].z,
        clip_position.y * reciprocal_w * LegacyVertex[6].y + LegacyVertex[6].w,
        0.0, 0.0);
    return output;
}
