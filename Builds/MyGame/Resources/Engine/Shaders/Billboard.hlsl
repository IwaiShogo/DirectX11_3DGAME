cbuffer ConstantBuffer : register(b0)
{
    matrix World; // ここでは「位置」のみを使用
    matrix View; // カメラの向き
    matrix Projection; // 投影
    float4 Color;
}

Texture2D tex : register(t0);
SamplerState sam : register(s0);

struct VS_INPUT
{
    float4 Pos : POSITION; // 中心からのオフセット位置
    float2 UV : TEXCOORD0;
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

	// 1. ビルボード回転の計算
	// View行列の「転置行列」の要素を使うと、カメラの「右方向」と「上方向」がわかります
	// View行列の逆回転を掛けることで、カメラと正対させます
	
    float3 cameraRight = float3(View._11, View._21, View._31);
    float3 cameraUp = float3(View._12, View._22, View._32);
	
	// ワールド行列から「中心座標」を取り出す (4行目のxyz)
    float3 centerPos = float3(World._41, World._42, World._43);
	
	// ワールド行列から「スケール」を取り出す (簡易的に1,1要素などを参照)
	// 正確には scale.x = length(World._11_12_13) ですが、
	// C++側でスケーリング済みの頂点を渡すか、ここで簡易計算します。
	// 今回は入力頂点(input.Pos)に既にサイズが乗算されている前提で計算します。
	
	// 2. 頂点位置の決定
	// 中心座標 + (カメラ右向き * Xオフセット) + (カメラ上向き * Yオフセット)
    float3 finalPos = centerPos
					+ (cameraRight * input.Pos.x)
					+ (cameraUp * input.Pos.y);

	// 3. ビュー・プロジェクション変換
    output.Pos = mul(float4(finalPos, 1.0f), View);
    output.Pos = mul(output.Pos, Projection);
	
    output.UV = input.UV;
	
    return output;
}

// ------------------------------------------------------------
// Pixel Shader
// ------------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
    float4 color = tex.Sample(sam, input.UV) * Color;
	
	// アルファテスト（透明度が高いピクセルは描画しない＝深度バッファに書かない）
	// 草や木を描くときはこれがないと四角い枠が見えてしまいます
    clip(color.a - 0.1f);
	
    return color;
}