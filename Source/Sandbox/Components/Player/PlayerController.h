#pragma once
// 必須インクルード（マクロ使用のため）
#include "Engine/Scene/Components/Components.h"

namespace Arche {

	enum class PlayerState
	{
		Idle,
		Run,
		Dash,	// 高速移動
		Drift,	// 急旋回衝撃波
		Attack,	// 射撃など
	};

	/**
	 * @struct	PlayerController
	 * @brief	プレイヤーの操作パラメータと状態
	 */
	struct PlayerController
	{
		PlayerState state = PlayerState::Idle;

		// 移動パラメータ
		float dashSpeed = 25.0f;
		float dashDuration = 0.25f;

		// 制御用
		float actionTimer = 0.0f;
		XMFLOAT3 moveDirection = { 0, 0, 1 };	// 慣性方向
		bool isDrifting = false;

		PlayerController() = default;
	};

	ARCHE_COMPONENT(PlayerController,
		REFLECT_VAR(state)
		REFLECT_VAR(dashSpeed)
		REFLECT_VAR(moveDirection)
	)
}

#include "Editor/Core/InspectorGui.h"
ARCHE_REGISTER_ENUM_NAMES(Arche::PlayerState, "Idle", "Run", "Dash", "Drift", "Attack")