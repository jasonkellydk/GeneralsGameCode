// Compiled form of the GenMD tree/grass vertex program.

struct VSInput
{
    float3 position : POSITION0;
    float3 normal : NORMAL0;
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
    float4 normal = float4(input.normal, 1.0);
    float4 position_from_base = float4(input.position, 1.0) -
        float4(0.0, 0.0, input.normal.z, 0.0);
    uint sway_index = (uint)max(input.normal.x, 0.0);
    float4 sway = LegacyVertex[8 + sway_index];
    float4 moved_position = position_from_base.zzzw * sway +
        float4(input.position, 1.0);
    output.position = float4(
        dot(moved_position, LegacyVertex[4]),
        dot(moved_position, LegacyVertex[5]),
        dot(moved_position, LegacyVertex[6]),
        dot(moved_position, LegacyVertex[7]));
    output.color0 = input.color0 * float4(input.normal.y, input.normal.y,
        input.normal.y, 1.0);
    output.color1 = float4(0.0, 0.0, 0.0, 1.0);
    output.texcoord0 = float4(input.texcoord0, 0.0, 0.0);
    output.texcoord1 = (float4(input.position, 1.0) + LegacyVertex[32]) *
        LegacyVertex[33];
    return output;
}
