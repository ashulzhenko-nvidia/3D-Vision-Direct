//--------------------------------------------------------------------------------------
// File: Tutorial07.fx
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------

cbuffer cbShared : register( b0 )
{
	matrix World;
	matrix View;
	matrix Projection;

	float4 StereoParams;
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
	uint viewport : SV_ViewportArrayIndex;
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

float4 MakeStereo(float4 pos, float eyeSign)
{
	float4 spos = pos;
	spos.x += eyeSign * StereoParams[0] * (spos.w - StereoParams[1]);
	return spos;
}

[maxvertexcount(6)]
void GS(triangle PS_INPUT In[3], inout TriangleStream<GS_OUTPUT> TriStream)
{
	GS_OUTPUT output;

	output.viewport = 0;
	[unroll] for (int v = 0; v < 3; v++)
	{
		output.Pos = MakeStereo(In[v].Pos, -1.0f);
		output.Tex = In[v].Tex;
		TriStream.Append(output);
	}
	TriStream.RestartStrip();

	output.viewport = 1;
	[unroll] for (int v = 0; v < 3; v++)
	{
		output.Pos = MakeStereo(In[v].Pos, +1.0f);
		output.Tex = In[v].Tex;
		TriStream.Append(output);
	}
	TriStream.RestartStrip();
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS( PS_INPUT input) : SV_Target
{
    return float4(0.5, 0.5, 0.5, 1);
}
