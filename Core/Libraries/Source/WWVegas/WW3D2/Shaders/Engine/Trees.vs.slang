// Explicit vertex contract for the batched GenMD tree/grass renderer.

struct TreeVertexInput
{
    float3 position : POSITION0;
    // x: sway type (1..10), y: vertex darkening, z: tree base height.
    float3 normal : NORMAL0;
    float4 color0 : COLOR0;
    float2 texcoord0 : TEXCOORD0;
};

struct TreeVertexOutput
{
    float4 position : SV_POSITION;
    float4 color0 : COLOR0;
    float2 diffuse_uv : TEXCOORD0;
    float2 shroud_uv : TEXCOORD1;
};

cbuffer TreeVertexConstants : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 View;
    row_major float4x4 Projection;
    float4 Viewport;
    float4 TreeSway[10];
    float4 TreeShroudProjection;
};

[shader("vertex")] TreeVertexOutput TreeVertexMain(TreeVertexInput input)
{
    TreeVertexOutput output;

    // The tree batch stores the base height in normal.z. Keep the same sway
    // pivot as the original tree vertex program, but feed it through the
    // common World/View/Projection transform contract.
    float4 position_from_base = float4(input.position, 1.0) -
        float4(0.0, 0.0, input.normal.z, 0.0);
    uint sway_index = (uint)clamp(input.normal.x, 1.0, 10.0) - 1u;
    float4 moved_position = position_from_base.zzzw * TreeSway[sway_index] +
        float4(input.position, 1.0);
    float4 world_position = mul(World, moved_position);
    float4 camera_position = mul(View, world_position);

    output.position = mul(Projection, camera_position);
    output.color0 = input.color0 * float4(input.normal.y, input.normal.y,
        input.normal.y, 1.0);
    output.diffuse_uv = input.texcoord0;
    output.shroud_uv = world_position.xy * TreeShroudProjection.xy +
        TreeShroudProjection.zw;
    return output;
}
