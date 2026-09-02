// Fixed-function texture-stage behavior expressed as programmable Slang.
// The backend uploads this state through PixelConstants; no legacy state machine
// or fixed-function shader is involved.

#ifndef WW3D2_TEXTURE0_TYPE
#define WW3D2_TEXTURE0_TYPE 0
#endif
#ifndef WW3D2_TEXTURE1_TYPE
#define WW3D2_TEXTURE1_TYPE 0
#endif
#ifndef WW3D2_TEXTURE2_TYPE
#define WW3D2_TEXTURE2_TYPE 0
#endif
#ifndef WW3D2_TEXTURE3_TYPE
#define WW3D2_TEXTURE3_TYPE 0
#endif
#ifndef WW3D2_TEXTURE4_TYPE
#define WW3D2_TEXTURE4_TYPE 0
#endif
#ifndef WW3D2_TEXTURE5_TYPE
#define WW3D2_TEXTURE5_TYPE 0
#endif
#ifndef WW3D2_TEXTURE6_TYPE
#define WW3D2_TEXTURE6_TYPE 0
#endif
#ifndef WW3D2_TEXTURE7_TYPE
#define WW3D2_TEXTURE7_TYPE 0
#endif

#if WW3D2_TEXTURE0_TYPE == 1
TextureCube StageTexture0 : register(t0);
#elif WW3D2_TEXTURE0_TYPE == 2
Texture3D StageTexture0 : register(t0);
#else
Texture2D StageTexture0 : register(t0);
#endif
#if WW3D2_TEXTURE1_TYPE == 1
TextureCube StageTexture1 : register(t1);
#elif WW3D2_TEXTURE1_TYPE == 2
Texture3D StageTexture1 : register(t1);
#else
Texture2D StageTexture1 : register(t1);
#endif
#if WW3D2_TEXTURE2_TYPE == 1
TextureCube StageTexture2 : register(t2);
#elif WW3D2_TEXTURE2_TYPE == 2
Texture3D StageTexture2 : register(t2);
#else
Texture2D StageTexture2 : register(t2);
#endif
#if WW3D2_TEXTURE3_TYPE == 1
TextureCube StageTexture3 : register(t3);
#elif WW3D2_TEXTURE3_TYPE == 2
Texture3D StageTexture3 : register(t3);
#else
Texture2D StageTexture3 : register(t3);
#endif
#if WW3D2_TEXTURE4_TYPE == 1
TextureCube StageTexture4 : register(t4);
#elif WW3D2_TEXTURE4_TYPE == 2
Texture3D StageTexture4 : register(t4);
#else
Texture2D StageTexture4 : register(t4);
#endif
#if WW3D2_TEXTURE5_TYPE == 1
TextureCube StageTexture5 : register(t5);
#elif WW3D2_TEXTURE5_TYPE == 2
Texture3D StageTexture5 : register(t5);
#else
Texture2D StageTexture5 : register(t5);
#endif
#if WW3D2_TEXTURE6_TYPE == 1
TextureCube StageTexture6 : register(t6);
#elif WW3D2_TEXTURE6_TYPE == 2
Texture3D StageTexture6 : register(t6);
#else
Texture2D StageTexture6 : register(t6);
#endif
#if WW3D2_TEXTURE7_TYPE == 1
TextureCube StageTexture7 : register(t7);
#elif WW3D2_TEXTURE7_TYPE == 2
Texture3D StageTexture7 : register(t7);
#else
Texture2D StageTexture7 : register(t7);
#endif
SamplerState StageSampler0 : register(s0);
SamplerState StageSampler1 : register(s1);
SamplerState StageSampler2 : register(s2);
SamplerState StageSampler3 : register(s3);
SamplerState StageSampler4 : register(s4);
SamplerState StageSampler5 : register(s5);
SamplerState StageSampler6 : register(s6);
SamplerState StageSampler7 : register(s7);

