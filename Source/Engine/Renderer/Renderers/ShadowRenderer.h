/*****************************************************************//**
 * @file	ShadowRenderer.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___SHADOW_RENDERER_H___
#define ___SHADOW_RENDERER_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Renderer/Data/Model.h"

namespace Arche
{
	class ShadowRenderer
	{
	public:
		// 定数バッファ (ShadowMap.hlslに合わせる)
		struct CBData
		{
			XMMATRIX world;
			XMMATRIX view;
			XMMATRIX projection;
			XMMATRIX boneTransforms[200];
			int hasAnimation;
			float padding[3];
		};

		static void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
		static void Shutdown();

		// 描画開始 (ライトのビュー・プロジェクション行列を渡す)
		static void Begin(const XMMATRIX& lightView, const XMMATRIX& lightProj);

		// モデル描画
		static void Draw(std::shared_ptr<Model> model, const DirectX::XMMATRIX& worldMatrix);

	private:
		static ID3D11Device* s_device;
		static ID3D11DeviceContext* s_context;

		static ComPtr<ID3D11VertexShader> s_vs;
		static ComPtr<ID3D11InputLayout> s_inputLayout;
		static ComPtr<ID3D11Buffer> s_constantBuffer;

		static ComPtr<ID3D11PixelShader> s_ps;

		static CBData s_cbData;
	};
}

#endif // !___SHADOW_RENDERER_H___