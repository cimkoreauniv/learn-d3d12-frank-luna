struct VertexIn
{
	float3 Pos : POSITION;
	float4 Color : COLOR;
};

struct VertexOut
{
	float4 PosH : SV_POSITION;
	float4 Color : COLOR;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	vout.PosH = float4(vin.Pos, 1.0f);
	
	vout.Color = vin.Color;

	return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
	//clip(pin.Color.r - 0.5f);
	return pin.Color;
}