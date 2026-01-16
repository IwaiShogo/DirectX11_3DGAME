#pragma once
#include "Engine/Scene/Components/Components.h"

namespace Arche {

	enum class BossState {
		Idle,		// 待機・移動
		Attack_A,	// パターンA (例: 砲撃)
		Attack_B,	// パターンB (例: 突進/回転)
		Attack_Ult, // 必殺技
		Stunned		// 部位破壊時の硬直
	};

	struct BossAI
	{
		std::string bossName = ""; // "Tank", "Omega" 等
		BossState state = BossState::Idle;

		float stateTimer = 0.0f;
		float attackInterval = 3.0f;

		// 攻撃用パラメータ
		int shotCount = 0;
		float phaseTimer = 0.0f;

		BossAI() = default;
	};

	ARCHE_COMPONENT(BossAI, REFLECT_VAR(bossName) REFLECT_VAR(attackInterval))
}