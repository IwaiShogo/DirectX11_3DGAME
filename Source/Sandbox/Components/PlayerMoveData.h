#pragma once
// 必須インクルード（マクロ使用のため）
#include "Engine/Scene/Components/Components.h"

namespace Arche {

	/**
	 * @struct PlayerMoveData
	 * @brief  プレイヤーの移動パラメータ
	 */
	struct PlayerMoveData
	{
		// 変数はここに定義
		float speed = 5.0f;
	};

	ARCHE_COMPONENT(PlayerMoveData,
		REFLECT_VAR(speed) // ここに変数を追加していく
	)
}
