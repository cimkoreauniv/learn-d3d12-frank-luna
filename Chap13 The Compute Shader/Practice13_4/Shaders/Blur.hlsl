cbuffer cbSettings : register(b0)
{
	int gBlurRadius;

	float w0;
	float w1;
	float w2;
	float w3;
	float w4;
	float w5;
	float w6;
	float w7;
	float w8;
	float w9;
	float w10;
};

static const int gMaxBlurRadius = 5;

Texture2D gInput			: register(t0);
RWTexture2D<float4> gOutput	: register(u0);

#define N 256
groupshared float4 gCache[N + 2 * gMaxBlurRadius];

[numthreads(N, 1, 1)]
void HorzBlurCS(int3 dispatchThreadID : SV_DispatchThreadID,
	int3 groupThreadID : SV_GroupThreadID)
{
	float weights[11] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10 };
	if (groupThreadID.x < gBlurRadius)
	{
		int x = max(dispatchThreadID.x - gBlurRadius, 0);
		gCache[groupThreadID.x] = gInput[int2(x, dispatchThreadID.y)];
	}
	if (groupThreadID.x >= N - gBlurRadius)
	{
		int x = min(dispatchThreadID.x + gBlurRadius, gInput.Length.x - 1);
		gCache[groupThreadID.x + 2 * gBlurRadius] = gInput[int2(x, dispatchThreadID.y)];
	}

	gCache[groupThreadID.x + gBlurRadius] = gInput[min(dispatchThreadID.xy, gInput.Length.xy - 1)];

	GroupMemoryBarrierWithGroupSync();

	float4 result = float4(0, 0, 0, 0);
	for (int i = -gBlurRadius; i <= gBlurRadius; i++)
		result += weights[i + gBlurRadius] * gCache[i + gBlurRadius + groupThreadID.x];

	gOutput[dispatchThreadID.xy] = result;
}

[numthreads(1, N, 1)]
void VertBlurCS(int3 dispatchThreadID : SV_DispatchThreadID,
	int3 groupThreadID : SV_GroupThreadID)
{
	float weights[11] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10 };
	if (groupThreadID.y < gBlurRadius)
	{
		int y = max(dispatchThreadID.y - gBlurRadius, 0);
		gCache[groupThreadID.y] = gInput[int2(dispatchThreadID.x, y)];
	}
	if (groupThreadID.y >= N - gBlurRadius)
	{
		int y = min(dispatchThreadID.y + gBlurRadius, gInput.Length.y - 1);
		gCache[groupThreadID.y + 2 * gBlurRadius] = gInput[int2(dispatchThreadID.x, y)];
	}

	gCache[groupThreadID.y + gBlurRadius] = gInput[min(dispatchThreadID.xy, gInput.Length.xy - 1)];

	GroupMemoryBarrierWithGroupSync();

	float4 result = float4(0, 0, 0, 0);
	for (int i = -gBlurRadius; i <= gBlurRadius; i++)
		result += weights[i + gBlurRadius] * gCache[i + gBlurRadius + groupThreadID.y];

	gOutput[dispatchThreadID.xy] = result;
	//gOutput[dispatchThreadID.xy] = float4(w0 + 0.5f, w1 + 0.5f, w2 + 0.5f, 0);
}