#pragma once
// 必須インクルード（マクロ使用のため）
#include "Engine/Scene/Components/Components.h"

namespace Arche {

	/**
	 * @struct	LifeTimer
	 * @brief	「5秒ルール」を管理するデータ
	 */
	struct LifeTimer
	{
		float currentTime = 5.0f;
		float maxTime = 5.0f;
		bool isDead = false;

		// ヒットストップ中でも減少し続けるか
		bool ignoreTimeScale = false;

		LifeTimer(float time = 5.0f) : currentTime(time), maxTime(time) {}
	};

	ARCHE_COMPONENT(LifeTimer,
		REFLECT_VAR(currentTime)
		REFLECT_VAR(maxTime)
		REFLECT_VAR(isDead)
	)
}
