/*****************************************************************//**
 * @file	ButtonSystem.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date   2025/12/29	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___BUTTON_SYSTEM_H___
#define ___BUTTON_SYSTEM_H___

#include "Engine/pch.h"
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Scene/Components/UIComponents.h" // 作成したファイル
#include "Engine/Core/Window/Input.h"
#include "Engine/Core/Context.h"

namespace Arche
{
	class ButtonSystem : public ISystem
	{
	public:
		ButtonSystem() { m_systemName = "Button System"; }

		//void Update(Registry& registry) override
		//{
		//	// マウス位置の取得
		//	// Input::GetMousePosition() はクライアント座標(左上0,0)を返すと仮定
		//	auto mousePos = Input::GetMousePosition();
		//	float mx = (float)mousePos.x;
		//	float my = (float)mousePos.y;

		//	// Y-Up座標系 (画面中央0,0, 上が+Y) に変換する必要がある場合
		//	// Inputの実装によりますが、UISystemの calculatedRect と合わせる必要があります。
		//	// ここでは UISystem が出力する calculatedRect が「スクリーン座標系(左上0,0)」に
		//	// 変換されているか、あるいはワールド座標系かを確認する必要があります。

		//	// ※前回の修正で UISystem はワールド座標(中心0,0) で計算しています。
		//	// なのでマウス座標も中心基準に変換します。
		//	float screenW = (float)Config::SCREEN_WIDTH;
		//	float screenH = (float)Config::SCREEN_HEIGHT;

		//	// Screen(0,0 TopLeft) -> World(0,0 Center, Y-Up)
		//	float worldMouseX = mx - (screenW * 0.5f);
		//	float worldMouseY = (screenH * 0.5f) - my;

		//	bool isMousePressed = Input::GetMouseButton(MouseButton::Left);
		//	bool isMouseClicked = Input::GetMouseButtonDown(MouseButton::Left);

		//	registry.view<ButtonComponent, Transform2D>().each([&](Entity e, ButtonComponent& btn, Transform2D& t)
		//		{
		//			if (!btn.enabled || !t.enabled) return;

		//			// 1. 当たり判定 (AABB)
		//			// rect = { left, top, right, bottom } (Y-Upなので top > bottom)
		//			const auto& r = t.calculatedRect;

		//			// 単純なAABB判定
		//			bool hit = (worldMouseX >= r.x && worldMouseX <= r.z &&
		//				worldMouseY <= r.y && worldMouseY >= r.w);

		//			// 2. 状態更新
		//			btn.isHovered = hit;
		//			btn.isClicked = false; // リセット

		//			if (hit)
		//			{
		//				if (isMousePressed)
		//				{
		//					btn.isPressed = true;
		//				}
		//				else
		//				{
		//					// 離した瞬間の判定が必要なら工夫が必要だが、
		//					// ここでは簡易的に「押した瞬間」を検知
		//					if (isMouseClicked)
		//					{
		//						btn.isClicked = true;
		//						Logger::Log("Button Clicked: Entity " + std::to_string((uint32_t)e));
		//					}
		//					btn.isPressed = false;
		//				}
		//			}
		//			else
		//			{
		//				btn.isPressed = false;
		//			}

		//			// 3. 色の反映 (TargetとなるSpriteやTextの色を変える)
		//			XMFLOAT4 targetColor = btn.normalColor;
		//			if (btn.isPressed) targetColor = btn.pressedColor;
		//			else if (btn.isHovered) targetColor = btn.hoverColor;

		//			// Spriteがあれば色変更
		//			if (registry.has<SpriteComponent>(e))
		//			{
		//				registry.get<SpriteComponent>(e).color = targetColor;
		//			}
		//			// Textがあれば色変更
		//			if (registry.has<TextComponent>(e))
		//			{
		//				registry.get<TextComponent>(e).color = targetColor;
		//			}
		//		});
		//}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::ButtonSystem, "ButtonSystem")

#endif // !___BUTTON_SYSTEM_H___