#pragma once
// 必須インクルード（マクロ使用のため）
#include "Engine/Scene/Components/Components.h"

namespace Arche {

	/**
	 * @struct PlayerTime
	 * @brief  ユーザー定義データ
	 */
	struct PlayerTime
	{
		float currentTime = 5.0f;	// 現在の残り秒数
		float maxTime = 5.0f;		// 最大時間

		// 状態フラグ
		float invincibilityTimer = 0.0f;	// ダメージ後の無敵時間
		bool isDead = false;

		PlayerTime() = default;
	};

	ARCHE_COMPONENT(PlayerTime,
		REFLECT_VAR(currentTime)
		REFLECT_VAR(maxTime)
		REFLECT_VAR(invincibilityTimer)
		REFLECT_VAR(isDead)
	)
}
