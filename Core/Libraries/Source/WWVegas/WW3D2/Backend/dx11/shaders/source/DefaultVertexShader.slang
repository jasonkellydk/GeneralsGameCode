// Generic programmable WW3D2 vertex shader.
//
// CMake compiles this source once for every RenderBackendVertexFormat.  The
// resulting DXBC files are loaded by the active renderer; this source is never
// parsed or compiled by the game process.

#ifndef WW3D2_VERTEX_FORMAT
#define WW3D2_VERTEX_FORMAT 12
#endif

#if WW3D2_VERTEX_FORMAT == 1
struct VSInput { float3 position : POSITION0; };
#elif WW3D2_VERTEX_FORMAT == 2
struct VSInput { float3 position : POSITION0; float4 color0 : COLOR0; };
#elif WW3D2_VERTEX_FORMAT == 3
struct VSInput { float3 position : POSITION0; float2 texcoord0 : TEXCOORD0; };
#elif WW3D2_VERTEX_FORMAT == 4
struct VSInput { float3 position : POSITION0; float4 color0 : COLOR0; float2 texcoord0 : TEXCOORD0; };
#elif WW3D2_VERTEX_FORMAT == 5
struct VSInput { float3 position : POSITION0; float2 texcoord0 : TEXCOORD0; float2 texcoord1 : TEXCOORD1; };
#elif WW3D2_VERTEX_FORMAT == 6
struct VSInput { float3 position : POSITION0; float4 color0 : COLOR0; float2 texcoord0 : TEXCOORD0; float2 texcoord1 : TEXCOORD1; };
#elif WW3D2_VERTEX_FORMAT == 7
struct VSInput { float3 position : POSITION0; float3 normal : NORMAL0; };
#elif WW3D2_VERTEX_FORMAT == 8
struct VSInput { float3 position : POSITION0; float3 normal : NORMAL0; float4 color0 : COLOR0; };
#elif WW3D2_VERTEX_FORMAT == 9
struct VSInput { float3 position : POSITION0; float3 normal : NORMAL0; float2 texcoord0 : TEXCOORD0; };
#elif WW3D2_VERTEX_FORMAT == 10
struct VSInput { float3 position : POSITION0; float3 normal : NORMAL0; float4 color0 : COLOR0; float2 texcoord0 : TEXCOORD0; };
#elif WW3D2_VERTEX_FORMAT == 11
struct VSInput { float3 position : POSITION0; float3 normal : NORMAL0; float2 texcoord0 : TEXCOORD0; float2 texcoord1 : TEXCOORD1; };
#elif WW3D2_VERTEX_FORMAT == 12
struct VSInput { float3 position : POSITION0; float3 normal : NORMAL0; float4 color0 : COLOR0; float2 texcoord0 : TEXCOORD0; float2 texcoord1 : TEXCOORD1; };
#elif WW3D2_VERTEX_FORMAT == 13
struct VSInput { float4 position : POSITION0; };
#elif WW3D2_VERTEX_FORMAT == 14
struct VSInput { float4 position : POSITION0; float4 color0 : COLOR0; };
#elif WW3D2_VERTEX_FORMAT == 15
struct VSInput { float4 position : POSITION0; float2 texcoord0 : TEXCOORD0; };
#elif WW3D2_VERTEX_FORMAT == 16
struct VSInput { float4 position : POSITION0; float4 color0 : COLOR0; float2 texcoord0 : TEXCOORD0; };
#elif WW3D2_VERTEX_FORMAT == 17
struct VSInput { float4 position : POSITION0; float2 texcoord0 : TEXCOORD0; float2 texcoord1 : TEXCOORD1; };
#elif WW3D2_VERTEX_FORMAT == 18
struct VSInput { float4 position : POSITION0; float4 color0 : COLOR0; float2 texcoord0 : TEXCOORD0; float2 texcoord1 : TEXCOORD1; };
#elif WW3D2_VERTEX_FORMAT == 19
struct VSInput
{
    float3 position : POSITION0;
    float3 normal : NORMAL0;
    float4 color0 : COLOR0;
    float2 texcoord0 : TEXCOORD0;
    float3 texcoord1 : TEXCOORD1;
    float3 texcoord2 : TEXCOORD2;
    float3 texcoord3 : TEXCOORD3;
};
#elif WW3D2_VERTEX_FORMAT == 20
struct VSInput
{
    float3 position : POSITION0;
    float3 normal : NORMAL0;
    float1 texcoord0 : TEXCOORD0;
    float4 texcoord1 : TEXCOORD1;
    float2 texcoord2 : TEXCOORD2;
};
#elif WW3D2_VERTEX_FORMAT == 21
struct VSInput { float3 position : POSITION0; float3 normal : NORMAL0; float4 color0 : COLOR0; };
#else
#error "Unsupported WW3D2 vertex format"
#endif

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
    float4 object_position;
    float3 object_normal = float3(0.0, 0.0, 1.0);

#if WW3D2_VERTEX_FORMAT >= 13 && WW3D2_VERTEX_FORMAT <= 18
    object_position = input.position;
