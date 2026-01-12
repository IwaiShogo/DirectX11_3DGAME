/*****************************************************************//**
 * @file	RenderSystem.h
 * @brief	描画処理
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/11/23	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___RENDER_SYSTEM_H___
#define ___RENDER_SYSTEM_H___

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Resource/ResourceManager.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Renderer/Renderers/GridRenderer.h"
#include "Engine/Renderer/Renderers/SkyboxRenderer.h"

namespace Arche
{
	class RenderSystem
		: public ISystem
	{
	public:
		RenderSystem();

		void Render(Registry& registry, const Context& context) override;

		// 3D座標 -> 2Dスクリーン座標への変換
		// 戻り値: スクリーン座標 (x, y)。zは深度(0-1)。
		// 画面外なら false を返すような判定も可能ですが今回は座標のみ計算。
		static DirectX::XMFLOAT3 WorldToScreen(
			const DirectX::XMFLOAT3& worldPos,
			const DirectX::XMMATRIX& view,
			const DirectX::XMMATRIX& proj,
			float screenW, float screenH)
		{
			XMVECTOR vWorld = XMLoadFloat3(&worldPos);

			// 座標変換 (World -> View -> Clip)
			XMVECTOR vClip = XMVector3TransformCoord(vWorld, view);
			vClip = XMVector3TransformCoord(vClip, proj);

			XMFLOAT3 clip;
			XMStoreFloat3(&clip, vClip);

			// NDC (-1~1) -> Screen (0~W, 0~H)
			float screenX = (clip.x + 1.0f) * 0.5f * screenW;
			float screenY = (1.0f - clip.y) * 0.5f * screenH; // Yは反転

			return XMFLOAT3(screenX, screenY, clip.z);
		}

	private:
		GridRenderer m_gridRenderer;
		SkyboxRenderer m_skyboxRenderer;

		bool m_isInitialized = false;
	};

}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::RenderSystem, "Render System")

#endif // !___RENDER_SYSTEM_H___