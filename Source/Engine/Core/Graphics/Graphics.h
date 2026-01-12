/*****************************************************************//**
 * @file	Graphics.h
 * @brief	画面のコピー機能を提供するクラス
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___GRAPHICS_H___
#define ___GRAPHICS_H___

// ===== インクルード =====
#include "Engine/pch.h"

namespace Arche
{
	class ARCHE_API Graphics
	{
	public:
		// 初期化（Application.cppなどで呼ぶ必要があります）
		static void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapChain);

		// 終了処理
		static void Shutdown();

		/**
		 * @brief	キャプチャ対象のソースビューを設定する
		 * @param	srv	キャプチャ元のSRV
		 */
		static void SetCaptureSource(ID3D11ShaderResourceView* srv);

		/**
		 * @brief	現在のバックバッファをコピーして新しいTextureを作成し、ResourceManagerに登録する
		 * @param	resourceKey ResourceManagerに登録する際の名前
		 */
		static void CopyBackBufferToTexture(const std::string& resourceKey);

	private:
		static ID3D11Device* s_device;
		static ID3D11DeviceContext* s_context;
		static IDXGISwapChain* s_swapChain;

		// キャプチャ元のSRV
		static ID3D11ShaderResourceView* s_captureSource;
	};
}

#endif // !___GRAPHICS_H___