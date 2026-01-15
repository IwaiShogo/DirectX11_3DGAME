#pragma once
// 必須インクルード（マクロ使用のため）
#include "Engine/Scene/Components/Components.h"

namespace Arche {

	/**
	 * @struct GameManager
	 * @brief  スコアやゲームの状態を管理する
	 */
	struct GameManager
	{
		int score = 0;
		bool isGameOver = false;

		// UI更新用フラグ
		bool isDirty = true;

		GameManager() = default;
	};

	ARCHE_COMPONENT(GameManager,
		REFLECT_VAR(score)
		REFLECT_VAR(isGameOver)
	)
}