#if WW3D2_TEXTURE0_TYPE == 1
float4 SampleStage0(float4 coordinate) { return StageTexture0.Sample(StageSampler0, coordinate.xyz); }
#elif WW3D2_TEXTURE0_TYPE == 2
float4 SampleStage0(float4 coordinate) { return StageTexture0.Sample(StageSampler0, coordinate.xyz); }
#else
float4 SampleStage0(float4 coordinate) { return StageTexture0.Sample(StageSampler0, coordinate.xy); }
#endif
#if WW3D2_TEXTURE1_TYPE == 1
float4 SampleStage1(float4 coordinate) { return StageTexture1.Sample(StageSampler1, coordinate.xyz); }
#elif WW3D2_TEXTURE1_TYPE == 2
float4 SampleStage1(float4 coordinate) { return StageTexture1.Sample(StageSampler1, coordinate.xyz); }
#else
float4 SampleStage1(float4 coordinate) { return StageTexture1.Sample(StageSampler1, coordinate.xy); }
#endif
#if WW3D2_TEXTURE2_TYPE == 1
float4 SampleStage2(float4 coordinate) { return StageTexture2.Sample(StageSampler2, coordinate.xyz); }
#elif WW3D2_TEXTURE2_TYPE == 2
float4 SampleStage2(float4 coordinate) { return StageTexture2.Sample(StageSampler2, coordinate.xyz); }
#else
float4 SampleStage2(float4 coordinate) { return StageTexture2.Sample(StageSampler2, coordinate.xy); }
#endif
#if WW3D2_TEXTURE3_TYPE == 1
float4 SampleStage3(float4 coordinate) { return StageTexture3.Sample(StageSampler3, coordinate.xyz); }
#elif WW3D2_TEXTURE3_TYPE == 2
float4 SampleStage3(float4 coordinate) { return StageTexture3.Sample(StageSampler3, coordinate.xyz); }
#else
float4 SampleStage3(float4 coordinate) { return StageTexture3.Sample(StageSampler3, coordinate.xy); }
#endif
#if WW3D2_TEXTURE4_TYPE == 1
float4 SampleStage4(float4 coordinate) { return StageTexture4.Sample(StageSampler4, coordinate.xyz); }
#elif WW3D2_TEXTURE4_TYPE == 2
float4 SampleStage4(float4 coordinate) { return StageTexture4.Sample(StageSampler4, coordinate.xyz); }
#else
float4 SampleStage4(float4 coordinate) { return StageTexture4.Sample(StageSampler4, coordinate.xy); }
#endif
#if WW3D2_TEXTURE5_TYPE == 1
float4 SampleStage5(float4 coordinate) { return StageTexture5.Sample(StageSampler5, coordinate.xyz); }
#elif WW3D2_TEXTURE5_TYPE == 2
float4 SampleStage5(float4 coordinate) { return StageTexture5.Sample(StageSampler5, coordinate.xyz); }
#else
float4 SampleStage5(float4 coordinate) { return StageTexture5.Sample(StageSampler5, coordinate.xy); }
#endif
#if WW3D2_TEXTURE6_TYPE == 1
float4 SampleStage6(float4 coordinate) { return StageTexture6.Sample(StageSampler6, coordinate.xyz); }
#elif WW3D2_TEXTURE6_TYPE == 2
float4 SampleStage6(float4 coordinate) { return StageTexture6.Sample(StageSampler6, coordinate.xyz); }
#else
float4 SampleStage6(float4 coordinate) { return StageTexture6.Sample(StageSampler6, coordinate.xy); }
#endif
#if WW3D2_TEXTURE7_TYPE == 1
float4 SampleStage7(float4 coordinate) { return StageTexture7.Sample(StageSampler7, coordinate.xyz); }
#elif WW3D2_TEXTURE7_TYPE == 2
float4 SampleStage7(float4 coordinate) { return StageTexture7.Sample(StageSampler7, coordinate.xyz); }
#else
float4 SampleStage7(float4 coordinate) { return StageTexture7.Sample(StageSampler7, coordinate.xy); }
#endif

struct PSInput
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

float4 SelectColor(uint argument, float4 current, float4 diffuse, float4 texture,
    float4 factor, uint modifier)
{
    float4 value;
    switch (argument)
    {
    case 0: value = current; break;
    case 1: value = diffuse; break;
    case 2: value = texture; break;
    case 3: value = factor; break;
    case 4: value = current.aaaa; break;
    case 5: value = diffuse.aaaa; break;
    case 6: value = texture.aaaa; break;
    case 7: value = factor.aaaa; break;
    default: value = current; break;
    }
    if ((modifier & 2u) != 0) value = value.aaaa;
    if ((modifier & 1u) != 0) value = 1.0 - value;
    return value;
}

