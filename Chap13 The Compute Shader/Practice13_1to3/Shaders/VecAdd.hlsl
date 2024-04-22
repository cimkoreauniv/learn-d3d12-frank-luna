
struct Data
{
	float3 v1;
	float2 v2;
};

StructuredBuffer<Data> gInputA : register(t0);
StructuredBuffer<Data> gInputB : register(t1);
AppendStructuredBuffer<Data> gOutput;


[numthreads(32, 1, 1)]
void CS(int3 dtid : SV_DispatchThreadID)
{
	Data d;
	d.v1 = gInputA[dtid.x].v1 + gInputB[dtid.x].v1;
	d.v2 = gInputA[dtid.x].v2 + gInputB[dtid.x].v2;
	gOutput.Append(d);
	//gOutput[dtid.x] = d;
}
