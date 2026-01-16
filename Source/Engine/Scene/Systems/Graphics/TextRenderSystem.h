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
#include "Engine/Scene/Components/Components.h"
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
			m_group = SystemGroup::Overlay;
		}

		void Update(Registry& registry) override {}

		void Render(Registry& registry, const Context& ctx) override
		{
			XMMATRIX view = XMMatrixIdentity();
			XMMATRIX proj = XMMatrixIdentity();
			bool cameraFound = false;

			float currentAspect = static_cast<float>(Config::SCREEN_WIDTH) / static_cast<float>(Config::SCREEN_HEIGHT);

			if (ctx.renderCamera.useOverride)
			{
				// エディタ（シーンビュー）
				view = XMLoadFloat4x4(&ctx.renderCamera.viewMatrix);
				proj = XMLoadFloat4x4(&ctx.renderCamera.projMatrix);
				cameraFound = true;
			}
			else
			{
				// ゲームビュー（通常時）
				auto cameraView = registry.view<Camera, Transform>();
				for (auto e : cameraView)
				{
					auto& cam = cameraView.get<Camera>(e);
					auto& t = cameraView.get<Transform>(e);

					// RenderSystem.cppの実装に合わせる
					XMVECTOR eye = XMLoadFloat3(&t.position);

					XMMATRIX rot = XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, 0.0f);

					XMVECTOR look = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rot);
					XMVECTOR up = XMVector3TransformCoord(XMVectorSet(0, 1, 0, 0), rot);

					view = XMMatrixLookToLH(eye, look, up);

					// 修正したアスペクト比を使用
					proj = XMMatrixPerspectiveFovLH(cam.fov, cam.aspect, cam.nearZ, cam.farZ);

					cameraFound = true;
					break;
				}
			}

			// カメラが見つかった場合のみ描画
			if (cameraFound)
			{
				TextRenderer::Draw(registry, view, proj, nullptr);
			}
		}
	};

}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::TextRenderSystem, "Text Render System")

#endif // !___TEXT_RENDER_SYSTEM_H___