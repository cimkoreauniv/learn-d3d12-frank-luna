//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Default shader, currently supports lighting.
//***************************************************************************************

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 0
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D gDiffuseMap : register(t0);
SamplerState gsamLinear : register(s4);

// Constant data that varies per frame.

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
};

// Constant data that varies per material.
cbuffer cbPass : register(b1)
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
};

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float  gRoughness;
    float4x4 gMatTransform;
};

struct VertexIn
{
	float3 PosL    : POSITION;
};

struct VertexOut
{
	float3 PosL    : POSITION;
};

struct PatchTess
{
    float EdgeTess[3] : SV_TessFactor;
    float InsideTess : SV_InsideTessFactor;
};

struct HullOut
{
    float3 PosL : POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosL = vin.PosL;
    return vout;
}

PatchTess ConstantHS(InputPatch<VertexOut, 3> patch, uint patchID: SV_PrimitiveID)
{
    PatchTess pt;
    float3 centerL = 0.25f * (patch[0].PosL + patch[1].PosL + patch[2].PosL);
    
    float3 centerW = mul(float4(centerL, 1.0f), gWorld).xyz;
    
    float d = distance(centerW, gEyePosW);

    const float d0 = 20.0f;
    const float d1 = 100.0f;
    float tess = 64.0f * saturate((d1 - d) / (d1 - d0));
    
    [unroll]
    for (int i = 0; i < 3; i++)
        pt.EdgeTess[i] = tess;

    pt.InsideTess = tess;
    
    return pt;
}

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexOut, 3> p,
           uint i : SV_OutputControlPointID,
           uint patchId : SV_PrimitiveID)
{
    HullOut hout;
	
    hout.PosL = p[i].PosL;
	
    return hout;
}

struct DomainOut
{
    float4 PosW : POSITION;
    float4 PosH : SV_POSITION;
};

[domain("tri")]
DomainOut DS(PatchTess patchTess,
	         float3 uvw : SV_DomainLocation,
             const OutputPatch<HullOut, 3> quad)
{
    DomainOut dout;
	
    float3 p = quad[0].PosL * uvw.x + quad[1].PosL * uvw.y + quad[2].PosL * uvw.z;
	
	// Displacement mapping
    p.y = 0.3f * (p.z * sin(p.x) + p.x * cos(p.z));
	
    float4 posW = mul(float4(p, 1.0f), gWorld);
    dout.PosW = posW;
    dout.PosH = mul(posW, gViewProj);
	
    return dout;
}

float4 PS(DomainOut pin) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}


