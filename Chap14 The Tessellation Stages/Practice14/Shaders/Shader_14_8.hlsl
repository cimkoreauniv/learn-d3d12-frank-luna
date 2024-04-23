//***************************************************************************************
// BezierTessellation.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Demonstrates hardware tessellating a Bezier patch.
//***************************************************************************************

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

 // Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D    gDiffuseMap : register(t0);


SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

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
	float4 gAmbientLight;

	float4 gFogColor;
	float gFogStart;
	float gFogRange;
	float2 cbPerObjectPad2;

	// Indices [0, NUM_DIR_LIGHTS) are directional lights;
	// indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
	// indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
	// are spot lights for a maximum of MaxLights per object.
	Light gLights[MaxLights];
};

cbuffer cbMaterial : register(b2)
{
	float4   gDiffuseAlbedo;
	float3   gFresnelR0;
	float    gRoughness;
	float4x4 gMatTransform;
};

struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float3 PosL : POSITION;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	
	vout.PosL = vin.PosL;

	return vout;
}
 
struct PatchTess
{
	float EdgeTess[4]   : SV_TessFactor;
	float InsideTess[2] : SV_InsideTessFactor;
};

PatchTess ConstantHS(InputPatch<VertexOut, 16> patch, uint patchID : SV_PrimitiveID)
{
	PatchTess pt;
	
	// Uniform tessellation for this demo.

	pt.EdgeTess[0] = 25;
	pt.EdgeTess[1] = 25;
	pt.EdgeTess[2] = 25;
	pt.EdgeTess[3] = 25;
	
	pt.InsideTess[0] = 25;
	pt.InsideTess[1] = 25;
	
	return pt;
}

struct HullOut
{
	float3 PosL : POSITION;
};

// This Hull Shader part is commonly used for a coordinate basis change, 
// for example changing from a quad to a Bezier bi-cubic.
[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(16)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexOut, 16> p, 
           uint i : SV_OutputControlPointID,
           uint patchId : SV_PrimitiveID)
{
	HullOut hout;
	
	hout.PosL = p[i].PosL;
	
	return hout;
}

struct DomainOut
{
	float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

float4 BernsteinBasis(float t)
{
    float invT = 1.0f - t;

    return float4( invT * invT * invT,
                   3.0f * t * invT * invT,
                   3.0f * t * t * invT,
                   t * t * t );
}

float3 CubicBezierSum(const OutputPatch<HullOut, 16> bezpatch, float4 basisU, float4 basisV)
{
    float3 sum = float3(0.0f, 0.0f, 0.0f);
    sum  = basisV.x * (basisU.x*bezpatch[0].PosL  + basisU.y*bezpatch[1].PosL  + basisU.z*bezpatch[2].PosL  + basisU.w*bezpatch[3].PosL );
    sum += basisV.y * (basisU.x*bezpatch[4].PosL  + basisU.y*bezpatch[5].PosL  + basisU.z*bezpatch[6].PosL  + basisU.w*bezpatch[7].PosL );
    sum += basisV.z * (basisU.x*bezpatch[8].PosL  + basisU.y*bezpatch[9].PosL  + basisU.z*bezpatch[10].PosL + basisU.w*bezpatch[11].PosL);
    sum += basisV.w * (basisU.x*bezpatch[12].PosL + basisU.y*bezpatch[13].PosL + basisU.z*bezpatch[14].PosL + basisU.w*bezpatch[15].PosL);

    return sum;
}

float4 dBernsteinBasis(float t)
{
    float invT = 1.0f - t;

    return float4( -3 * invT * invT,
                   3 * invT * invT - 6 * t * invT,
                   6 * t * invT - 3 * t * t,
                   3 * t * t );
}

// The domain shader is called for every vertex created by the tessellator.  
// It is like the vertex shader after tessellation.
[domain("quad")]
DomainOut DS(PatchTess patchTess, 
             float2 uv : SV_DomainLocation, 
             const OutputPatch<HullOut, 16> bezPatch)
{
	DomainOut dout;
	
	float4 basisU = BernsteinBasis(uv.x);
	float4 basisV = BernsteinBasis(uv.y);

	float3 p  = CubicBezierSum(bezPatch, basisU, basisV);
	
	float4 posW = mul(float4(p, 1.0f), gWorld);
    dout.PosW = posW.xyz;
	dout.PosH = mul(posW, gViewProj);
    dout.TexC = uv;
	
    float4 dBasisU = dBernsteinBasis(uv.x);
    float4 dBasisV = dBernsteinBasis(uv.y);
	
    float3 du = basisV.x * (dBasisU.x * bezPatch[0].PosL + dBasisU.y * bezPatch[1].PosL + dBasisU.z * bezPatch[2].PosL + dBasisU.w * bezPatch[3].PosL);
    du += basisV.y * (dBasisU.x * bezPatch[4].PosL + dBasisU.y * bezPatch[5].PosL + dBasisU.z * bezPatch[6].PosL + dBasisU.w * bezPatch[7].PosL);
    du += basisV.z * (dBasisU.x * bezPatch[8].PosL + dBasisU.y * bezPatch[9].PosL + dBasisU.z * bezPatch[10].PosL + dBasisU.w * bezPatch[11].PosL);
    du += basisV.w * (dBasisU.x * bezPatch[12].PosL + dBasisU.y * bezPatch[13].PosL + dBasisU.z * bezPatch[14].PosL + dBasisU.w * bezPatch[15].PosL);
    
    float3 dv = dBasisV.x * (basisU.x * bezPatch[0].PosL + basisU.y * bezPatch[1].PosL + basisU.z * bezPatch[2].PosL + basisU.w * bezPatch[3].PosL);
    dv += dBasisV.y * (basisU.x * bezPatch[4].PosL + basisU.y * bezPatch[5].PosL + basisU.z * bezPatch[6].PosL + basisU.w * bezPatch[7].PosL);
    dv += dBasisV.z * (basisU.x * bezPatch[8].PosL + basisU.y * bezPatch[9].PosL + basisU.z * bezPatch[10].PosL + basisU.w * bezPatch[11].PosL);
    dv += dBasisV.w * (basisU.x * bezPatch[12].PosL + basisU.y * bezPatch[13].PosL + basisU.z * bezPatch[14].PosL + basisU.w * bezPatch[15].PosL);
	
    dout.NormalW = cross(du, dv);
	
	return dout;
}

float4 PS(DomainOut pin) : SV_Target
{
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;

#ifdef ALPHA_TEST
    clip(diffuseAlbedo.a - 0.1f);
#endif

    // Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);

    //Reuse length when computing fog.
    float3 toEyeW = gEyePosW - pin.PosW;
    float distToEye = length(toEyeW);
    toEyeW /= distToEye;

	// Indirect lighting.
    float4 ambient = gAmbientLight * diffuseAlbedo;

    const float shininess = 1.0f - gRoughness;
    Material mat = { diffuseAlbedo, gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

#ifdef FOG
    float fogAmount = saturate((distToEye - gFogStart) / gFogRange);
    litColor = lerp(litColor, gFogColor, fogAmount);
#endif

    // Common convention to take alpha from diffuse material.
    litColor.a = gDiffuseAlbedo.a;

    return litColor;
}
