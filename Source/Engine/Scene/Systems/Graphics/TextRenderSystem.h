/*****************************************************************//**
 * @file	TextRenderSystem.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date   2025/12/18	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___TEXT_RENDER_SYSTEM_H___
#define ___TEXT_RENDER_SYSTEM_H___

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Renderer/Text/TextRenderer.h"

namespace Arche
{

	class TextRenderSystem
		: public ISystem
	{
	public:
		TextRenderSystem()
		{
			m_systemName = "Text Render System";
		}

		void Update(Registry& registry) override {}

		void Render(Registry& registry, const Context& ctx) override
		{
			// カメラ行列の取得 (BillboardSystemなどと同様)
			XMMATRIX view = XMMatrixIdentity();
			XMMATRIX proj = XMMatrixIdentity();
			bool cameraFound = false;

			auto cameraView = registry.view<Camera, Transform>();
			for (auto e : cameraView)
			{
				auto& cam = cameraView.get<Camera>(e);
				auto& t = cameraView.get<Transform>(e);

				XMVECTOR eye = XMLoadFloat3(&t.position);
				XMMATRIX rot = XMMatrixRotationRollPitchYaw(XMConvertToRadians(t.rotation.x), XMConvertToRadians(t.rotation.y), 0.0f);
				XMVECTOR look = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rot);
				XMVECTOR up = XMVector3TransformCoord(XMVectorSet(0, 1, 0, 0), rot);

				view = XMMatrixLookToLH(eye, look, up);
				proj = XMMatrixPerspectiveFovLH(cam.fov, cam.aspect, cam.nearZ, cam.farZ);
				cameraFound = true;
				break;
			}

			// 3D描画のために行列を渡す
			TextRenderer::Draw(registry, view, proj, nullptr);
		}
	};

}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::TextRenderSystem, "Text Render System")

#endif // !___TEXT_RENDER_SYSTEM_H___