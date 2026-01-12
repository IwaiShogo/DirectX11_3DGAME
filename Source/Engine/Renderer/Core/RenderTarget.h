/*****************************************************************//**
 * @file	RenderTarget.h
 * @brief	シーンビュー、ゲームビューの実体
 * 
 * @details	
 * 「画面に表示せずに、メモリ上の画像に描画する」ためのクラス作成。 
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___RENDER_TARGET_H___
#define ___RENDER_TARGET_H___

// ===== インクルード =====
#include"Engine/pch.h"

namespace Arche
{

	class RenderTarget
	{
	public:
		RenderTarget() = default;	// デフォルトコンストラクタ（make_unique用）
		~RenderTarget() = default;

		/**
		 * @brief	初期化・再生成
		 * @param	device	デバイス
		 * @param	width	画面幅
		 * @param	height	画面高さ
		 */
		void Create(ID3D11Device* device, int width, int height);

		/**
		 * @brief	描画先として設定
		 * @param	context	コンテキスト
		 */
		void Bind(ID3D11DeviceContext* context);

		/**
		 * @brief	描画結果をクリア
		 * @param	context	コンテキスト
		 * @param	r		背景色（R）
		 * @param	g		背景色（G）
		 * @param	b		背景色（B）
		 * @param	a		背景色（A）
		 */
		void Clear(ID3D11DeviceContext* context, float r, float g, float b, float a);

		/**
		 * @brief	ImGuiやテクスチャとして扱うためのSRV
		 * @return	ID3D11ShaderResourceView*
		 */
		ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }

		/**
		 * @brief	互換性のため（ImGui::Image用）
		 */
		void* GetID() const { return m_srv.Get(); }
		
		// --- 画面サイズの取得 ---
		int GetWidth() const { return m_width; }
		int GetHeight() const { return m_height; }

	private:
		int m_width = 0;
		int m_height = 0;
		
		ComPtr<ID3D11Texture2D>				m_texture;
		ComPtr<ID3D11RenderTargetView>		m_rtv;
		ComPtr<ID3D11ShaderResourceView>	m_srv;	// ImGui表示用
		ComPtr<ID3D11DepthStencilView>		m_dsv;
	};

}	// namespace Arche

#endif // !___RENDER_TARGET_H___