#pragma once
// 必須インクルード（マクロ使用のため）
#include "Engine/Scene/Components/Components.h"

namespace Arche {

	enum class EnemyType
	{
		Chaser,		// 単純追跡型
		Dasher,		// 高速突進型
		Floater,	// 浮遊型
	};

	/**
	 * @struct EnemyStats
	 * @brief  敵のデータ
	 */
	struct EnemyStats
	{
		EnemyType type = EnemyType::Chaser;
		float hp = 10.0f;
		float moveSpeed = 5.0f;
		
		float collisionPenalty = 2.0f;	// 接触時にプレイヤーから奪う秒数
		float killReward = 3.0f;		// 倒したときにプレイヤーに回復する秒数

		EnemyStats() = default;
	};

	ARCHE_COMPONENT(EnemyStats,
		REFLECT_VAR(type)
		REFLECT_VAR(hp)
		REFLECT_VAR(moveSpeed)
		REFLECT_VAR(collisionPenalty)
		REFLECT_VAR(killReward)
	)
}

#include "Editor/Core/InspectorGui.h"
ARCHE_REGISTER_ENUM_NAMES(Arche::EnemyType, "Chaser", "Dasher", "Floater")