// Resources/Engine/Shaders/ShadowMap.hlsl

cbuffer ShadowCB : register(b0)
{
	float4x4 world;
	float4x4 view;
	float4x4 proj;
	float4x4 boneTransforms[200];
	int hasAnimation;
	float3 padding;
};

struct VS_IN
{
	float3 pos		: POSITION;
	float3 normal	: NORMAL0;
	float2 uv		: TEXCOORD0;
	float4 color	: COLOR0;
	float4 weight	: WEIGHT0;
	uint4  index	: INDEX0;
};

struct VS_OUT
{
	float4 pos : SV_POSITION;
};

VS_OUT VS(VS_IN vin)
{
	VS_OUT vout;
	float4 localPos = float4(vin.pos, 1.0f);

	// スキニング処理 (アニメーション対応)
	if (hasAnimation)
	{
		float wSum = dot(vin.weight, float4(1,1,1,1));
		if (wSum < 0.001f) { vin.weight = float4(1, 0, 0, 0); wSum = 1.0f; }
		float4 w = vin.weight / wSum;

		float4x4 skinMatrix = 
			boneTransforms[vin.index.x] * w.x +
			boneTransforms[vin.index.y] * w.y +
			boneTransforms[vin.index.z] * w.z +
			boneTransforms[vin.index.w] * w.w;

		localPos = mul(localPos, skinMatrix);
	}

	// 座標変換 (World -> View -> Proj)
	// ここでのView/Projは「ライトの視点」になります
	vout.pos = mul(localPos, world);
	vout.pos = mul(vout.pos, view);
	vout.pos = mul(vout.pos, proj);

	return vout;
}

// ピクセルシェーダーは空でOK (深度のみ書き込むため)
void PS(VS_OUT pin)
{
}