/*****************************************************************//**
 * @file	BillboardSystem.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/11/27	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___BILLBOARD_SYSTEM_H___
#define ___BILLBOARD_SYSTEM_H___

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Renderer/Renderers/BillboardRenderer.h"
#include "Engine/Resource/ResourceManager.h"

namespace Arche
{

	class BillboardSystem : public ISystem
	{
	public:
		BillboardSystem()
		{
			m_systemName = "Billboard System";
		}

		void Render(Registry& registry, const Context& context) override
		{
			// カメラ計算 (RenderSystem等と同じロジック。共通化推奨)
			XMMATRIX viewMatrix = XMMatrixIdentity();
			XMMATRIX projMatrix = XMMatrixIdentity();
			bool cameraFound = false;
			registry.view<Camera, Transform>().each([&](Entity e, Camera& cam, Transform& trans)
				{
					if (cameraFound) return;
					XMVECTOR eye = XMLoadFloat3(&trans.position);
					XMMATRIX rotM = XMMatrixRotationRollPitchYaw(trans.rotation.x, trans.rotation.y, 0.0f);
					XMVECTOR look = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rotM);
					XMVECTOR up = XMVector3TransformCoord(XMVectorSet(0, 1, 0, 0), rotM);
					viewMatrix = XMMatrixLookToLH(eye, look, up);
					projMatrix = XMMatrixPerspectiveFovLH(cam.fov, cam.aspect, cam.nearZ, cam.farZ);
					cameraFound = true;
				});
			if (!cameraFound) return;

			// 描画開始
			BillboardRenderer::Begin(viewMatrix, projMatrix);

				registry.view<Transform, BillboardComponent>().each([&](Entity e, Transform& t, BillboardComponent& b)
					{
						auto tex = ResourceManager::Instance().GetTexture(b.textureKey);
						if (tex)
						{
							// Transformのスケールも加味する場合:
							float w = b.size.x * t.scale.x;
							float h = b.size.y * t.scale.y;

							BillboardRenderer::Draw(tex.get(), t.position, w, h, b.color);
						}
					});
		}
	};

}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::BillboardSystem, "Billboard System")

#endif // !___BILLBOARD_SYSTEM_H___