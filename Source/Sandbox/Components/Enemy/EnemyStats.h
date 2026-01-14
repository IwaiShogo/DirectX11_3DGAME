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
		float recoverAmount = 1.0f;	// 倒した時の回復量
		float damage = 1.0f;		// プレイヤーへの接触ダメージ
	};

	ARCHE_COMPONENT(EnemyStats,
		REFLECT_VAR(type)
		REFLECT_VAR(hp)
		REFLECT_VAR(moveSpeed)
		REFLECT_VAR(recoverAmount)
	)
}

#include "Editor/Core/InspectorGui.h"
ARCHE_REGISTER_ENUM_NAMES(Arche::EnemyType, "Chaser", "Dasher", "Floater")