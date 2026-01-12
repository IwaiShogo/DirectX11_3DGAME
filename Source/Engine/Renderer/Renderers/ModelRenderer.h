/*****************************************************************//**
 * @file	ModelRenderer.h
 * @brief	モデルクラスが持つ複数のメッシュをループして描画するレンダラー
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/11/26	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___MODEL_RENDERER_H___
#define ___MODEL_RENDERER_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Renderer/Data/Model.h"

namespace Arche
{

	class ARCHE_API ModelRenderer
	{
	public:
		// MainCB (b0)
		struct CBData
		{
			XMMATRIX world;		 // ワールド行列
			XMMATRIX view;		 // ビュー行列
			XMMATRIX projection; // プロジェクション行列
			XMMATRIX boneTransforms[200]; // ボーン行列 (最大200)
			XMFLOAT4 lightDir;	 // ライト方向
			XMFLOAT4 lightColor; // ライト色
			XMFLOAT4 materialColor; // マテリアル色
			int hasAnimation;	 // アニメーション有無フラグ
			float padding[3];

			// 影計算用行列
			XMMATRIX lightView;
			XMMATRIX lightProj;
		};

		// SceneLightCB (b1)
		struct PointLightData
		{
			XMFLOAT3 position;
			float range;
			XMFLOAT3 color;
			float intensity;
		};

		struct SceneLightCBData
		{
			XMFLOAT3 ambientColor;
			float ambientIntensity;
			int pointLightCount;
			float _padding[3];

			PointLightData pointLights[32];
		};

	public:
		static void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
		static void Shutdown();

		static void Begin(const XMMATRIX& view, const XMMATRIX& projection, const XMFLOAT3& lightDir, const XMFLOAT3& lightColor);

		static void SetSceneLights(const XMFLOAT3& ambientColor, float ambientIntensity, const std::vector<PointLightData>& lights);

		static void SetShadowMap(ID3D11ShaderResourceView* srv);
		static void SetLightMatrix(const XMMATRIX& view, const XMMATRIX& proj);

		static void Draw(std::shared_ptr<Model> model, const XMFLOAT3& pos, const XMFLOAT3& scale = { 1,1,1 }, const XMFLOAT3& rot = { 0,0,0 });
		static void Draw(std::shared_ptr<Model> model, const DirectX::XMMATRIX& worldMatrix);

	private:
		static void CreateWhiteTexture();

	private:
		static ID3D11Device* s_device;
		static ID3D11DeviceContext* s_context;

		static ComPtr<ID3D11VertexShader> s_vs;
		static ComPtr<ID3D11PixelShader> s_ps;
		static ComPtr<ID3D11InputLayout> s_inputLayout;

		static ComPtr<ID3D11Buffer> s_constantBuffer;
		static ComPtr<ID3D11Buffer> s_lightConstantBuffer;

		static ComPtr<ID3D11SamplerState> s_samplerState;
		static ComPtr<ID3D11RasterizerState> s_rsSolid;

		static ComPtr<ID3D11ShaderResourceView> s_whiteTexture;
		static CBData s_cbData;
		static SceneLightCBData s_lightData;

		// 影用リソース
		static ComPtr<ID3D11ShaderResourceView> s_shadowSRV;
		static ComPtr<ID3D11SamplerState> s_shadowSampler;
		static XMMATRIX s_lightView;
		static XMMATRIX s_lightProj;
	};

}	// namespace Arche

#endif // !___MODEL_RENDERER_H___