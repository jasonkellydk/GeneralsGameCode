struct BasicOpaqueVertexInput
{
    float3 position : POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

struct BasicOpaqueVertexOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

struct GPUInstanceData
{
    row_major float4x4 transform;
    float4 bounds;
    uint mesh_index;
    uint material_index;
    uint flags;
    uint reserved;
};

StructuredBuffer<GPUInstanceData> Instances : register(t1);

cbuffer MaterialParameters : register(b0)
{
    float4 Values[4];
};

BasicOpaqueVertexOutput basic_opaque_vertex(BasicOpaqueVertexInput input, uint instance_id : SV_InstanceID)
{
    BasicOpaqueVertexOutput output;
    output.position = mul(float4(input.position, 1.0), Instances[instance_id].transform);
    output.color = input.color;
    return output;
}

float4 basic_opaque_pixel(BasicOpaqueVertexOutput input) : SV_TARGET
{
    return input.color * Values[0];
}
