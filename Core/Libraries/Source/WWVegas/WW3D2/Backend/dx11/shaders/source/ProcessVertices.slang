ByteAddressBuffer SourceVertices : register(t0);
RWByteAddressBuffer DestinationVertices : register(u0);

cbuffer ProcessVerticesConstants : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 View;
    row_major float4x4 Projection;
    float4 Viewport;
    uint SourceStride;
    uint DestinationStride;
    uint SourceStart;
    uint VertexCount;
    uint SourceLocationOffset;
    uint DestinationLocationOffset;
    uint SourceDiffuseOffset;
    uint DestinationDiffuseOffset;
    uint SourceTextureCount;
    uint DestinationTextureCount;
    uint SourceTextureOffsets[8];
    uint DestinationTextureOffsets[8];
    uint SourceTextureDimensions[8];
    uint DestinationTextureDimensions[8];
};

float LoadSourceFloat(uint address)
{
    return asfloat(SourceVertices.Load(address));
}

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    const uint vertex_index = dispatch_id.x;
    if (vertex_index >= VertexCount)
    {
        return;
    }

    const uint source_base = SourceStart + vertex_index * SourceStride;
    const uint destination_base = vertex_index * DestinationStride;

    for (uint byte_offset = 0; byte_offset < DestinationStride; byte_offset += 4)
    {
        DestinationVertices.Store(destination_base + byte_offset, 0u);
    }

    const float3 source_position = float3(
        LoadSourceFloat(source_base + SourceLocationOffset),
        LoadSourceFloat(source_base + SourceLocationOffset + 4u),
        LoadSourceFloat(source_base + SourceLocationOffset + 8u));
    const float4 clip_position = mul(Projection,
        mul(View, mul(World, float4(source_position, 1.0))));
    const float inverse_w = abs(clip_position.w) > 0.000001 ?
        1.0 / clip_position.w : 1.0;
    const float2 ndc = clip_position.xy * inverse_w;
    const float4 transformed_position = float4(
        Viewport.x + (ndc.x + 1.0) * Viewport.z * 0.5,
        Viewport.y + (1.0 - ndc.y) * Viewport.w * 0.5,
        clip_position.z * inverse_w,
        inverse_w);

    DestinationVertices.Store(destination_base + DestinationLocationOffset,
        asuint(transformed_position.x));
    DestinationVertices.Store(destination_base + DestinationLocationOffset + 4u,
        asuint(transformed_position.y));
    DestinationVertices.Store(destination_base + DestinationLocationOffset + 8u,
        asuint(transformed_position.z));
    DestinationVertices.Store(destination_base + DestinationLocationOffset + 12u,
        asuint(transformed_position.w));

    if (SourceDiffuseOffset != 0xffffffffu)
    {
        if (DestinationDiffuseOffset != 0xffffffffu)
        {
            DestinationVertices.Store(destination_base + DestinationDiffuseOffset,
                SourceVertices.Load(source_base + SourceDiffuseOffset));
        }
    }

    for (uint texture_index = 0; texture_index < DestinationTextureCount &&
        texture_index < 8u; ++texture_index)
    {
        const uint destination_dimension = min(4u,
            max(1u, DestinationTextureDimensions[texture_index]));
        const uint source_dimension = texture_index < SourceTextureCount ?
            min(4u, max(1u, SourceTextureDimensions[texture_index])) : 0u;
        for (uint component = 0; component < destination_dimension; ++component)
        {
            if (component < source_dimension)
            {
                const uint source_address = source_base +
                    SourceTextureOffsets[texture_index] + component * 4u;
                const uint destination_address = destination_base +
                    DestinationTextureOffsets[texture_index] + component * 4u;
                DestinationVertices.Store(destination_address,
                    SourceVertices.Load(source_address));
            }
        }
    }
}
