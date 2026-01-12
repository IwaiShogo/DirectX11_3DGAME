// =========================================================
// Standard.hlsl - 統合定数バッファ版
// =========================================================

// C++側の ModelRenderer::CBData 構造体と一致させる必要があります
cbuffer MainCB : register(b0)
{
	float4x4 world;			 // ワールド行列
	float4x4 view;			 // ビュー行列
	float4x4 proj;			 // プロジェクション行列
	float4x4 boneTransforms[200]; // ボーン行列
	float4 lightDir;		 // ライト方向
	float4 lightColor;		 // ライト色
	float4 materialColor;	 // マテリアル色
	int hasAnimation;		 // アニメーションフラグ (bool)
	float3 padding;			 // パディング
};

// --- 入力構造体 ---
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
	float4 pos		: SV_POSITION;
	float3 normal	: NORMAL0;
	float2 uv		: TEXCOORD0;
	float4 color	: COLOR0;
	float4 wPos		: POSITION0;
};

// --- テクスチャ・サンプラー ---
Texture2D tex : register(t0);
SamplerState samp : register(s0);

// =========================================================
// 頂点シェーダー
// =========================================================
VS_OUT VS(VS_IN vin)
{
	VS_OUT vout;

	float4 localPos = float4(vin.pos, 1.0f);
	float3 localNormal = vin.normal;

	if (hasAnimation)
	{
		// ウェイトの合計を計算
		float wSum = dot(vin.weight, float4(1,1,1,1));
		
		// 合計が0に近い（アニメなし、またはエラー）場合はスキニングしない、または補正
		if (wSum < 0.001f)
		{
			vin.weight = float4(1, 0, 0, 0);
			wSum = 1.0f;
		}
		
		float4 w = vin.weight / wSum; // 正規化
		
		float4x4 skinMatrix = 
			boneTransforms[vin.index.x] * w.x +
			boneTransforms[vin.index.y] * w.y +
			boneTransforms[vin.index.z] * w.z +
			boneTransforms[vin.index.w] * w.w;

		localPos = mul(localPos, skinMatrix);
	}

	// ワールド変換
	vout.pos = mul(localPos, world);
	vout.wPos = vout.pos;

	// ビュー・プロジェクション変換
	vout.pos = mul(vout.pos, view);
	vout.pos = mul(vout.pos, proj);

	// 法線変換
	vout.normal = mul(localNormal, (float3x3)world);
	vout.normal = normalize(vout.normal);

	// その他
	vout.uv = vin.uv;
	vout.color = vin.color * materialColor;

	return vout;
}

// =========================================================
// ピクセルシェーダー
// =========================================================
float4 PS(VS_OUT pin) : SV_TARGET
{
	// テクスチャカラー
	float4 color = tex.Sample(samp, pin.uv);
	color *= pin.color; // 頂点カラー(マテリアル色含む)を乗算

	// ライティング (Lambert)
	float3 N = normalize(pin.normal);
	float3 L = normalize(-lightDir.xyz);
	float dotNL = saturate(dot(N, L));

	float3 diffuse = lightColor.rgb * dotNL;
	float3 ambient = float3(0.2f, 0.2f, 0.2f); // 簡易アンビエント

	// 最終カラー
	color.rgb = color.rgb * (diffuse + ambient);

	return color;
}