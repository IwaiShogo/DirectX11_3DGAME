/*****************************************************************//**
 * @file	ShadowMap.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___SHADOW_MAP_H___
#define ___SHADOW_MAP_H___

// ===== インクルード =====
#include "Engine/pch.h"

namespace Arche
{
	class ShadowMap
	{
	public:
		ShadowMap() = default;
		~ShadowMap();

		// 初期化 (解像度を指定: 1024, 2048, 4096など)
		void Initialize(ID3D11Device* device, float width, float height);

		// 影描画の開始 (レンダーターゲットを切り替え)
		void Begin(ID3D11DeviceContext* context);

		// 影描画の終了 (レンダーターゲットを元に戻す処理はRenderSystemで行うため、ここではリソース化の準備のみ)
		void End(ID3D11DeviceContext* context);

		// シェーダーに渡すリソースビューを取得
		ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }

		// ビューポート情報
		D3D11_VIEWPORT GetViewport() const { return m_viewport; }

	private:
		float m_width = 0.0f;
		float m_height = 0.0f;

		D3D11_VIEWPORT m_viewport = {};

		// 深度テクスチャ本体
		ComPtr<ID3D11Texture2D> m_texture;
		// 書き込み用ビュー (Depth Stencil View)
		ComPtr<ID3D11DepthStencilView> m_dsv;
		// 読み込み用ビュー (Shader Resource View)
		ComPtr<ID3D11ShaderResourceView> m_srv;
	};

}	// namespace Arche

#endif // !___SHADOW_MAP_H___