// DebugPrimitive.hlsl

// 定数バッファ（C++側から送られる行列データ）
cbuffer ConstantBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
    float4 Color;   // RGBA
}

// 入力頂点データ
struct VS_INPUT
{
    float4 Pos : POSITION;
};

// ピクセルシェーダーへ渡すデータ
struct PS_INPUT
{
    float4 POS : SV_Position;
};

// ------------------------------------------------------------
// Vertex Shader
// ------------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;
    
    // 座標変換: Local -> World -> View -> Projection
    output.POS = mul(input.Pos, World);
    output.POS = mul(output.POS, View);
    output.POS = mul(output.POS, Projection);
    
    return output;
}

// ------------------------------------------------------------
// Pixel Shader
// ------------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
    // 単色を返す
    return Color;
}