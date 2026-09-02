// Common ABI for the GenMD engine shaders.
//
// The first six pixel constant registers are the normal WW3D material state.
// LegacyPixel then begins at c0 from the engine shader API, which keeps the
// existing Set_Pixel_Shader_Constant contract usable by the Slang shaders.

Texture2D StageTexture0 : register(t0);
Texture2D StageTexture1 : register(t1);
Texture2D StageTexture2 : register(t2);
Texture2D StageTexture3 : register(t3);

SamplerState StageSampler0 : register(s0);
SamplerState StageSampler1 : register(s1);
SamplerState StageSampler2 : register(s2);
SamplerState StageSampler3 : register(s3);

struct EnginePixelInput
{
    float4 position : SV_POSITION;
    float4 color0 : COLOR0;
    float4 color1 : COLOR1;
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
    float3 world_position : TEXCOORD11;
};

cbuffer PixelConstants : register(b1)
{
    float4 MaterialDiffuse;
    float4 MaterialAmbient;
    float4 MaterialEmissive;
    float4 MaterialSpecular;
    float4 FogColor;
    float4 TextureFactor;
    float4 LegacyPixel[8];
    float4 PixelState0;
    uint4 PixelState1;
    uint4 MaterialSourceState;
    uint4 StageEnabled[8];
    uint4 StageColorOperation[8];
    uint4 StageAlphaOperation[8];
    uint4 StageCoordinateState[8];
    uint4 StageColorArgument[8];
    uint4 StageAlphaArgument[8];
    uint4 StageColorModifier[8];
    uint4 StageAlphaModifier[8];
    row_major float4x4 TextureTransform[8];
    float4 BumpMatrix[8];
    float4 BumpParams[8];
    float4 MaterialPower;
    float4 SceneAmbient;
    float4 LightDiffuse[4];
    float4 LightSpecular[4];
    float4 LightAmbient[4];
    float4 LightPosition[4];
    float4 LightDirection[4];
    float4 LightAttenuation[4];
    float4 LightSpot[4];
    uint4 LightEnabled;
};

// The legacy texbem operation applies the signed bump value through the
// stage's bump environment matrix and scale/offset. The same state is
// uploaded by the active backend for the translated water and motion-blur
// shaders.
float2 LegacyTexBemUV(float2 base_uv, float4 bump_sample,
    float4 bump_matrix, float4 bump_params)
{
    float2 centered = bump_sample.rg - float2(0.5, 0.5);
    float2 delta = float2(dot(centered, bump_matrix.xy),
        dot(centered, bump_matrix.zw));
    return base_uv + delta * bump_params.x + bump_params.y;
}

// Reproduce the neutral WW3D texture-stage coordinate contract for the
// engine-owned Slang shaders. The flat terrain path uses UV0 for the terrain
// image and camera-space coordinates for the shroud/noise images; both paths
// are selected by the backend's stage state and texture transform.
float4 SelectEngineUV(uint index, EnginePixelInput input)
{
    switch (index)
    {
    case 1: return input.texcoord1;
    case 2: return input.texcoord2;
    case 3: return input.texcoord3;
    default: return input.texcoord0;
    }
}

float4 EngineStageCoordinate(uint stage, EnginePixelInput input)
{
    uint4 state = StageCoordinateState[stage];
    float4 coordinate;
    if (state.x == 1u)
    {
        coordinate = float4(input.camera_position, 1.0);
    }
    else if (state.x == 2u)
    {
        coordinate = float4(input.camera_normal, 0.0);
    }
    else if (state.x == 3u)
    {
        coordinate = float4(input.reflection, 0.0);
    }
    else
    {
        coordinate = SelectEngineUV(state.y, input);
    }
    if (state.z != 0u)
    {
        coordinate = mul(TextureTransform[stage], coordinate);
        if (state.z == 3u)
        {
            coordinate.xyz /= max(abs(coordinate.w), 0.000001);
        }
    }
    return coordinate;
}
