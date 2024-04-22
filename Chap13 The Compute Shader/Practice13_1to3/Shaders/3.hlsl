StructuredBuffer<float3> gInput : register(t0);
AppendStructuredBuffer<float> gOutput : register(u0);

[numthreads(64, 1, 1)]
void CS(int3 dtid : SV_DispatchThreadID)
{
	float l = length(gInput[dtid.x]);
	gOutput.Append(l);
}