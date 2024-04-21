//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Default shader, currently supports lighting.
//***************************************************************************************

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

// Constant data that varies per frame.

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

cbuffer cbMaterial : register(b1)
{
	float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float  gRoughness;
	float4x4 gMatTransform;
};

// Constant data that varies per material.
cbuffer cbPass : register(b2)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};
 
struct VertexIn
{
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
};

struct VertexOut
{
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
};

struct GeoOut
{
    float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;
	
    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

    // Transform to homogeneous clip space.
    //vout.PosH = mul(posW, gViewProj);

    return vout;
}

[maxvertexcount(48)]
void GS(triangle VertexOut gin[3],
    inout TriangleStream<GeoOut> triStream)
{
    float d = length(gEyePosW);
    if (d >= 30.0f)
    {
        GeoOut gout;
        [unroll]
        for (int i = 0; i < 3; ++i)
        {
            gout.PosH = mul(float4(gin[i].PosW, 1.0f), gViewProj);
            gout.PosW = gin[i].PosW;
            gout.NormalW = gin[i].NormalW;

            triStream.Append(gout);
        }
    }
    else
    {
        float3 v[12];
        float3 m0 = normalize((gin[0].PosW + gin[1].PosW) / 2);
        float3 m1 = normalize((gin[0].PosW + gin[2].PosW) / 2);
        float3 m2 = normalize((gin[1].PosW + gin[2].PosW) / 2);
        v[0] = gin[0].PosW; v[1] = m0; v[2] = m1;
        v[3] = gin[1].PosW; v[4] = m2; v[5] = m0;
        v[6] = m0; v[7] = m2; v[8] = m1;
        v[9] = gin[2].PosW; v[10] = m1; v[11] = m2;
        if (d >= 15)
        {
            GeoOut gout;
            for (int i = 0; i < 12; i++)
            {
                gout.PosW = v[i];
                gout.PosH = mul(float4(gout.PosW, 1.0f), gViewProj);
                gout.NormalW = gout.PosW;
                triStream.Append(gout);
                if (i % 3 == 2)
                    triStream.RestartStrip();
            }
        }
        else
        {
            float3 v2[48];
            for (int i = 0; i < 4; i++)
            {
                float3 m0 = normalize((v[3 * i + 0] + v[3 * i + 1]) / 2);
                float3 m1 = normalize((v[3 * i + 0] + v[3 * i + 2]) / 2);
                float3 m2 = normalize((v[3 * i + 1] + v[3 * i + 2]) / 2);
                v2[12 * i + 0] = v[3 * i + 0]; v2[12 * i + 1] = m0; v2[12 * i + 2] = m1;
                v2[12 * i + 3] = v[3 * i + 1]; v2[12 * i + 4] = m2; v2[12 * i + 5] = m0;
                v2[12 * i + 6] = m0; v2[12 * i + 7] = m2; v2[12 * i + 8] = m1;
                v2[12 * i + 9] = v[3 * i + 2]; v2[12 * i + 10] = m1; v2[12 * i + 11] = m2;
            }

            GeoOut gout;
            for (int i = 0; i < 48; i++)
            {
                gout.PosW = v2[i];
                gout.PosH = mul(float4(gout.PosW, 1.0f), gViewProj);
                gout.NormalW = gout.PosW;
                triStream.Append(gout);
                if (i % 3 == 2)
                    triStream.RestartStrip();
            }
        }
    }
}

float4 PS(GeoOut pin) : SV_Target
{
    // Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);

    // Vector from point being lit to eye. 
    float3 toEyeW = normalize(gEyePosW - pin.PosW);

	// Indirect lighting.
    float4 ambient = gAmbientLight*gDiffuseAlbedo;

    const float shininess = 1.0f - gRoughness;
    Material mat = { gDiffuseAlbedo, gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

    // Common convention to take alpha from diffuse material.
    litColor.a = gDiffuseAlbedo.a;

    return litColor;
}


