#pragma once
#include "Engine/Scene/Components/Components.h"

namespace Arche {

	enum class BossState {
		Idle,		// 待機・移動
		Attack_A,	// パターンA
		Attack_B,	// パターンB
		Attack_Ult, // 必殺技
		Stunned		// 硬直
	};

	struct BossAI
	{
		std::string bossName = "";
		BossState state = BossState::Idle;

		float stateTimer = 0.0f;
		float attackInterval = 3.0f;

		// 攻撃用パラメータ
		int shotCount = 0;
		float phaseTimer = 0.0f;

		// ★追加: 複雑なパターンのための制御変数
		int patternStep = 0;      // 攻撃内の段階 (例: 溜め->発射->後隙)
		float secondaryTimer = 0.0f; // サブタイマー

		BossAI() = default;
	};

	ARCHE_COMPONENT(BossAI, REFLECT_VAR(bossName) REFLECT_VAR(attackInterval))
}