#else
    object_position = float4(input.position, 1.0);
#endif

#if WW3D2_VERTEX_FORMAT == 7 || WW3D2_VERTEX_FORMAT == 8 || WW3D2_VERTEX_FORMAT == 9 || WW3D2_VERTEX_FORMAT == 10 || WW3D2_VERTEX_FORMAT == 11 || WW3D2_VERTEX_FORMAT == 12 || WW3D2_VERTEX_FORMAT == 19 || WW3D2_VERTEX_FORMAT == 20 || WW3D2_VERTEX_FORMAT == 21
    object_normal = input.normal;
#endif

#if WW3D2_VERTEX_FORMAT >= 13 && WW3D2_VERTEX_FORMAT <= 18
    output.position = float4(
        (object_position.x - Viewport.x) / max(Viewport.z, 1.0) * 2.0 - 1.0,
        1.0 - (object_position.y - Viewport.y) / max(Viewport.w, 1.0) * 2.0,
        object_position.z, 1.0);
    output.camera_position = float3(0.0, 0.0, 0.0);
    output.camera_normal = float3(0.0, 0.0, 1.0);
    output.reflection = float3(0.0, 0.0, 1.0);
#else
    output.position = mul(Projection, mul(View, mul(World, object_position)));
    float4 camera_position = mul(View, mul(World, object_position));
    float3 camera_normal = normalize(mul(View, mul(World, float4(object_normal, 0.0))).xyz);
    output.camera_position = camera_position.xyz;
    output.camera_normal = camera_normal;
    output.reflection = normalize(reflect(-normalize(camera_position.xyz), camera_normal));
#endif

    output.color0 = float4(1.0, 1.0, 1.0, 1.0);
    output.color1 = float4(0.0, 0.0, 0.0, 1.0);
    output.normal = object_normal;
    output.texcoord0 = float4(0.0, 0.0, 0.0, 0.0);
    output.texcoord1 = float4(0.0, 0.0, 0.0, 0.0);
    output.texcoord2 = float4(0.0, 0.0, 0.0, 0.0);
    output.texcoord3 = float4(0.0, 0.0, 0.0, 0.0);
    output.texcoord4 = float4(0.0, 0.0, 0.0, 0.0);
    output.texcoord5 = float4(0.0, 0.0, 0.0, 0.0);
    output.texcoord6 = float4(0.0, 0.0, 0.0, 0.0);
    output.texcoord7 = float4(0.0, 0.0, 0.0, 0.0);

#if WW3D2_VERTEX_FORMAT == 2 || WW3D2_VERTEX_FORMAT == 4 || WW3D2_VERTEX_FORMAT == 6 || WW3D2_VERTEX_FORMAT == 8 || WW3D2_VERTEX_FORMAT == 10 || WW3D2_VERTEX_FORMAT == 12 || WW3D2_VERTEX_FORMAT == 14 || WW3D2_VERTEX_FORMAT == 16 || WW3D2_VERTEX_FORMAT == 18 || WW3D2_VERTEX_FORMAT == 19 || WW3D2_VERTEX_FORMAT == 21
    output.color0 = input.color0;
#endif

#if WW3D2_VERTEX_FORMAT == 3 || WW3D2_VERTEX_FORMAT == 4 || WW3D2_VERTEX_FORMAT == 5 || WW3D2_VERTEX_FORMAT == 6 || WW3D2_VERTEX_FORMAT == 9 || WW3D2_VERTEX_FORMAT == 10 || WW3D2_VERTEX_FORMAT == 11 || WW3D2_VERTEX_FORMAT == 12 || WW3D2_VERTEX_FORMAT == 15 || WW3D2_VERTEX_FORMAT == 16 || WW3D2_VERTEX_FORMAT == 17 || WW3D2_VERTEX_FORMAT == 18 || WW3D2_VERTEX_FORMAT == 19
    output.texcoord0 = float4(input.texcoord0, 0.0, 0.0);
#elif WW3D2_VERTEX_FORMAT == 20
    output.texcoord0 = float4(input.texcoord0.x, 0.0, 0.0, 0.0);
#endif

#if WW3D2_VERTEX_FORMAT == 5 || WW3D2_VERTEX_FORMAT == 6 || WW3D2_VERTEX_FORMAT == 11 || WW3D2_VERTEX_FORMAT == 12 || WW3D2_VERTEX_FORMAT == 17 || WW3D2_VERTEX_FORMAT == 18
    output.texcoord1 = float4(input.texcoord1, 0.0, 0.0);
#endif

#if WW3D2_VERTEX_FORMAT == 19
    output.texcoord1 = float4(input.texcoord1, 0.0);
    output.texcoord2 = float4(input.texcoord2, 0.0);
    output.texcoord3 = float4(input.texcoord3, 0.0);
#elif WW3D2_VERTEX_FORMAT == 20
    output.texcoord1 = input.texcoord1;
    output.texcoord2 = float4(input.texcoord2, 0.0, 0.0);
#endif

    return output;
}
