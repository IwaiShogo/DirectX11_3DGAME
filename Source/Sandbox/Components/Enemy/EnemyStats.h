#pragma once
#include "Engine/Scene/Components/Components.h"

namespace Arche {

	// 敵の種類
	enum class EnemyType
	{
		// 雑魚
		Zako_Cube,
		Zako_Speed,
		Zako_Armored,

		// 中ボス
		Boss_Tank,
		Boss_Prism,
		Boss_Carrier,
		Boss_Construct,

		// 大ボス
		Boss_Omega
	};

	/**
	 * @struct EnemyStats
	 * @brief  敵のパラメータ（本体・部位共通）
	 */
	struct EnemyStats
	{
		EnemyType type = EnemyType::Zako_Cube;

		float hp = 10.0f;
		float maxHp = 10.0f;
		float speed = 5.0f;

		// 接触ダメージ / 撃破報酬
		float collisionPenalty = 2.0f;
		float killReward = 3.0f;

		// スコア
		int scoreValue = 100;

		bool isInitialized = false;

		EnemyStats() = default;
	};

	ARCHE_COMPONENT(EnemyStats,
		REFLECT_VAR(type)
		REFLECT_VAR(hp)
		REFLECT_VAR(maxHp)
		REFLECT_VAR(speed)
		REFLECT_VAR(collisionPenalty)
		REFLECT_VAR(killReward)
		REFLECT_VAR(scoreValue)
	)
}

#include "Editor/Core/InspectorGui.h"
ARCHE_REGISTER_ENUM_NAMES(Arche::EnemyType,
	"Zako_Cube", "Zako_Speed", "Zako_Armored",
	"Boss_Tank", "Boss_Prism", "Boss_Carrier", "Boss_Construct",
	"Boss_Omega"
)