float SelectAlpha(uint argument, float current, float diffuse, float texture,
    float factor, uint modifier)
{
    float value;
    switch (argument)
    {
    case 0: value = current; break;
    case 1: value = diffuse; break;
    case 2: value = texture; break;
    case 3: value = factor; break;
    case 4: value = current; break;
    case 5: value = diffuse; break;
    case 6: value = texture; break;
    case 7: value = factor; break;
    default: value = current; break;
    }
    if ((modifier & 1u) != 0) value = 1.0 - value;
    return value;
}

float4 ApplyColor(float4 current, float4 texture, float4 diffuse, float4 factor,
    uint operation, uint4 arguments, uint4 modifiers)
{
    float4 a0 = SelectColor(arguments.x, current, diffuse, texture, factor, modifiers.x);
    float4 a1 = SelectColor(arguments.y, current, diffuse, texture, factor, modifiers.y);
    float4 a2 = SelectColor(arguments.z, current, diffuse, texture, factor, modifiers.z);
    switch (operation)
    {
    case 1: return a1;
    case 2: return a2;
    case 3: return a1 * a2;
    case 4: return a1 + a2 - a1 * a2;
    case 5: return a1 + a2;
    case 6: return a1 - a2;
    case 7: return a1 * texture.a + a2 * (1.0 - texture.a);
    case 8: return a1 * current.a + a2 * (1.0 - current.a);
    case 9: return a1 + a2 - 0.5;
    case 10: return (a1 + a2 - 0.5) * 2.0;
    case 11: return a1 * a2 * 2.0;
    case 12: return a1 * a2.a + a2;
    case 13:
    case 14: return current;
    case 15:
    {
        float value = dot(a1.rgb * 2.0 - 1.0, a2.rgb * 2.0 - 1.0);
        return float4(value, value, value, value);
    }
    case 16: return a0 * a1 + a2;
    default: return current;
    }
}

float ApplyAlpha(float current, float4 current_color, float4 texture, float4 diffuse,
    float4 factor, uint operation, uint4 arguments, uint4 modifiers)
{
    float a0 = SelectAlpha(arguments.x, current, diffuse.a, texture.a, factor.a, modifiers.x);
    float a1 = SelectAlpha(arguments.y, current, diffuse.a, texture.a, factor.a, modifiers.y);
    float a2 = SelectAlpha(arguments.z, current, diffuse.a, texture.a, factor.a, modifiers.z);
    switch (operation)
    {
    case 1: return a1;
    case 2: return a2;
    case 3: return a1 * a2;
    case 4: return a1 + a2 - a1 * a2;
    case 5: return a1 + a2;
    case 6: return a1 - a2;
    case 7: return a1 * texture.a + a2 * (1.0 - texture.a);
    case 8: return a1 * current + a2 * (1.0 - current);
    case 9: return a1 + a2 - 0.5;
    case 10: return (a1 + a2 - 0.5) * 2.0;
    case 11: return a1 * a2 * 2.0;
    case 12: return a1 * a2 + a2;
    case 13:
    case 14: return current;
    case 15: return dot(current_color.rgb * 2.0 - 1.0, texture.rgb * 2.0 - 1.0);
    case 16: return a0 * a1 + a2;
    default: return current;
    }
}

float4 SelectUV(uint index, PSInput input)
{
    switch (index)
    {
    case 1: return input.texcoord1;
    case 2: return input.texcoord2;
    case 3: return input.texcoord3;
    case 4: return input.texcoord4;
    case 5: return input.texcoord5;
    case 6: return input.texcoord6;
    case 7: return input.texcoord7;
    default: return input.texcoord0;
    }
}

