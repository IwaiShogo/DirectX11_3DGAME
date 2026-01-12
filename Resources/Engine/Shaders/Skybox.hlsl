// Resources/Engine/Shaders/Skybox.hlsl

cbuffer ConstantBuffer : register(b0)
{
	matrix View;
	matrix Projection;
	float3 CameraPos;
	float UseTexture; // 1.0 = Texture, 0.0 = Procedural
	
	// プロシージャル用カラー
	float4 ColorTop;
	float4 ColorHorizon;
	float4 ColorBottom;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float3 LocalPos : TEXCOORD0;
};

TextureCube SkyboxTexture : register(t0);
SamplerState Sampler : register(s0);

static const float3 kCubeVertices[36] = 
{
	// Front
	float3(-1,	1,	1), float3( 1,	1,	1), float3(-1, -1,	1),
	float3(-1, -1,	1), float3( 1,	1,	1), float3( 1, -1,	1),
	// Right
	float3( 1,	1,	1), float3( 1,	1, -1), float3( 1, -1,	1),
	float3( 1, -1,	1), float3( 1,	1, -1), float3( 1, -1, -1),
	// Back
	float3( 1,	1, -1), float3(-1,	1, -1), float3( 1, -1, -1),
	float3( 1, -1, -1), float3(-1,	1, -1), float3(-1, -1, -1),
	// Left
	float3(-1,	1, -1), float3(-1,	1,	1), float3(-1, -1, -1),
	float3(-1, -1, -1), float3(-1,	1,	1), float3(-1, -1,	1),
	// Top
	float3(-1,	1, -1), float3( 1,	1, -1), float3(-1,	1,	1),
	float3(-1,	1,	1), float3( 1,	1, -1), float3( 1,	1,	1),
	// Bottom
	float3(-1, -1,	1), float3( 1, -1,	1), float3(-1, -1, -1),
	float3(-1, -1, -1), float3( 1, -1,	1), float3( 1, -1, -1)
};

VS_OUTPUT mainVS(uint id : SV_VertexID)
{
	VS_OUTPUT output;
	float3 pos = kCubeVertices[id];
	output.LocalPos = pos;

	// 平行移動成分を無視したView行列を作成
	matrix viewNoTrans = View;
	viewNoTrans._41 = 0; viewNoTrans._42 = 0; viewNoTrans._43 = 0;

	// Z=W (深度1.0) になるように射影
	float4 clipPos = mul(float4(pos, 1.0), mul(viewNoTrans, Projection));
	output.Pos = clipPos.xyww;
	
	return output;
}

float4 mainPS(VS_OUTPUT input) : SV_Target
{
	// テクスチャモード
	if (UseTexture > 0.5f)
	{
		float3 texDir = float3(input.LocalPos.x, -input.LocalPos.z, input.LocalPos.y);
		
		return SkyboxTexture.Sample(Sampler, texDir);
	}
	
	// プロシージャルモード (グラデーション)
	float3 dir = normalize(input.LocalPos);
	float p = dir.y; // -1.0(下) ~ 1.0(上)

	float4 color;
	
	if (p > 0.0f)
	{
		// 空側 (Horizon -> Top)
		float t = pow(abs(p), 0.6f); // 指数で調整
		color = lerp(ColorHorizon, ColorTop, t);
	}
	else
	{
		// 地面側 (Horizon -> Bottom)
		float t = pow(abs(p), 0.6f);
		color = lerp(ColorHorizon, ColorBottom, t);
	}

	return color;
}