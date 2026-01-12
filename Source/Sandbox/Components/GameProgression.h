#pragma once
// 必須インクルード（マクロ使用のため）
#include "Engine/Scene/Components/Components.h"

namespace Arche {

	/**
	 * @struct	GameProgression
	 * @brief	ゲーム進行・Wave管理
	 */
	struct GameProgression
	{
		int currentWave = 1;
		int maxWaves = 3;
		int enemyCount = 0;	// 現在存在している敵の数
		bool isStageClear = false;
	};
	ARCHE_COMPONENT(GameProgression, REFLECT_VAR(currentWave) REFLECT_VAR(maxWaves));
}