float4 StageCoordinate(uint stage, PSInput input)
{
    uint4 state = StageCoordinateState[stage];
    float4 coordinate;
    if (state.x == 1) coordinate = float4(input.camera_position, 1.0);
    else if (state.x == 2) coordinate = float4(input.camera_normal, 0.0);
    else if (state.x == 3) coordinate = float4(input.reflection, 0.0);
    else coordinate = SelectUV(state.y, input);
    if (state.z != 0)
    {
        coordinate = mul(TextureTransform[stage], coordinate);
        if (state.z == 3) coordinate.xyz /= max(abs(coordinate.w), 0.000001);
    }
    return coordinate;
}

float2 BumpOffset(uint stage, float4 texture)
{
    float2 signed_value = texture.rg * 2.0 - 1.0;
    float2 offset = float2(dot(signed_value, BumpMatrix[stage].xy),
        dot(signed_value, BumpMatrix[stage].zw));
    return offset * BumpParams[stage].x + float2(BumpParams[stage].y, BumpParams[stage].y);
}

float4 SelectMaterialColor(uint source, float4 material, float4 color1, float4 color2)
{
    if (source == 1u) return color1;
    if (source == 2u) return color2;
    return material;
}

[shader("pixel")] float4 main(PSInput input) : SV_TARGET
{
    float4 diffuse = input.color0 * MaterialDiffuse;
    if (PixelState1.z != 0u)
    {
        float4 diffuse_material = SelectMaterialColor(MaterialSourceState.y,
            MaterialDiffuse, input.color0, input.color1);
        float4 ambient_material = SelectMaterialColor(MaterialSourceState.x,
            MaterialAmbient, input.color0, input.color1);
        float4 emissive_material = SelectMaterialColor(MaterialSourceState.z,
            MaterialEmissive, input.color0, input.color1);
        float3 normal = normalize(input.camera_normal);
        float3 view_direction = normalize(-input.camera_position);
        float3 light_accumulation = SceneAmbient.rgb * ambient_material.rgb + emissive_material.rgb;
        float3 specular_accumulation = float3(0.0, 0.0, 0.0);

        if (LightEnabled.x != 0u)
        {
            float3 to_light;
            float attenuation = 1.0;
            float light_type = LightPosition[0].w;
            if (light_type == 3.0)
            {
                to_light = normalize(-LightDirection[0].xyz);
            }
            else
            {
                float3 light_delta = LightPosition[0].xyz - input.camera_position;
                float distance_to_light = length(light_delta);
                to_light = distance_to_light > 0.0001 ?
                    light_delta / distance_to_light : float3(0.0, 0.0, 1.0);
                if (LightAttenuation[0].w > 0.0 &&
                    distance_to_light > LightAttenuation[0].w) attenuation = 0.0;
                float denominator = LightAttenuation[0].x +
                    LightAttenuation[0].y * distance_to_light +
                    LightAttenuation[0].z * distance_to_light * distance_to_light;
                attenuation *= 1.0 / max(denominator, 0.0001);
                if (light_type == 2.0)
                {
                    float spot_cosine = dot(-to_light, normalize(LightDirection[0].xyz));
                    float inner_cosine = cos(0.5 * LightSpot[0].y);
                    float outer_cosine = cos(0.5 * LightSpot[0].z);
                    float spot = saturate((spot_cosine - outer_cosine) /
                        max(inner_cosine - outer_cosine, 0.0001));
                    attenuation *= pow(spot, max(LightSpot[0].x, 0.0));
                }
            }
            float diffuse_factor = saturate(dot(normal, to_light));
            light_accumulation += LightAmbient[0].rgb * ambient_material.rgb * attenuation;
            light_accumulation += LightDiffuse[0].rgb * diffuse_material.rgb *
                diffuse_factor * attenuation;
            if (PixelState1.w != 0u && diffuse_factor > 0.0)
            {
                float3 half_vector = normalize(to_light + view_direction);
                float specular_factor = pow(saturate(dot(normal, half_vector)),
                    max(MaterialPower.x, 1.0));
                specular_accumulation += LightSpecular[0].rgb * MaterialSpecular.rgb *
                    specular_factor * attenuation;
            }
        }
        if (LightEnabled.y != 0u)
        {
            float3 to_light;
            float attenuation = 1.0;
            float light_type = LightPosition[1].w;
            if (light_type == 3.0) to_light = normalize(-LightDirection[1].xyz);
            else
            {
                float3 light_delta = LightPosition[1].xyz - input.camera_position;
                float distance_to_light = length(light_delta);
                to_light = distance_to_light > 0.0001 ? light_delta / distance_to_light : float3(0.0, 0.0, 1.0);
                if (LightAttenuation[1].w > 0.0 && distance_to_light > LightAttenuation[1].w) attenuation = 0.0;
                float denominator = LightAttenuation[1].x + LightAttenuation[1].y * distance_to_light + LightAttenuation[1].z * distance_to_light * distance_to_light;
                attenuation *= 1.0 / max(denominator, 0.0001);
                if (light_type == 2.0)
                {
                    float spot_cosine = dot(-to_light, normalize(LightDirection[1].xyz));
                    float inner_cosine = cos(0.5 * LightSpot[1].y);
                    float outer_cosine = cos(0.5 * LightSpot[1].z);
                    float spot = saturate((spot_cosine - outer_cosine) / max(inner_cosine - outer_cosine, 0.0001));
                    attenuation *= pow(spot, max(LightSpot[1].x, 0.0));
                }
            }
            float diffuse_factor = saturate(dot(normal, to_light));
            light_accumulation += LightAmbient[1].rgb * ambient_material.rgb * attenuation;
            light_accumulation += LightDiffuse[1].rgb * diffuse_material.rgb * diffuse_factor * attenuation;
            if (PixelState1.w != 0u && diffuse_factor > 0.0)
            {
                float3 half_vector = normalize(to_light + view_direction);
                float specular_factor = pow(saturate(dot(normal, half_vector)), max(MaterialPower.x, 1.0));
                specular_accumulation += LightSpecular[1].rgb * MaterialSpecular.rgb * specular_factor * attenuation;
            }
        }
        if (LightEnabled.z != 0u)
        {
            float3 to_light;
            float attenuation = 1.0;
            float light_type = LightPosition[2].w;
            if (light_type == 3.0) to_light = normalize(-LightDirection[2].xyz);
            else
            {
                float3 light_delta = LightPosition[2].xyz - input.camera_position;
                float distance_to_light = length(light_delta);
                to_light = distance_to_light > 0.0001 ? light_delta / distance_to_light : float3(0.0, 0.0, 1.0);
                if (LightAttenuation[2].w > 0.0 && distance_to_light > LightAttenuation[2].w) attenuation = 0.0;
                float denominator = LightAttenuation[2].x + LightAttenuation[2].y * distance_to_light + LightAttenuation[2].z * distance_to_light * distance_to_light;
                attenuation *= 1.0 / max(denominator, 0.0001);
                if (light_type == 2.0)
                {
                    float spot_cosine = dot(-to_light, normalize(LightDirection[2].xyz));
                    float inner_cosine = cos(0.5 * LightSpot[2].y);
                    float outer_cosine = cos(0.5 * LightSpot[2].z);
                    float spot = saturate((spot_cosine - outer_cosine) / max(inner_cosine - outer_cosine, 0.0001));
                    attenuation *= pow(spot, max(LightSpot[2].x, 0.0));
                }
            }
            float diffuse_factor = saturate(dot(normal, to_light));
            light_accumulation += LightAmbient[2].rgb * ambient_material.rgb * attenuation;
            light_accumulation += LightDiffuse[2].rgb * diffuse_material.rgb * diffuse_factor * attenuation;
            if (PixelState1.w != 0u && diffuse_factor > 0.0)
            {
                float3 half_vector = normalize(to_light + view_direction);
                float specular_factor = pow(saturate(dot(normal, half_vector)), max(MaterialPower.x, 1.0));
                specular_accumulation += LightSpecular[2].rgb * MaterialSpecular.rgb * specular_factor * attenuation;
            }
        }
        if (LightEnabled.w != 0u)
        {
            float3 to_light;
            float attenuation = 1.0;
            float light_type = LightPosition[3].w;
            if (light_type == 3.0) to_light = normalize(-LightDirection[3].xyz);
            else
            {
                float3 light_delta = LightPosition[3].xyz - input.camera_position;
                float distance_to_light = length(light_delta);
                to_light = distance_to_light > 0.0001 ? light_delta / distance_to_light : float3(0.0, 0.0, 1.0);
                if (LightAttenuation[3].w > 0.0 && distance_to_light > LightAttenuation[3].w) attenuation = 0.0;
                float denominator = LightAttenuation[3].x + LightAttenuation[3].y * distance_to_light + LightAttenuation[3].z * distance_to_light * distance_to_light;
                attenuation *= 1.0 / max(denominator, 0.0001);
                if (light_type == 2.0)
                {
                    float spot_cosine = dot(-to_light, normalize(LightDirection[3].xyz));
                    float inner_cosine = cos(0.5 * LightSpot[3].y);
                    float outer_cosine = cos(0.5 * LightSpot[3].z);
                    float spot = saturate((spot_cosine - outer_cosine) / max(inner_cosine - outer_cosine, 0.0001));
                    attenuation *= pow(spot, max(LightSpot[3].x, 0.0));
                }
            }
            float diffuse_factor = saturate(dot(normal, to_light));
            light_accumulation += LightAmbient[3].rgb * ambient_material.rgb * attenuation;
            light_accumulation += LightDiffuse[3].rgb * diffuse_material.rgb * diffuse_factor * attenuation;
            if (PixelState1.w != 0u && diffuse_factor > 0.0)
            {
                float3 half_vector = normalize(to_light + view_direction);
                float specular_factor = pow(saturate(dot(normal, half_vector)), max(MaterialPower.x, 1.0));
                specular_accumulation += LightSpecular[3].rgb * MaterialSpecular.rgb * specular_factor * attenuation;
            }
        }
        diffuse.rgb = saturate(light_accumulation);
        diffuse.a = diffuse_material.a;
        if (PixelState1.w != 0u) diffuse.rgb = saturate(diffuse.rgb + specular_accumulation);
    }

    float4 current = diffuse;
    float2 bump_offset = float2(0.0, 0.0);
    if (StageEnabled[0].x != 0u)
    {
        float4 coordinate = StageCoordinate(0u, input); coordinate.xy += bump_offset;
        float4 texture = SampleStage0(coordinate);
        float4 previous = current;
        current = ApplyColor(previous, texture, diffuse, TextureFactor,
            StageColorOperation[0].x, StageColorArgument[0], StageColorModifier[0]);
        current.a = ApplyAlpha(previous.a, previous, texture, diffuse, TextureFactor,
            StageAlphaOperation[0].x, StageAlphaArgument[0], StageAlphaModifier[0]);
        if (StageColorOperation[0].x == 13u || StageColorOperation[0].x == 14u)
            bump_offset += BumpOffset(0u, texture);
    }
    if (StageEnabled[1].x != 0u)
    {
        float4 coordinate = StageCoordinate(1u, input); coordinate.xy += bump_offset;
        float4 texture = SampleStage1(coordinate);
        float4 previous = current;
        current = ApplyColor(previous, texture, diffuse, TextureFactor,
            StageColorOperation[1].x, StageColorArgument[1], StageColorModifier[1]);
        current.a = ApplyAlpha(previous.a, previous, texture, diffuse, TextureFactor,
            StageAlphaOperation[1].x, StageAlphaArgument[1], StageAlphaModifier[1]);
        if (StageColorOperation[1].x == 13u || StageColorOperation[1].x == 14u)
            bump_offset += BumpOffset(1u, texture);
    }
    if (StageEnabled[2].x != 0u)
    {
        float4 coordinate = StageCoordinate(2u, input); coordinate.xy += bump_offset;
        float4 texture = SampleStage2(coordinate);
        float4 previous = current;
        current = ApplyColor(previous, texture, diffuse, TextureFactor,
            StageColorOperation[2].x, StageColorArgument[2], StageColorModifier[2]);
        current.a = ApplyAlpha(previous.a, previous, texture, diffuse, TextureFactor,
            StageAlphaOperation[2].x, StageAlphaArgument[2], StageAlphaModifier[2]);
        if (StageColorOperation[2].x == 13u || StageColorOperation[2].x == 14u)
            bump_offset += BumpOffset(2u, texture);
    }
    if (StageEnabled[3].x != 0u)
    {
        float4 coordinate = StageCoordinate(3u, input); coordinate.xy += bump_offset;
        float4 texture = SampleStage3(coordinate);
        float4 previous = current;
        current = ApplyColor(previous, texture, diffuse, TextureFactor,
            StageColorOperation[3].x, StageColorArgument[3], StageColorModifier[3]);
        current.a = ApplyAlpha(previous.a, previous, texture, diffuse, TextureFactor,
            StageAlphaOperation[3].x, StageAlphaArgument[3], StageAlphaModifier[3]);
        if (StageColorOperation[3].x == 13u || StageColorOperation[3].x == 14u)
            bump_offset += BumpOffset(3u, texture);
    }
    if (StageEnabled[4].x != 0u)
    {
        float4 coordinate = StageCoordinate(4u, input); coordinate.xy += bump_offset;
        float4 texture = SampleStage4(coordinate);
        float4 previous = current;
        current = ApplyColor(previous, texture, diffuse, TextureFactor,
            StageColorOperation[4].x, StageColorArgument[4], StageColorModifier[4]);
        current.a = ApplyAlpha(previous.a, previous, texture, diffuse, TextureFactor,
            StageAlphaOperation[4].x, StageAlphaArgument[4], StageAlphaModifier[4]);
        if (StageColorOperation[4].x == 13u || StageColorOperation[4].x == 14u)
            bump_offset += BumpOffset(4u, texture);
    }
    if (StageEnabled[5].x != 0u)
    {
        float4 coordinate = StageCoordinate(5u, input); coordinate.xy += bump_offset;
        float4 texture = SampleStage5(coordinate);
        float4 previous = current;
        current = ApplyColor(previous, texture, diffuse, TextureFactor,
            StageColorOperation[5].x, StageColorArgument[5], StageColorModifier[5]);
        current.a = ApplyAlpha(previous.a, previous, texture, diffuse, TextureFactor,
            StageAlphaOperation[5].x, StageAlphaArgument[5], StageAlphaModifier[5]);
        if (StageColorOperation[5].x == 13u || StageColorOperation[5].x == 14u)
            bump_offset += BumpOffset(5u, texture);
    }
    if (StageEnabled[6].x != 0u)
    {
        float4 coordinate = StageCoordinate(6u, input); coordinate.xy += bump_offset;
        float4 texture = SampleStage6(coordinate);
        float4 previous = current;
        current = ApplyColor(previous, texture, diffuse, TextureFactor,
            StageColorOperation[6].x, StageColorArgument[6], StageColorModifier[6]);
        current.a = ApplyAlpha(previous.a, previous, texture, diffuse, TextureFactor,
            StageAlphaOperation[6].x, StageAlphaArgument[6], StageAlphaModifier[6]);
        if (StageColorOperation[6].x == 13u || StageColorOperation[6].x == 14u)
            bump_offset += BumpOffset(6u, texture);
    }
    if (StageEnabled[7].x != 0u)
    {
        float4 coordinate = StageCoordinate(7u, input); coordinate.xy += bump_offset;
        float4 texture = SampleStage7(coordinate);
        float4 previous = current;
        current = ApplyColor(previous, texture, diffuse, TextureFactor,
            StageColorOperation[7].x, StageColorArgument[7], StageColorModifier[7]);
        current.a = ApplyAlpha(previous.a, previous, texture, diffuse, TextureFactor,
            StageAlphaOperation[7].x, StageAlphaArgument[7], StageAlphaModifier[7]);
    }

    if (PixelState0.w != 0.0)
    {
        bool pass = true;
        float reference = PixelState1.y / 255.0;
        switch (PixelState1.x)
        {
        case 1: pass = current.a < reference; break;
        case 2: pass = abs(current.a - reference) < 0.0001; break;
        case 3: pass = current.a <= reference; break;
        case 4: pass = current.a > reference; break;
        case 5: pass = abs(current.a - reference) >= 0.0001; break;
        case 6: pass = current.a >= reference; break;
        case 7: pass = true; break;
        default: pass = false; break;
        }
        if (!pass) discard;
    }
    if (PixelState0.z != 0.0)
    {
        float distance = abs(input.camera_position.z);
        float fog = saturate((PixelState0.y - distance) /
            max(0.0001, PixelState0.y - PixelState0.x));
        current.rgb = lerp(FogColor.rgb, current.rgb, fog);
    }
    return current;
}
