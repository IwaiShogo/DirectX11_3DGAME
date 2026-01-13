// Resources/Engine/Shaders/InfiniteGrid.hlsl

cbuffer ConstantBuffer : register(b0)
{
    matrix View;
    matrix Projection;
    float3 CameraPos;
    float Near;
    float Far;
    float3 Padding;
}

cbuffer GridConstants : register(b1)
{
    matrix InverseViewProj;
}

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float3 NearPos : TEXCOORD0;
    float3 FarPos : TEXCOORD1;
};

struct PS_OUTPUT
{
    float4 Color : SV_Target;
    float Depth : SV_Depth; // 深度値を出力するためのセマンティクス
};

// --------------------------------------------------------
// Vertex Shader
// --------------------------------------------------------
VS_OUTPUT mainVS(uint id : SV_VertexID)
{
    VS_OUTPUT output;
    
    // 画面全体を覆う三角形を生成 (ID: 0, 1, 2 でカバー可能)
    float2 grid = float2((id << 1) & 2, id & 2);
    float4 pos = float4(grid * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f); // Z=0 (Near Plane)
    
    output.Pos = pos;

    // クリップ空間座標からワールド空間座標へUnproject
    float4 nearW = mul(float4(pos.xy, 0.0, 1.0), InverseViewProj);
    nearW /= nearW.w;
    output.NearPos = nearW.xyz;

    float4 farW = mul(float4(pos.xy, 1.0, 1.0), InverseViewProj);
    farW /= farW.w;
    output.FarPos = farW.xyz;

    return output;
}

// --------------------------------------------------------
// Pixel Shader
// --------------------------------------------------------
PS_OUTPUT mainPS(VS_OUTPUT input)
{
    PS_OUTPUT output;

    // レイの定義
    float3 rayOrigin = input.NearPos;
    float3 rayDir = normalize(input.FarPos - input.NearPos);

    // Y=0 平面との交差判定 (P = Origin + t * Dir)
    if (abs(rayDir.y) < 0.001) discard;
    float t = -rayOrigin.y / rayDir.y;
    if (t < 0) discard;
    
    float3 worldPos = rayOrigin + t * rayDir;
    
    // ▼▼▼ 修正: 正しい深度値を計算して出力 ▼▼▼
    float4 clipPos = mul(float4(worldPos, 1.0), mul(View, Projection));
    output.Depth = clipPos.z / clipPos.w;
    // ▲▲▲ ここまで ▲▲▲

    // FarPlaneより遠い場合は破棄
    if (output.Depth >= 1.0f) discard; 

    // --- グリッド描画計算 ---
    float2 coord = worldPos.xz;
    float2 derivative = fwidth(coord);
    
    // 1m間隔
    float2 grid = abs(frac(coord - 0.5) - 0.5) / derivative;
    float lineVal = min(grid.x, grid.y);
    float alpha = 1.0 - min(lineVal, 1.0);
    
    // 10m間隔
    float2 thickGrid = abs(frac(coord * 0.1 - 0.5) - 0.5) / derivative * 10.0;
    float thickLineVal = min(thickGrid.x, thickGrid.y);
    float thickAlpha = 1.0 - min(thickLineVal, 1.0);
    
    // 軸 (X, Z)
    float minZ = abs(coord.y) / derivative.y;
    float minX = abs(coord.x) / derivative.x;
    
    float4 color = float4(0.2, 0.2, 0.2, 0.5); // ベース色
    
    if(thickAlpha > 0.1) color = lerp(color, float4(0.1, 0.1, 0.1, 0.8), thickAlpha);
    if(minZ < 1.0) color = lerp(color, float4(0.8, 0.2, 0.2, 1.0), 1.0 - minZ); // X軸 (赤)
    if(minX < 1.0) color = lerp(color, float4(0.2, 0.2, 0.8, 1.0), 1.0 - minX); // Z軸 (青)

    color.a *= max(alpha, max(thickAlpha, 0.0)); // 修正: axisAlpha除外または修正

    // 距離フェード
    float dist = distance(CameraPos.xz, worldPos.xz);
    float fade = 1.0 - smoothstep(10.0, Far * 0.8, dist);
    color.a *= fade;

    if (color.a <= 0.0) discard;

    output.Color = color;
    return output;
}