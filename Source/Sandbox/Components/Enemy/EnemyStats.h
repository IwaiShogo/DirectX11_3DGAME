#pragma once
#include "Engine/Scene/Components/Components.h"

namespace Arche {

	// 敵の種類
	enum class EnemyType
	{
		// --- 雑魚 ---
		Zako_Cube,		// 基本：追ってくるだけ
		Zako_Speed,		// 高速：突進してくる
		Zako_Armored,	// 装甲：HP高い、遅い
		Zako_Sniper,	// 遠距離：逃げながら撃つ ★追加
		Zako_Kamikaze,	// 自爆：近づいて爆発 ★追加
		Zako_Shield,	// 盾持ち：正面無敵 ★追加

		// --- 中ボス ---
		Boss_Tank,		// 戦車型
		Boss_Prism,		// 浮遊要塞
		Boss_Carrier,	// 空母型
		Boss_Construct, // 触手型

		// --- 大ボス ---
		Boss_Omega,		// ラスボス（旧）
		Boss_Titan		// 巨大ロボット（新ラスボス） ★追加
	};

	struct EnemyStats
	{
		EnemyType type = EnemyType::Zako_Cube;

		float hp = 10.0f;
		float maxHp = 10.0f;
		float speed = 5.0f;

		float collisionPenalty = 2.0f;
		float killReward = 3.0f;

		int scoreValue = 100;
		float attackTimer = 0.0f;

		// 自動回復用
		float regenTimer = 0.0f;

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
	"Zako_Cube", "Zako_Speed", "Zako_Armored", "Zako_Sniper", "Zako_Kamikaze", "Zako_Shield",
	"Boss_Tank", "Boss_Prism", "Boss_Carrier", "Boss_Construct",
	"Boss_Omega", "Boss_Titan"
)