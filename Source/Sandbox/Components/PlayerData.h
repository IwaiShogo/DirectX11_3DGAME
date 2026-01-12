#pragma once
// 必須インクルード（マクロ使用のため）
#include "Engine/pch.h"
#include "Engine/Scene/Components/Components.h"

namespace Arche
{
	/**
	 * @struct	LifeTimer
	 * @brief	寿命管理
	 */
	struct LifeTimer
	{
		float currentInfo = 5.0f;	// 残り時間
		float maxTime = 5.0f;		// 最大時間
		bool isDead = false;		// 死亡フラグ

		LifeTimer(float time = 5.0f) : currentInfo(time), maxTime(time) {}
	};
	ARCHE_COMPONENT(LifeTimer,
		REFLECT_VAR(currentInfo)
		REFLECT_VAR(maxTime)
		REFLECT_VAR(isDead)
	);

	/**
	 * @enum	PlayerState
	 * @brief	プレイヤーのアクション状態
	 */
	enum class PlayerState
	{
		Idle,
		Run,
		Dash,	// ダッシュ斬り中
		Spin,	// 回転斬り中
		Drift,	// ドリフト衝撃波発生中
	};

	struct PlayerController
	{
		PlayerState state = PlayerState::Idle;
		float actionTimer = 0.0f;	// アクション継続時間計算用

		// ダッシュ・ドリフト用パラメータ
		XMFLOAT3 moveDirection = { 0, 0, 0 };
		float currentSpeed = 0.0f;
		float dashSpeed = 20.0f;
		float dashDuration = 0.2f;	// ダッシュ継続時間

		// ドリフト判定用
		bool isDrifting = false;
	};
	ARCHE_COMPONENT(PlayerController,
		REFLECT_VAR(state)
		REFLECT_VAR(currentSpeed)
		REFLECT_VAR(dashSpeed)
		REFLECT_VAR(dashDuration)
	);

}	// namespace Arche