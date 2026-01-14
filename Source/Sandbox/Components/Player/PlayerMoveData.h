#pragma once
// 必須インクルード（マクロ使用のため）
#include "Engine/Scene/Components/Components.h"

namespace Arche {

	/**
	 * @struct PlayerMoveData
	 * @brief  プレイヤーの移動に関するデータ
	 */
	struct PlayerMoveData
	{
		float walkSpeed = 5.0f;
		float runSpeed = 10.0f;
		float jumpPower = 6.0f;
		float rotationSpeed = 15.0f;	// 振り向き速度

		PlayerMoveData() = default;
	};

	ARCHE_COMPONENT(PlayerMoveData,
		REFLECT_VAR(walkSpeed)
		REFLECT_VAR(runSpeed)
		REFLECT_VAR(jumpPower)
		REFLECT_VAR(rotationSpeed)
	)
}
