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
    float3 world_position : TEXCOORD1;
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

StructuredBuffer<GPUInstanceData> Instances : register(t127);

struct GPULightData
{
    float4 position_range;
    float4 direction_intensity;
    float4 color_inner_angle;
    float outer_angle;
    uint type;
    uint flags;
    uint shadow_data_index;
};

StructuredBuffer<GPULightData> Lights : register(t126);

cbuffer MaterialParameters : register(b0)
{
    float4 Values[4];
};

BasicOpaqueVertexOutput basic_opaque_vertex(BasicOpaqueVertexInput input, uint instance_id : SV_InstanceID)
{
    BasicOpaqueVertexOutput output;
    const float4 world_position = mul(float4(input.position, 1.0), Instances[instance_id].transform);
    output.position = world_position;
    output.color = input.color;
    output.world_position = world_position.xyz;
    return output;
}

float4 basic_opaque_pixel(BasicOpaqueVertexOutput input) : SV_TARGET
{
    float3 light_color = 1.0.xxx;
    if (Values[1].x > 0.5)
    {
        const GPULightData light = Lights[0];
        const float distance_to_light = length(input.world_position - light.position_range.xyz);
        const float attenuation = saturate(1.0 - distance_to_light / max(light.position_range.w, 0.0001));
        const float enabled = (light.flags & 1) != 0 ? 1.0 : 0.0;
        light_color = light.color_inner_angle.rgb * light.direction_intensity.w * (0.2 + 0.8 * attenuation) * enabled;
    }
    return float4(input.color.rgb * Values[0].rgb * light_color, input.color.a * Values[0].a);
}
