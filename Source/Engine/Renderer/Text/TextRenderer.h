/*****************************************************************//**
 * @file	TextRenderer.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/12/18	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___TEXT_RENDERER_H___
#define ___TEXT_RENDERER_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Renderer/Text/FontManager.h"

namespace Arche
{

	class ARCHE_API TextRenderer
	{
	public:
		// インスタンス化禁止
		~TextRenderer() = default;

		/**
		 * @brief	静的初期化
		 * @param	device	デバイス
		 * @param	context	コンテキスト
		 */
		static void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

		/**
		 * @brief	終了処理（キャッシュ解放）
		 */
		static void Shutdown();

		static void ClearCache();

		/**
		 * @brief	テキスト描画実行
		 * @details	現在バインドされているRTVを取得して、その上にD2Dで描画します
		 * @param	registry	ECSレジストリ
		 * @param	rtv			描画先のRTV（nullptrの場合は現在バインドされている物を取得して使用）
		 */
		static void Draw(Registry& registry, const XMMATRIX& view, const XMMATRIX& projection, ID3D11RenderTargetView* rtv = nullptr);

	private:
		// D3D11テクスチャに関連付けられたD2Dレンダーターゲットを作成・取得
		static ID2D1RenderTarget* GetD2DRenderTarget(ID3D11RenderTargetView* rtv);

	private:
		// 静的メンバ変数
		static ID3D11Device* s_device;
		static ID3D11DeviceContext* s_context;

		// D2Dリソース
		static ComPtr<ID2D1Factory> s_d2dFactory;
		static ComPtr<ID2D1SolidColorBrush> s_brush;
		
		// RTVポインタをキーにしてD2Dターゲットをキャッシュ
		// （Debug/Release切り替えやリサイズ時に対応）
		static std::unordered_map<ID3D11RenderTargetView*, ComPtr<ID2D1RenderTarget>> s_d2dTargets;
	};

}	// namespace Arche

#endif // !___TEXT_RENDERER_H___