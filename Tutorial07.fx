//--------------------------------------------------------------------------------------
// File: Tutorial07.fx
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

uint4 packDepth(float depth)
{
	uint uiDepth = asuint(depth);
	return uint4(uiDepth & 255, (uiDepth >> 8) & 255, (uiDepth >> 16) & 255, (uiDepth >> 24));
}

float unpackDepth(uint4 packedDepth)
{
	uint uiDepth = packedDepth.x | (packedDepth.y << 8) | (packedDepth.z << 16) | (packedDepth.w << 24);
	return asfloat(uiDepth);
}

//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------

cbuffer cbShared : register( b0 )
{
	matrix World;
	matrix View;
	matrix Projection;

	float4 StereoParamsArray[3];
};


//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float4 Pos : POSITION;
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

struct GS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD0;
	uint rtIndex : SV_RenderTargetArrayIndex;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS( VS_INPUT input )
{
    PS_INPUT output = (PS_INPUT)0;
    output.Pos = mul( input.Pos, World );
    output.Pos = mul( output.Pos, View );
    output.Pos = mul( output.Pos, Projection );
    output.Tex = input.Tex;
    
    return output;
}

float4 GetStereoPos(float4 pos, float4 stereoParams)
{
	float4 spos = pos;
	spos.x += stereoParams[0] * (spos.w - stereoParams[1]);
	return spos;
}

[instance(3)]
[maxvertexcount(3)]
void GS(triangle PS_INPUT In[3], inout TriangleStream<GS_OUTPUT> TriStream, uint gsInstanceId : SV_GSInstanceID)
{
	GS_OUTPUT output;
	output.rtIndex = gsInstanceId;
	[unroll] for (int v = 0; v < 3; v++)
	{
		output.Pos = GetStereoPos(In[v].Pos, StereoParamsArray[gsInstanceId]);
		output.Tex = In[v].Tex;
		TriStream.Append(output);
	}
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

//[earlydepthstencil]
uint4 PS(PS_INPUT input, uint rtIndex : SV_RenderTargetArrayIndex) : SV_Target
{
	if (rtIndex == 2)
	{
		return packDepth(input.Pos.w);
	}
	return uint4(128, 128, 128, 255);
}


struct QuadVS_Output {
	float4 pos : SV_POSITION;
};

QuadVS_Output QuadVS(uint vIdx : SV_VertexID)
{
	QuadVS_Output output;
	float2 tex = float2(vIdx % 2, vIdx % 4 / 2);
	output.pos = float4((tex.x - 0.5f) * 2, -(tex.y - 0.5f) * 2, 0, 1);
	return output;
}

Texture2D<uint4> PackedDepthSRV : register(t0);

float4 QuadPS(QuadVS_Output input) : SV_Target
{
	float depth = unpackDepth(PackedDepthSRV.Load(int3(input.pos.xy, 0))) * 0.1f;
	return float4(depth, 0.0f, depth, 1.0f);
}

Texture2DMS<uint4> PackedDepthSRV_MS : register(t0);

float4 MSQuadPS(QuadVS_Output input) : SV_Target
{
	float depth = unpackDepth(PackedDepthSRV_MS.Load(int3(input.pos.xy, 0), 0)) * 0.1f;
	return float4(depth, 0.0f, depth, 1.0f);
}
