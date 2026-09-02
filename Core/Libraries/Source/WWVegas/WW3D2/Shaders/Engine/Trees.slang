// Explicit pixel contract for the batched GenMD tree/grass renderer.

Texture2D TreeTexture : register(t0);
Texture2D TreeShroudTexture : register(t1);

SamplerState TreeTextureSampler : register(s0);
SamplerState TreeShroudSampler : register(s1);

struct TreePixelInput
{
    float4 position : SV_POSITION;
    float4 color0 : COLOR0;
    float2 diffuse_uv : TEXCOORD0;
    float2 shroud_uv : TEXCOORD1;
};

cbuffer TreePixelConstants : register(b1)
{
    // Preserve the backend's shared material header so the explicit tree
    // constants begin at the same c0 offset used by every programmable pass.
    float4 MaterialDiffuse;
    float4 MaterialAmbient;
    float4 MaterialEmissive;
    float4 MaterialSpecular;
    float4 FogColor;
    float4 TextureFactor;
    // x: shroud enabled, y: alpha cutoff, z: diffuse overbright multiplier.
    float4 TreeOptions;
};

[shader("pixel")] float4 TreePixelMain(TreePixelInput input) : SV_TARGET
{
    float4 tree = TreeTexture.Sample(TreeTextureSampler, input.diffuse_uv);
    if (tree.a < TreeOptions.y)
    {
        discard;
    }

    float3 visibility = float3(1.0, 1.0, 1.0);
    if (TreeOptions.x > 0.5)
    {
        visibility = TreeShroudTexture.Sample(TreeShroudSampler,
            input.shroud_uv).rgb;
    }

    return float4(input.color0.rgb * tree.rgb * visibility * TreeOptions.z,
        tree.a);
}
