// =========================================================
// Standard.hlsl
// =========================================================

// b0: メイン定数バッファ (オブジェクト単位 + ディレクショナルライト)
cbuffer MainCB : register(b0)
{
	float4x4 world;			 // ワールド行列
	float4x4 view;			 // ビュー行列
	float4x4 proj;			 // プロジェクション行列
	float4x4 boneTransforms[200]; // ボーン行列
	float4 lightDir;		 // ディレクショナルライト方向
	float4 lightColor;		 // ディレクショナルライト色
	float4 materialColor;	 // マテリアル色
	int hasAnimation;		 // アニメーションフラグ
	float3 padding;
	float4x4 lightView;
	float4x4 lightProj;
};

struct PointLight
{
	float3 position;
	float range;
	float3 color;
	float intensity;
};

cbuffer SceneLightCB : register(b1)
{
	float3 ambientColor;
	float ambientIntensity;
	int pointLightCount;
	float _padding1;
	
	PointLight pointLights[32]; // 最大32個まで対応
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
	float4 pos		: SV_POSITION;
	float3 normal	: NORMAL0;
	float2 uv		: TEXCOORD0;
	float4 color	: COLOR0;
	float4 wPos		: POSITION0;
	float4 lPos		: TEXCOORD1;
};

Texture2D tex : register(t0);
SamplerState samp : register(s0);

Texture2D shadowMap : register(t1);
SamplerComparisonState shadowSamp : register(s1);

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
		float wSum = dot(vin.weight, float4(1,1,1,1));
		if (wSum < 0.001f) { vin.weight = float4(1, 0, 0, 0); wSum = 1.0f; }
		float4 w = vin.weight / wSum;
		
		float4x4 skinMatrix = 
			boneTransforms[vin.index.x] * w.x +
			boneTransforms[vin.index.y] * w.y +
			boneTransforms[vin.index.z] * w.z +
			boneTransforms[vin.index.w] * w.w;

		localPos = mul(localPos, skinMatrix);
		// 法線も回転させるべきですが簡易実装のため省略
	}

	// ワールド変換
	vout.pos = mul(localPos, world);
	vout.wPos = vout.pos; // ワールド座標を保存
	
	// ライトビュープロジェクション変換
	float4 lightPos = mul(vout.wPos, lightView);
	lightPos = mul(lightPos, lightProj);
	vout.lPos = lightPos;

	// ビュー・プロジェクション変換
	vout.pos = mul(vout.pos, view);
	vout.pos = mul(vout.pos, proj);

	// 法線変換
	vout.normal = mul(localNormal, (float3x3)world);
	vout.normal = normalize(vout.normal);

	vout.uv = vin.uv;
	vout.color = vin.color * materialColor;

	return vout;
}

// =========================================================
// ピクセルシェーダー
// =========================================================
float4 PS(VS_OUT pin) : SV_TARGET
{
	float4 texColor = tex.Sample(samp, pin.uv);
	texColor *= pin.color;

	float3 N = normalize(pin.normal);
	
	// 拡散反射光の総和
	float3 totalDiffuse = float3(0, 0, 0);

	// -----------------------------------------------------
	// 1. 影の計算 (Shadow Mapping)
	// -----------------------------------------------------
	float shadow = 1.0f; // 1.0 = 光が当たる, 0.0 = 影
	
	// ライト空間の同次座標除算 (-1~1へ)
	float3 projCoords = pin.lPos.xyz / pin.lPos.w;
	
	// UV空間 (0~1) へ変換 (Yは反転)
	projCoords.x = projCoords.x * 0.5f + 0.5f;
	projCoords.y = -projCoords.y * 0.5f + 0.5f;
	
	// 深度バイアス (シャドウアクネ対策)
	float bias = 0.005f;
	
	// 範囲内チェック（ライトの視錐台の中か？）
	if (projCoords.x >= 0.0f && projCoords.x <= 1.0f &&
		projCoords.y >= 0.0f && projCoords.y <= 1.0f &&
		projCoords.z <= 1.0f)
	{
		// 影マップサンプリング (現在のピクセルの深度と比較)
		// SampleCmpLevelZero はハードウェアPCFを使用して境界を滑らかにします
		shadow = shadowMap.SampleCmpLevelZero(shadowSamp, projCoords.xy, projCoords.z - bias);
	}
	
	// -----------------------------------------------------
	// 2. ディレクショナルライト (MainCB)
	// -----------------------------------------------------
	{
		float3 L = normalize(-lightDir.xyz);
		float dotNL = saturate(dot(N, L));
		// 影を適用 (shadowを掛ける)
		totalDiffuse += lightColor.rgb * dotNL * shadow;
	}

	// -----------------------------------------------------
	// 3. ポイントライト (SceneLightCB)
	// -----------------------------------------------------
	for(int i = 0; i < pointLightCount; ++i)
	{
		PointLight pl = pointLights[i];
		
		float3 toLight = pl.position - pin.wPos.xyz;
		float dist = length(toLight);
		
		if(dist < pl.range)
		{
			float3 L = normalize(toLight);
			float dotNL = saturate(dot(N, L));
			
			// 減衰
			float atten = 1.0 - saturate(dist / pl.range);
			atten *= atten; // 二乗減衰風
			
			totalDiffuse += pl.color * pl.intensity * dotNL * atten;
		}
	}

	// -----------------------------------------------------
	// 4. アンビエントと合成
	// -----------------------------------------------------
	float3 ambient = ambientColor * ambientIntensity;
	
	// 最終カラー = テクスチャ色 * (拡散光 + 環境光)
	texColor.rgb = texColor.rgb * (totalDiffuse + ambient);

	return texColor;
}