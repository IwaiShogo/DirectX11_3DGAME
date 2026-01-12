// 定数バッファ
cbuffer ConstantBuffer : register(b0)
{
    matrix World;
    matrix Projection; // 2DなのでView行列は不要（正射影行列を直接使う）
    float4 Color;
}

// テクスチャとサンプラー
Texture2D tex : register(t0);
SamplerState sam : register(s0);

struct VS_INPUT
{
    float4 Pos : POSITION;
    float2 UV : TEXCOORD0; // テクスチャ座標
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};

// ------------------------------------------------------------
// Vertex Shader
// ------------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;
	
	// 座標変換: Local -> World -> Projection (Orthographic)
    output.Pos = mul(input.Pos, World);
    output.Pos = mul(output.Pos, Projection);
	
    output.UV = input.UV;
	
    return output;
}

// ------------------------------------------------------------
// Pixel Shader
// ------------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
	// テクスチャの色 × 指定色
    return tex.Sample(sam, input.UV) * Color;
}