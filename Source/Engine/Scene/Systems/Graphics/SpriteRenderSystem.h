/*****************************************************************//**
 * @file	SpriteRenderSystem.h
 * @brief	SpriteRendrerを使って描画を行うシステム
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

#ifndef ___SPRITE_RENDERER_SYSTEM_H___
#define ___SPRITE_RENDERER_SYSTEM_H___

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Renderer/Renderers/SpriteRenderer.h"
#include "Engine/Resource/ResourceManager.h"

namespace Arche
{

	class SpriteRenderSystem
		: public ISystem
	{
	public:
		SpriteRenderSystem()
		{
			m_systemName = "Sprite Render System";
		}

		void Render(Registry& registry, const Context& context) override
		{
			// 2D描画開始
			SpriteRenderer::Begin();

			registry.view<SpriteComponent, Transform2D>().each([&](Entity e, SpriteComponent& s, Transform2D& t2d)
			{
				// テクスチャ取得
				auto tex = ResourceManager::Instance().GetTexture(s.textureKey);
				if (tex)
				{
					// 1. 矩形変形行列
					XMMATRIX matGeometry = XMMatrixScaling(t2d.size.x, t2d.size.y, 1.0f) *
						XMMatrixTranslation(-t2d.size.x * t2d.pivot.x, -t2d.size.y * t2d.pivot.y, 0.0f);

					auto& m = t2d.worldMatrix;
					XMMATRIX matWorld = XMMatrixSet(
						m._11, m._12, 0.0f, 0.0f,
						m._21, m._22, 0.0f, 0.0f,
						0.0f,  0.0f,  1.0f, 0.0f,
						m._31, m._32, 0.0f, 1.0f
					);

					// 3. 合成
					XMMATRIX finalMat = matGeometry * matWorld;

					// 描画
					SpriteRenderer::Draw(tex.get(), finalMat, s.color);
				}
			});
		}
	};

}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::SpriteRenderSystem, "Sprite Render System")

#endif // !___SPRITE_RENDERER_SYSTEM_H___