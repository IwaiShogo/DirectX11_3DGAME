#pragma once
// 必須インクルード（マクロ使用のため）
#include "Engine/Scene/Components/Components.h"

namespace Arche {

	/**
	 * @struct FloatingTextAnimation
	 * @brief  テキストを上昇させながらフェードアウトさせる演出用
	 */
	struct FloatingTextAnimation
	{
		float moveSpeedY = 2.0f;	// 上昇速度
		float fadeSpeed = 1.0f;		// 透明になる速さ

		FloatingTextAnimation() = default;
	};

	ARCHE_COMPONENT(FloatingTextAnimation,
		REFLECT_VAR(moveSpeedY)
		REFLECT_VAR(fadeSpeed)
	)
}
