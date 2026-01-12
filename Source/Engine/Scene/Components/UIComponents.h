/*****************************************************************//**
 * @file	UIComponents.h
 * @brief	UI用コンポーネント
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___UI_COMPONENTS_H___
#define ___UI_COMPONENTS_H___

#include "Engine/pch.h"
#include "Engine/Scene/Components/Components.h"

namespace Arche
{
	/**
	 * @struct	ButtonComponent
	 * @brief	クリック可能なUI要素
	 */
	struct ButtonComponent
	{
		bool enabled = true;

		// 色設定
		XMFLOAT4 normalColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		XMFLOAT4 hoverColor = { 0.8f, 0.8f, 0.8f, 1.0f };
		XMFLOAT4 pressedColor = { 0.5f, 0.5f, 0.5f, 1.0f };

		// 状態（システムが書き込む）
		bool isHovered = false;
		bool isPressed = false;
		bool isClicked = false; // 1フレームだけtrueになる

		ButtonComponent() = default;
	};
	ARCHE_COMPONENT(ButtonComponent,
		REFLECT_VAR(enabled)
		REFLECT_VAR(normalColor)
		REFLECT_VAR(hoverColor)
		REFLECT_VAR(pressedColor)
	)
}

#endif // !___UI_COMPONENTS_H___