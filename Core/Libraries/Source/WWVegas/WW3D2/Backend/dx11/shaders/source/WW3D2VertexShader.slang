// The dynamic WW3D2 vertex layout: XYZ + normal + packed diffuse + two UV pairs.
// This is compiled to DXBC by the checkout-provided Slang compiler in CMake.

struct VSInput
{
    float3 position : POSITION0;
    float3 normal : NORMAL0;
    float4 color0 : COLOR0;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color0 : COLOR0;
    float4 color1 : COLOR1;
    float3 normal : NORMAL0;
    float4 texcoord0 : TEXCOORD0;
    float4 texcoord1 : TEXCOORD1;
    float4 texcoord2 : TEXCOORD2;
    float4 texcoord3 : TEXCOORD3;
    float4 texcoord4 : TEXCOORD4;
    float4 texcoord5 : TEXCOORD5;
    float4 texcoord6 : TEXCOORD6;
    float4 texcoord7 : TEXCOORD7;
    float3 camera_position : TEXCOORD8;
    float3 camera_normal : TEXCOORD9;
    float3 reflection : TEXCOORD10;
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
    output.position = mul(Projection, mul(View, mul(World, float4(input.position, 1.0))));
    output.color0 = input.color0;
    output.color1 = float4(0.0, 0.0, 0.0, 1.0);
    output.normal = input.normal;
    output.texcoord0 = float4(input.texcoord0, 0.0, 0.0);
    output.texcoord1 = float4(input.texcoord1, 0.0, 0.0);
    output.texcoord2 = float4(0.0, 0.0, 0.0, 0.0);
    output.texcoord3 = float4(0.0, 0.0, 0.0, 0.0);
    output.texcoord4 = float4(0.0, 0.0, 0.0, 0.0);
    output.texcoord5 = float4(0.0, 0.0, 0.0, 0.0);
    output.texcoord6 = float4(0.0, 0.0, 0.0, 0.0);
    output.texcoord7 = float4(0.0, 0.0, 0.0, 0.0);
    float4 camera_position = mul(View, mul(World, float4(input.position, 1.0)));
    float3 camera_normal = normalize(mul(View, mul(World, float4(input.normal, 0.0))).xyz);
    output.camera_position = camera_position.xyz;
    output.camera_normal = camera_normal;
    output.reflection = normalize(reflect(-normalize(camera_position.xyz), camera_normal));
    return output;
}
